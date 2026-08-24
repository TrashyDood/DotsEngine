#pragma once

#include <vector>
#include <atomic>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "DebugLines.h"
#include "tracy/Tracy.hpp"

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

private:
	// SOA storage — points at externally owned data, not copied
	const glm::vec3* positions = nullptr;
	const float* radii = nullptr;
	int colliderCount = 0;

	// index pool mapping tree slots -> indices into positions/radii
	std::vector<int> indexPool;

	Cell cellPool[CellPoolSize()];
	std::atomic<int> cellCount = 1;

	uint8_t splitThreshhold = 0;
	float looseMargin = 0;

public:
	void BuildRoot(const glm::vec3* positions, const float* radii, uint32_t colliderCount,
		float rootHalfExtent, uint8_t splitThreshhold, glm::vec3 rootCenter = glm::vec3())
	{
		CollisionTreeSOA::positions = positions;
		CollisionTreeSOA::radii = radii;
		CollisionTreeSOA::colliderCount = (int)colliderCount;
		CollisionTreeSOA::splitThreshhold = splitThreshhold;

		if ((int)indexPool.size() != colliderCount)
		{
			indexPool.resize(colliderCount);
			for (int i = 0; i < colliderCount; ++i)
				indexPool[i] = i;
		}

		cellCount = 1; // reserve index 0
		cellPool[0] = Cell{
			rootCenter,
			rootHalfExtent,
			0,
			(int)colliderCount
		};

		BuildCell(0, 0);
	}

	void BuildCell(int cellIndex, uint8_t depth)
	{
		Cell& cell = cellPool[cellIndex];

		if (depth >= MaxDepth || cell.colliderCount < splitThreshhold)
			return;

		int start = cell.colliderStart;
		int count = cell.colliderCount;
		int* begin = indexPool.data() + start;

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

		// map each partition segment to its correct slot (x=bit0, y=bit1, z=bit2)
		int* rangeBegin[8];
		int* rangeEnd[8];
		rangeBegin[0] = begin;    rangeEnd[0] = zMid1;   // x=0,y=0,z=0
		rangeBegin[4] = zMid1;    rangeEnd[4] = yMid1;   // x=0,y=0,z=1
		rangeBegin[2] = yMid1;    rangeEnd[2] = zMid2;   // x=0,y=1,z=0
		rangeBegin[6] = zMid2;    rangeEnd[6] = xMid;    // x=0,y=1,z=1
		rangeBegin[1] = xMid;     rangeEnd[1] = zMid3;   // x=1,y=0,z=0
		rangeBegin[5] = zMid3;    rangeEnd[5] = yMid2;   // x=1,y=0,z=1
		rangeBegin[3] = yMid2;    rangeEnd[3] = zMid4;   // x=1,y=1,z=0
		rangeBegin[7] = zMid4;    rangeEnd[7] = begin + count; // x=1,y=1,z=1

		int firstChild = cellCount.fetch_add(8);
		float childExtent = cell.halfExtent * 0.5f;

		for (int i = 0; i < 8; i++)
		{
			int cnt = (int)(rangeEnd[i] - rangeBegin[i]);
			int startOffset = (int)(rangeBegin[i] - indexPool.data());

			glm::vec3 offset{
				(i & 1) ? childExtent : -childExtent,
				(i & 2) ? childExtent : -childExtent,
				(i & 4) ? childExtent : -childExtent
			};

			cellPool[firstChild + i] = Cell{
				cell.center + offset,
				childExtent + looseMargin,
				startOffset,
				cnt
			};

			if (cnt > 0)
				cell.childMask |= (1 << i);
		}

		cell.childStart = firstChild;
		cell.colliderCount = 0;

		for (int i = 0; i < 8; i++)
			if (cellPool[firstChild + i].colliderCount > 0)
				BuildCell(firstChild + i, depth + 1);
	}

	void QueryCell(std::vector<std::pair<int, int>>& out, int cellIndex = 0, float looseMargin = 0)
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

	// test overlapping cells with loose boundaries
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

struct Collider
{
	glm::vec3 position{};
	float radius;

	Collider(glm::vec3 position = {}, float radius = 0) :
		position(position), radius(radius) {
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

		int colliderStart = -1; // the index of our first collider in the collider pool
		int colliderCount = 0; // number of colliders in this cell

		int childStart = -1; // the index of our first child cell in the cell pool
		uint8_t childMask; // bitmask of which children exist (0-7)

		constexpr bool HasChild(const int slot) const { return childMask & (1 << slot); }
		constexpr bool IsLeaf() const { return childStart == -1; }

		constexpr int GetChildSlot(const glm::vec3& position) const
		{
			return (position.x > center.x) | // 0-1 +
				((position.y > center.y) << 1) | // 0-2 +
				((position.z > center.z) << 2); // 0-4
		}
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

	// expose these constants for external use
	static constexpr uint8_t maxDepth = MaxDepth;

private:
	Collider** colliders = nullptr;
	int colliderCount = 0;

	Cell cellPool[CellPoolSize()];
	std::atomic<int> cellCount = 1;

	uint8_t splitThreshhold = 0;
	float looseMargin = 0;

	std::vector<Collider*> buildScratch;

public:
	void BuildRoot(Collider** colliders, uint32_t colliderCount, float rootHalfExtent,
		size_t splitThreshhold, float looseMargin = 0, glm::vec3 rootCenter = glm::vec3())
	{
		CollisionTree<MaxDepth>::colliders = colliders;
		CollisionTree<MaxDepth>::colliderCount = colliderCount;
		CollisionTree<MaxDepth>::splitThreshhold = splitThreshhold;
		CollisionTree<MaxDepth>::looseMargin = looseMargin;

		cellCount = 1; // index 0 is the root, already occupied
		cellPool[0] = Cell{
			rootCenter,
			rootHalfExtent,
			0,
			(int)colliderCount
		};
	}

	void QueryCell(std::vector<std::pair<Collider*, Collider*>>& out, int cellIndex = 0)
	{
		Cell& cell = cellPool[cellIndex];

		if (cell.childStart == -1)
		{
			for (int i = 0; i < cell.colliderCount; i++)
				for (int j = i + 1; j < cell.colliderCount; j++)
					out.emplace_back(colliders[cell.colliderStart + i], colliders[cell.colliderStart + j]);
			return;
		}

		for (int i = 0; i < 8; i++)
			if (cell.childMask & (1 << i))
				QueryCell(out, cell.childStart + i);

		if (looseMargin <= 0) // only do cross test if loose margin is enabled
			return;

		for (int i = 0; i < 8; i++) // cross test with overlapping cells
		{
			if (!(cell.childMask & (1 << i))) continue;
			for (int j = i + 1; j < 8; j++)
			{
				if (!(cell.childMask & (1 << j))) continue;
				CrossTest(cell.childStart + i, cell.childStart + j, out);
			}
		}
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

	void BuildCell(int cellIndex, uint8_t depth)
	{
		Cell& cell = cellPool[cellIndex];

		if (depth >= MaxDepth || cell.colliderCount < splitThreshhold)
			return;

		int start = cell.colliderStart;
		int count = cell.colliderCount;

		int counts[8] = {};
		for (int i = 0; i < count; i++)
		{
			Collider* c = colliders[start + i];
			int slot = (c->position.x > cell.center.x) |
				((c->position.y > cell.center.y) << 1) |
				((c->position.z > cell.center.z) << 2);
			counts[slot]++;
		}

		int offsets[9] = { 0 };
		for (int i = 0; i < 8; i++)
			offsets[i + 1] = offsets[i] + counts[i];


		buildScratch.resize(count);

		int cursor[8];
		for (int i = 0; i < 8; i++)
			cursor[i] = offsets[i];

		for (int i = 0; i < count; i++)
		{
			Collider* collider = colliders[start + i];
			int slot = (collider->position.x > cell.center.x) |
				((collider->position.y > cell.center.y) << 1) |
				((collider->position.z > cell.center.z) << 2);

			buildScratch[cursor[slot]] = collider;
			cursor[slot]++;
		}

		for (int i = 0; i < count; i++)
			colliders[start + i] = buildScratch[i];


		int firstChild = cellCount.fetch_add(8);

		float childExtent = cell.halfExtent * 0.5f;

		for (int i = 0; i < 8; i++)
		{
			glm::vec3 offset{
				(i & 1) ? childExtent : -childExtent,
				(i & 2) ? childExtent : -childExtent,
				(i & 4) ? childExtent : -childExtent
			};

			cellPool[firstChild + i] = Cell{
				cell.center + offset,
				childExtent + looseMargin,
				start + offsets[i],
				counts[i]
			};

			if (counts[i] > 0)
				cell.childMask |= (1 << i); // parent records that child i exists
		}

		cell.childStart = firstChild;
		cell.colliderCount = 0;

		for (int i = 0; i < 8; i++)
			if (counts[i] > 0)
				BuildCell(firstChild + i, depth + 1);
	}

	// test overlapping cells with loose boundaries
	void CrossTest(int cellIndexA, int cellIndexB, std::vector<std::pair<Collider*, Collider*>>& out)
	{
		Cell& a = cellPool[cellIndexA];
		Cell& b = cellPool[cellIndexB];

		// bounds overlap check
		if (fabs(a.center.x - b.center.x) > a.halfExtent + b.halfExtent) return;
		if (fabs(a.center.y - b.center.y) > a.halfExtent + b.halfExtent) return;
		if (fabs(a.center.z - b.center.z) > a.halfExtent + b.halfExtent) return;

		bool aLeaf = a.childStart == -1;
		bool bLeaf = b.childStart == -1;

		if (aLeaf && bLeaf)
		{
			for (int i = 0; i < a.colliderCount; i++)
				for (int j = 0; j < b.colliderCount; j++)
					out.emplace_back(colliders[a.colliderStart + i], colliders[b.colliderStart + j]);
			return;
		}

		if (!aLeaf)
			for (int i = 0; i < 8; i++)
				if (a.childMask & (1 << i))
					CrossTest(a.childStart + i, cellIndexB, out);
				else
					for (int i = 0; i < 8; i++)
						if (b.childMask & (1 << i))
							CrossTest(cellIndexA, b.childStart + i, out);
	}
};