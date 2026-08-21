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

	std::vector<int> buildScratch;

public:
	void BuildRoot(const glm::vec3* positionsIn, const float* radiiIn, uint32_t colliderCountIn,
		float rootHalfExtent, size_t splitThreshholdIn, float looseMarginIn = 0, glm::vec3 rootCenter = glm::vec3())
	{
		positions = positionsIn;
		radii = radiiIn;
		colliderCount = (int)colliderCountIn;
		splitThreshhold = (uint8_t)splitThreshholdIn;
		looseMargin = looseMarginIn;

		// only (re)build identity mapping when the count actually changes,
		// not every frame — BuildCell doesn't care about starting order.
		if ((int)indexPool.size() != colliderCount)
		{
			indexPool.resize(colliderCount);
			for (int i = 0; i < colliderCount; ++i)
				indexPool[i] = i;
		}

		cellCount = 1; // index 0 is the root, already occupied
		cellPool[0] = Cell{
			rootCenter,
			rootHalfExtent,
			0,
			(int)colliderCount
		};
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
			int idx = indexPool[start + i];
			int slot = (positions[idx].x > cell.center.x) |
				((positions[idx].y > cell.center.y) << 1) |
				((positions[idx].z > cell.center.z) << 2);
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
			int idx = indexPool[start + i];
			int slot = (positions[idx].x > cell.center.x) |
				((positions[idx].y > cell.center.y) << 1) |
				((positions[idx].z > cell.center.z) << 2);

			buildScratch[cursor[slot]] = idx;
			cursor[slot]++;
		}

		for (int i = 0; i < count; i++)
			indexPool[start + i] = buildScratch[i];

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
				cell.childMask |= (1 << i);
		}

		cell.childStart = firstChild;
		cell.colliderCount = 0;

		for (int i = 0; i < 8; i++)
			if (counts[i] > 0)
				BuildCell(firstChild + i, depth + 1);
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
			for (int i = 0; i < 8; i++)
				if (a.childMask & (1 << i))
					CrossTest(a.childStart + i, cellIndexB, out);
				else
					for (int i = 0; i < 8; i++)
						if (b.childMask & (1 << i))
							CrossTest(cellIndexA, b.childStart + i, out);
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