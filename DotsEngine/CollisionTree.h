#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <vector>
#include <algorithm>

#include "tracy/Tracy.hpp"
#include "ThreadPool.h"
#include "Utilities.h"

using uint8_t = unsigned char;
using Color = glm::vec3;
using CollisionPairs = std::vector<std::pair<int, int>>;

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

		int childStart = -1; // index of first child, in DFS order, after compaction
		uint8_t childMask = 0;
		int skipCount = 1; // number of array slots this cell's subtree occupies (itself included)

		bool IsLeaf() const { return childStart == -1; }
		bool HasChild(uint8_t octant) const { return childMask& (1 << octant); }
	};

	static constexpr size_t MaxCellCount(uint32_t bytes = (25 * 1024 * 1024)) { return bytes / sizeof(Cell); }
	static constexpr size_t EstimateCellCount(int colliderCount, int splitThreshold) { return std::max(64, (colliderCount / std::max(1, (int)splitThreshold)) * 4); }
	static constexpr uint8_t GetChildSlot(const glm::vec3& center, const glm::vec3& pos) { return (pos.x > center.x) | ((pos.y > center.y) << 1) | ((pos.z > center.z) << 2); }

	const std::vector<Cell>& Cells() const { return cells; }

	size_t TotalBytes()
	{
		return sizeof(CollisionTree) +
			(sizeof(int) * indexPool.size()) +
			(sizeof(Cell) * cells.size());
	}

	void Build(const glm::vec3* positions, const float* radii, int colliderCount,
		float rootHalfExtent, uint8_t splitThreshold, uint8_t maxDepth = 6, float looseMargin = 0, glm::vec3 rootCenter = {})
	{
		CollisionTree::positions = positions;
		CollisionTree::radii = radii;
		CollisionTree::colliderCount = colliderCount;
		CollisionTree::splitThreshold = splitThreshold;
		CollisionTree::maxDepth = maxDepth;
		CollisionTree::looseMargin = looseMargin;

		std::vector<Cell> ordered_scratch(EstimateCellCount(colliderCount, splitThreshold));
		std::vector<uint8_t> octants_scratch(colliderCount);
		std::vector<int> placed_scratch(colliderCount);

		EnsureIndexPool(colliderCount);

		cells.clear();
		cells.reserve(EstimateCellCount(colliderCount, splitThreshold));

		// root cell
		cells.push_back(Cell
			{
				rootCenter,
				rootHalfExtent,
				0,
				colliderCount,
				colliderCount,
			});

		{
			ZoneScopedNC("Build Recursive", HexColors::Brown);
			BuildCellRecursive(0, 0, octants_scratch, placed_scratch);
		}

		{
			ZoneScopedNC("Compact To DFS", HexColors::Brown);
			CompactToDFSOrder(ordered_scratch);
		}
	}

	void QueryPairs(CollisionPairs& outPairs) const
	{
		ZoneScopedNC("Query", HexColors::Brown);

		outPairs.clear();

		for (const Cell& cell : cells)
		{
			if (!cell.IsLeaf()) continue;

			for (int a = 0; a < cell.colliderCount; a++)
				for (int b = a + 1; b < cell.colliderCount; b++)
					outPairs.emplace_back(indexPool[cell.colliderStart + a], indexPool[cell.colliderStart + b]);
		}
	}

	template<uint8_t ThreadCount>
	void QueryPairs_Parallel(std::vector<std::pair<int, int>>& outPairs) const
	{
		std::array<std::vector<std::pair<int, int>>, ThreadCount> threadBuffers;

		StaticThreadPool().For_Parallel<ThreadCount>(cells.size(),
			[this, &threadBuffers](uint32_t thread, uint32_t chunkStart, size_t cell)
			{
				const Cell& c = cells[cell];
				if (!c.IsLeaf()) return;

				auto& buffer = threadBuffers[thread];

				for (int a = 0; a < c.colliderCount; a++)
					for (int b = a + 1; b < c.colliderCount; b++)
						buffer.emplace_back(indexPool[c.colliderStart + a], indexPool[c.colliderStart + b]);
			});

		size_t total = 0;
		for (auto& buffer : threadBuffers) total += buffer.size();
		outPairs.reserve(total);
		for (auto& buffer : threadBuffers)
			outPairs.insert(outPairs.end(), buffer.begin(), buffer.end());
	}

private:
	const glm::vec3* positions = nullptr;
	const float* radii = nullptr;
	int colliderCount = 0;

	std::vector<int> indexPool;
	std::vector<Cell> cells;
	uint8_t maxDepth = 6;
	uint8_t splitThreshold = 0;
	float looseMargin = 0;

private:
	void EnsureIndexPool(int colliderCount)
	{
		if ((int)indexPool.size() != colliderCount)
		{
			indexPool.resize(colliderCount);
			for (int i = 0; i < colliderCount; ++i)
				indexPool[i] = i;
		}
	}

	void BuildCellRecursive(int cellIndex, uint8_t depth,
		std::vector<uint8_t>& octants_scratch, std::vector<int>& placed_scratch)
	{
		if (depth >= maxDepth || cells[cellIndex].colliderCount < splitThreshold || cells.size() >= MaxCellCount())
			return;

		int firstChild = SplitCell(cellIndex, octants_scratch, placed_scratch);

		for (int i = 0; i < 8; i++)
			if (cells[firstChild + i].colliderCount > 0)
				BuildCellRecursive(firstChild + i, depth + 1, octants_scratch, placed_scratch);
	}

	// splits one cell into up to 8 children. returns the index of the first child.
	int SplitCell(int cellIndex,
		std::vector<uint8_t>& octants_scratch, std::vector<int>& placed_scratch)
	{
		Cell cellCopy = cells[cellIndex]; // copy: recursive pushes below may reallocate

		std::array<int, 8> counts{}, starts{};
		BucketSortChildren(cellCopy.colliderStart, cellCopy.colliderCount,
			cellCopy.center, counts, starts, octants_scratch, placed_scratch);

		int firstChild = (int)cells.size();
		cells.resize(cells.size() + 8);

		float childExtent = cellCopy.halfExtent * 0.5f;

		for (int i = 0; i < 8; i++)
		{
			glm::vec3 offset{
				(i & 1) ? childExtent : -childExtent,
				(i & 2) ? childExtent : -childExtent,
				(i & 4) ? childExtent : -childExtent
			};

			Cell child
			{
				cellCopy.center + offset,
				childExtent + looseMargin,
				cellCopy.colliderStart + starts[i],
				counts[i],
				counts[i]
			};

			cells[firstChild + i] = child;
		}

		Cell& cell = cells[cellIndex];
		cell.childStart = firstChild;
		cell.colliderCount = 0;

		for (int i = 0; i < 8; i++)
			if (counts[i] > 0)
				cell.childMask |= (1 << i);

		return firstChild;
	}

	void BucketSortChildren(int start, int count, const glm::vec3& center,
		std::array<int, 8>& outCounts, std::array<int, 8>& outStarts,
		std::vector<uint8_t>& octants_scratch, std::vector<int>& placed_scratch)
	{
		int* base = indexPool.data() + start;

		outCounts.fill(0);
		for (int i = 0; i < count; i++)
			outCounts[octants_scratch[i] = GetChildSlot(center, positions[base[i]])]++;

		int offset = 0;
		for (int i = 0; i < 8; i++)
		{
			outStarts[i] = offset;
			offset += outCounts[i];
		}

		std::array<int, 8> cursor = outStarts;
		for (int i = 0; i < count; i++)
			placed_scratch[cursor[octants_scratch[i]]++] = base[i];

		std::copy(placed_scratch.begin(), placed_scratch.begin() + count, base);
	}

	// Rebuilds m_cells in depth-first pre-order and fills in skipCount, so a
	// flat scan can either walk every cell (i++) or jump over a whole subtree
	// (i += skipCount) without recursion.
	void CompactToDFSOrder(std::vector<Cell>& ordered_scratch)
	{
		ordered_scratch.clear();
		ordered_scratch.reserve(cells.size());

		std::function<int(int)> visit =
			[&](int oldIndex) -> int
			{
				int newIndex = (int)ordered_scratch.size();
				ordered_scratch.push_back(cells[oldIndex]);

				const Cell oldCell = cells[oldIndex]; // copy: recursive pushes below may reallocate
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

					ordered_scratch[newIndex].childStart = newChildStart;
				}

				ordered_scratch[newIndex].skipCount = (int)ordered_scratch.size() - newIndex;
				return newIndex;
			};

		visit(0);
		cells = std::move(ordered_scratch);
	}
};