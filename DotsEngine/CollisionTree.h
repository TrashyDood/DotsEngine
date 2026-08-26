#pragma once

#include <vector>
#include <atomic>
#include <math.h>
#include <algorithm>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>

#include "DebugLines.h"
#include "tracy/Tracy.hpp"
#include "ThreadPool.h"

using uint8_t = unsigned char;
using Color = glm::vec3;

namespace Utilities
{
	inline glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }
}

template<uint8_t MaxDepth>
class CollisionTreeSOA
{
public:
	struct Cell
	{
		glm::vec3 center{};
		float halfExtent = 0;

		int colliderStart = -1; // index into the indexPool
		int colliderCount = 0;  // number of colliders in this cell
		int totalColliderCount = 0; // total colliders under this cell/subtree

		int childStart = -1; // index of first child in cellPool
		uint8_t childMask = 0; // bitmask of which children exist (0-7)

		constexpr bool HasChild(const int slot) const { return childMask & (1 << slot); }
		constexpr bool IsLeaf() const { return childStart == -1; }

		constexpr int GetChildSlot(const glm::vec3& position) const
		{
			return (position.x > center.x) |
				((position.y > center.y) << 1) |
				((position.z > center.z) << 2);
		}

		bool HasChildIndex(uint32_t index) { return childMask & (1 << index); }
	};

	static constexpr int CellPoolSize()
	{
		int total = 0;
		int levelCount = 1;

		for (int i = 0; i <= MaxDepth; i++)
		{
			total += levelCount;
			levelCount *= 8;
		}

		return total;
	}

	static constexpr uint8_t maxDepth = MaxDepth;

//private:
	const glm::vec3* positions = nullptr;
	const float* radii = nullptr;
	int colliderCount = 0;

	std::unique_ptr<int[]> indexPool;
	int indexPoolSize = 0;

	Cell cellPool[CellPoolSize()];
	std::atomic<int> cellCount = 1;

	uint8_t splitThreshhold = 0;
	float looseMargin = 0;

public:
	void StartBuild(const glm::vec3* positions, const float* radii, uint32_t colliderCount,
		float rootHalfExtent, uint8_t splitThreshhold, float looseMargin = 0,
		glm::vec3 rootCenter = glm::vec3())
	{
		CollisionTreeSOA::positions = positions;
		CollisionTreeSOA::radii = radii;
		CollisionTreeSOA::colliderCount = (int)colliderCount;
		CollisionTreeSOA::splitThreshhold = splitThreshhold;
		CollisionTreeSOA::looseMargin = looseMargin;

		if (indexPoolSize != (int)colliderCount)
		{
			indexPool = std::make_unique<int[]>(colliderCount);
			indexPoolSize = (int)colliderCount;
			for (int i = 0; i < (int)colliderCount; ++i)
				indexPool[i] = i;
		}

		cellCount = 1;
		cellPool[0] = Cell{ rootCenter, rootHalfExtent, 0, (int)colliderCount };
	}

	void BuildCells(int cellIndex = 0, uint8_t depth = 0)
	{
		Cell& cell = cellPool[cellIndex];

		if (depth >= MaxDepth || cell.colliderCount < splitThreshhold)
			return;

		int firstChild = SplitCell(cellIndex);

		for (int i = 0; i < 8; i++)
			if (cellPool[firstChild + i].colliderCount > 0)
				BuildCells(firstChild + i, depth + 1);
	}

	void BuildCells_Parallel(int cellIndex, uint8_t depth, int parallelThreshold, std::vector<std::future<void>>& tasks)
	{
		Cell& cell = cellPool[cellIndex];

		if (depth >= MaxDepth || cell.colliderCount < splitThreshhold)
			return;

		int firstChild = SplitCell(cellIndex);

		for (int i = 0; i < 8; i++)
		{
			if (!(cellPool[cellIndex].HasChild(i))) continue;

			int childIndex = firstChild + i;
			if (cellPool[childIndex].colliderCount >= parallelThreshold)
			{
				tasks.push_back(StaticThreadPool().Submit(
					[this, childIndex, depth, parallelThreshold]()
					{
						std::vector<std::future<void>> subTasks;
						BuildCells_Parallel(childIndex, depth + 1, parallelThreshold, subTasks);
						for (auto& t : subTasks) t.get();
					}));
			}
			else
				BuildCells(childIndex, depth + 1);
		}
	}

	void QueryCell(std::vector<std::pair<int, int>>& out, int cellIndex = 0)
	{
		Cell& cell = cellPool[cellIndex];

		if (cell.childStart == -1)
		{
			for (int i = 0; i < cell.colliderCount; i++)
				for (int j = i + 1; j < cell.colliderCount; j++)
					out.emplace_back(indexPool[cell.colliderStart + i], indexPool[cell.colliderStart + j]);
			return;
		}

		for (int i = 0; i < 8; i++)
			if (cell.childMask & (1 << i))
				QueryCell(out, cell.childStart + i);

		if (looseMargin <= 0) // only do cross test if loose margin is enabled
			return;

		for (int i = 0; i < 8; i++) // cross test with overlapping cells
		{
			if (!(cell.childMask & (1 << i))) continue; // if this child cell isn present
			for (int j = i + 1; j < 8; j++)
			{
				if (!(cell.childMask & (1 << j))) continue;
				CrossTest(cell.childStart + i, cell.childStart + j, out);
			}
		}
	}

	void QueryCellParallel(std::vector<std::pair<int, int>>& out, int cellIndex, int parallelThreshold,
		std::vector<std::future<std::vector<std::pair<int, int>>>>& tasks)
	{
		Cell& cell = cellPool[cellIndex];

		if (cell.childStart == -1)
		{
			// leaf — just do it inline, no point spawning a task for one cell
			for (int i = 0; i < cell.colliderCount; i++)
				for (int j = i + 1; j < cell.colliderCount; j++)
					out.emplace_back(indexPool[cell.colliderStart + i], indexPool[cell.colliderStart + j]);
			return;
		}

		for (int i = 0; i < 8; i++)
		{
			if (!(cell.childMask & (1 << i))) continue;

			int childIndex = cell.childStart + i;
			Cell& child = cellPool[childIndex];

			// only worth spawning a task if this subtree has enough colliders under it
			if (cellPool[childIndex].totalColliderCount >= parallelThreshold)
			{
				tasks.push_back(StaticThreadPool().Submit([this, childIndex, parallelThreshold]()
					{
						std::vector<std::pair<int, int>> localOut;
						std::vector<std::future<std::vector<std::pair<int, int>>>> subTasks;
						QueryCellParallel(localOut, childIndex, parallelThreshold, subTasks);
						for (auto& t : subTasks)
						{
							auto sub = t.get();
							localOut.insert(localOut.end(), sub.begin(), sub.end());
						}
						return localOut;
					}));
			}
			else
			{
				QueryCell(out, childIndex); // small enough, finish serially inline (existing function)
			}
		}

		if (looseMargin <= 0)
			return;

		for (int i = 0; i < 8; i++)
			if (cell.childMask & (1 << i))
				for (int j = i + 1; j < 8; j++)
					if (cell.childMask & (1 << j))
						CrossTest(cell.childStart + i, cell.childStart + j, out); // still serial — see note below
	}

	void GetRootChildren(int(&outChildIndices)[8], int& outCount)
	{
		Cell& root = cellPool[0];
		outCount = 0;
		for (int i = 0; i < 8; i++)
			if (root.childMask & (1 << i))
				outChildIndices[outCount++] = root.childStart + i;
	}

	void DrawCell(int cellIndex = 0) const
	{
		const Cell& cell = cellPool[cellIndex];

		DebugLines::DrawCube(cell.center, glm::quat(), glm::vec3(cell.halfExtent),
			Utilities::Lerp(Color(1, 1, 1), Color(1, 0, 0), cell.colliderCount / (float)splitThreshhold));

		if (cell.childStart == -1) return;

		for (int i = 0; i < 8; i++)
			if (cell.childMask & (1 << i))
				DrawCell(cell.childStart + i);
	}

private:
	//returns index of first child, or -1 if not split.
	int SplitCell(int cellIndex)
	{
		Cell& cell = cellPool[cellIndex];

		int start = cell.colliderStart;
		int count = cell.colliderCount;
		int* begin = indexPool.get() + start;

		auto xMid = std::partition(begin, begin + count,
			[&](int idx) { return positions[idx].x <= cell.center.x; });

		auto yMid1 = std::partition(begin, xMid,
			[&](int idx) { return positions[idx].y <= cell.center.y; });
		auto yMid2 = std::partition(xMid, begin + count,
			[&](int idx) { return positions[idx].y <= cell.center.y; });

		auto zMid1 = std::partition(begin, yMid1,
			[&](int idx) { return positions[idx].z <= cell.center.z; });
		auto zMid2 = std::partition(yMid1, xMid,
			[&](int idx) { return positions[idx].z <= cell.center.z; });
		auto zMid3 = std::partition(xMid, yMid2,
			[&](int idx) { return positions[idx].z <= cell.center.z; });
		auto zMid4 = std::partition(yMid2, begin + count,
			[&](int idx) { return positions[idx].z <= cell.center.z; });

		int* rangeBegin[8];
		int* rangeEnd[8];
		rangeBegin[0] = begin;    rangeEnd[0] = zMid1;
		rangeBegin[4] = zMid1;    rangeEnd[4] = yMid1;
		rangeBegin[2] = yMid1;    rangeEnd[2] = zMid2;
		rangeBegin[6] = zMid2;    rangeEnd[6] = xMid;
		rangeBegin[1] = xMid;     rangeEnd[1] = zMid3;
		rangeBegin[5] = zMid3;    rangeEnd[5] = yMid2;
		rangeBegin[3] = yMid2;    rangeEnd[3] = zMid4;
		rangeBegin[7] = zMid4;    rangeEnd[7] = begin + count;

		int firstChild = cellCount.fetch_add(8);
		float childExtent = cell.halfExtent * 0.5f;

		for (int i = 0; i < 8; i++)
		{
			int cnt = (int)(rangeEnd[i] - rangeBegin[i]);
			int startOffset = (int)(rangeBegin[i] - indexPool.get());

			glm::vec3 offset{
				(i & 1) ? childExtent : -childExtent,
				(i & 2) ? childExtent : -childExtent,
				(i & 4) ? childExtent : -childExtent
			};

			cellPool[firstChild + i] = Cell{
				cell.center + offset,
				childExtent + looseMargin,
				startOffset,
				cnt,
				cnt
			};

			if (cnt > 0)
				cell.childMask |= (1 << i);
		}

		cell.childStart = firstChild;
		cell.colliderCount = 0;

		return firstChild;
	}

	void CrossTest(int cellIndexA, int cellIndexB, std::vector<std::pair<int, int>>& out)
	{
		Cell& a = cellPool[cellIndexA];
		Cell& b = cellPool[cellIndexB];

		if (fabs(a.center.x - b.center.x) > a.halfExtent + b.halfExtent) return;
		if (fabs(a.center.y - b.center.y) > a.halfExtent + b.halfExtent) return;
		if (fabs(a.center.z - b.center.z) > a.halfExtent + b.halfExtent) return;

		bool aLeaf = a.childStart == -1;
		bool bLeaf = b.childStart == -1;

		if (aLeaf && bLeaf)
		{
			for (int i = 0; i < a.colliderCount; i++)
				for (int j = 0; j < b.colliderCount; j++)
					out.emplace_back(indexPool[a.colliderStart + i], indexPool[b.colliderStart + j]);
			return;
		}

		if (!aLeaf)
		{
			for (int i = 0; i < 8; i++)
				if (a.childMask & (1 << i))
					CrossTest(a.childStart + i, cellIndexB, out);
			return;
		}

		for (int j = 0; j < 8; j++)
			if (b.childMask & (1 << j))
				CrossTest(cellIndexA, b.childStart + j, out);
	}
};

template<uint8_t MaxDepth>
class CollisionTree
{
public:
	struct Cell
	{
		glm::vec3 center{};
		float halfExtent = 0;

		int colliderStart = -1;
		int colliderCount = 0;
		int totalColliderCount = 0;

		int childStart = -1;   // index of first child, in DFS order, after compaction
		uint8_t childMask = 0;
		int skipCount = 1;     // number of array slots this cell's subtree occupies (itself included)

		bool IsLeaf() const { return childStart == -1; }
	};

	void Build(const glm::vec3* positions, const float* radii, int colliderCount,
		float rootHalfExtent, uint8_t splitThreshold, glm::vec3 rootCenter = {}, float looseMargin = 0)
	{
		m_positions = positions;
		m_radii = radii;
		m_colliderCount = colliderCount;
		m_splitThreshold = splitThreshold;
		m_looseMargin = looseMargin;

		EnsureIndexPool(colliderCount);

		m_cells.clear();
		m_cells.reserve(EstimateCellCount(colliderCount, splitThreshold));

		Cell root{};
		root.center = rootCenter;
		root.halfExtent = rootHalfExtent;
		root.colliderStart = 0;
		root.colliderCount = colliderCount;
		root.totalColliderCount = colliderCount;
		m_cells.push_back(root);

		BuildCellRecursive(0, 0);
		CompactToDFSOrder();
	}

	// Flat leaf-pair query. Walks the DFS-ordered array; every leaf cell
	// contributes its own collider pairs. No recursion, no pointer chasing.
	void QueryPairs(std::vector<std::pair<int, int>>& out) const
	{
		for (const Cell& cell : m_cells)
		{
			if (!cell.IsLeaf()) continue;

			for (int a = 0; a < cell.colliderCount; a++)
				for (int b = a + 1; b < cell.colliderCount; b++)
					out.emplace_back(m_indexPool[cell.colliderStart + a], m_indexPool[cell.colliderStart + b]);
		}
	}

	const std::vector<Cell>& Cells() const { return m_cells; }

private:
	const glm::vec3* m_positions = nullptr;
	const float* m_radii = nullptr;
	int m_colliderCount = 0;

	std::vector<int> m_indexPool;
	std::vector<Cell> m_cells;

	uint8_t m_splitThreshold = 0;
	float m_looseMargin = 0;

private:
	void EnsureIndexPool(int colliderCount)
	{
		if ((int)m_indexPool.size() != colliderCount)
		{
			m_indexPool.resize(colliderCount);
			for (int i = 0; i < colliderCount; ++i)
				m_indexPool[i] = i;
		}
	}

	static int EstimateCellCount(int colliderCount, int splitThreshold)
	{
		// Rough working-set guess so the vector rarely needs to grow during a build.
		int threshold = std::max(1, (int)splitThreshold);
		return std::max(64, (colliderCount / threshold) * 4);
	}

	static int GetChildSlot(const glm::vec3& center, const glm::vec3& pos)
	{
		return (pos.x > center.x) | ((pos.y > center.y) << 1) | ((pos.z > center.z) << 2);
	}

	// Classifies every index in [start, start+count) exactly once, then bucket-sorts
	// them into 8 contiguous runs in indexPool. Two passes total (classify+count, place),
	// versus the previous 7-way std::partition chain.
	void BucketSortChildren(int start, int count, const glm::vec3& center,
		std::array<int, 8>& outCounts, std::array<int, 8>& outStarts)
	{
		int* base = m_indexPool.data() + start;

		std::vector<uint8_t> slots(count);
		outCounts.fill(0);

		for (int i = 0; i < count; i++)
		{
			uint8_t slot = (uint8_t)GetChildSlot(center, m_positions[base[i]]);
			slots[i] = slot;
			outCounts[slot]++;
		}

		int offset = 0;
		for (int i = 0; i < 8; i++)
		{
			outStarts[i] = offset;
			offset += outCounts[i];
		}

		std::vector<int> placed(count);
		std::array<int, 8> cursor = outStarts;
		for (int i = 0; i < count; i++)
			placed[cursor[slots[i]]++] = base[i];

		std::copy(placed.begin(), placed.end(), base);
	}

	// Splits one cell into up to 8 children. Returns the index of the first child.
	int SplitCell(int cellIndex)
	{
		Cell cellCopy = m_cells[cellIndex]; // copy: m_cells may reallocate below

		std::array<int, 8> counts{}, starts{};
		BucketSortChildren(cellCopy.colliderStart, cellCopy.colliderCount, cellCopy.center, counts, starts);

		int firstChild = (int)m_cells.size();
		m_cells.resize(m_cells.size() + 8);

		float childExtent = cellCopy.halfExtent * 0.5f;

		for (int i = 0; i < 8; i++)
		{
			glm::vec3 offset{
				(i & 1) ? childExtent : -childExtent,
				(i & 2) ? childExtent : -childExtent,
				(i & 4) ? childExtent : -childExtent
			};

			Cell child{};
			child.center = cellCopy.center + offset;
			child.halfExtent = childExtent + m_looseMargin;
			child.colliderStart = cellCopy.colliderStart + starts[i];
			child.colliderCount = counts[i];
			child.totalColliderCount = counts[i];

			m_cells[firstChild + i] = child;
		}

		Cell& cell = m_cells[cellIndex];
		cell.childStart = firstChild;
		cell.colliderCount = 0;
		for (int i = 0; i < 8; i++)
			if (counts[i] > 0)
				cell.childMask |= (1 << i);

		return firstChild;
	}

	void BuildCellRecursive(int cellIndex, uint8_t depth)
	{
		if (depth >= MaxDepth || m_cells[cellIndex].colliderCount < m_splitThreshold)
			return;

		int firstChild = SplitCell(cellIndex);

		for (int i = 0; i < 8; i++)
			if (m_cells[firstChild + i].colliderCount > 0)
				BuildCellRecursive(firstChild + i, depth + 1);
	}

	// Rebuilds m_cells in depth-first pre-order and fills in skipCount, so a
	// flat scan can either walk every cell (i++) or jump over a whole subtree
	// (i += skipCount) without recursion.
	void CompactToDFSOrder()
	{
		std::vector<Cell> ordered;
		ordered.reserve(m_cells.size());

		std::function<int(int)> visit = [&](int oldIndex) -> int
			{
				int newIndex = (int)ordered.size();
				ordered.push_back(m_cells[oldIndex]);

				Cell oldCell = m_cells[oldIndex]; // copy: recursive pushes below may reallocate
				if (!oldCell.IsLeaf())
				{
					int newChildStart = -1;
					for (int i = 0; i < 8; i++)
					{
						if (!(oldCell.childMask & (1 << i))) continue;
						int childNewIndex = visit(oldCell.childStart + i);
						if (newChildStart == -1)
							newChildStart = childNewIndex;
					}
					ordered[newIndex].childStart = newChildStart;
				}

				ordered[newIndex].skipCount = (int)ordered.size() - newIndex;
				return newIndex;
			};

		visit(0);
		m_cells = std::move(ordered);
	}
};