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

struct BoundingBox
{
	glm::vec3 center{};
	float halfSize = 0;

	BoundingBox() {};

	BoundingBox(glm::vec3 center, float halfSize) :
		center(center), halfSize(halfSize) {}
};

namespace Utils
{
	inline constexpr bool SphereFitsAABB(const glm::vec3* position, float radius, const BoundingBox& box)
	{
		glm::vec3 axisDistance = glm::abs(*position - box.center);
		return std::max({ axisDistance.x, axisDistance.y, axisDistance.z }) + radius <= box.halfSize;
	}

	inline bool SphereOverlapsAABB(const glm::vec3* position, float radius, const BoundingBox& box)
	{
		return glm::length2(
			(glm::clamp(*position,
				box.center - glm::vec3(box.halfSize),
				box.center + glm::vec3(box.halfSize))) - *position) <= radius * radius;
	}

	inline glm::vec3 Lerp(const glm::vec3& a, const glm::vec3& b, float t) { return a + (b - a) * t; }
}

struct Collider
{
	glm::vec3 position{};
	glm::vec3 velocity{};
	float radius;

	Collider(glm::vec3 position = {}, float radius = 0) :
		position(position), radius(radius) { }

	inline constexpr bool FitsAABB(const BoundingBox& box) const { return Utils::SphereFitsAABB(&position, radius, box); }
	inline bool OverlapsAABB(const BoundingBox& box) const { return Utils::SphereOverlapsAABB(&position, radius, box); }
 };

template<uint8_t MaxDepth>
class CollisionTree2
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

		constexpr bool HasChild(const int slot) const { return childMask& (1 << slot); }
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
		CollisionTree2<MaxDepth>::colliders = colliders;
		CollisionTree2<MaxDepth>::colliderCount = colliderCount;
		CollisionTree2<MaxDepth>::splitThreshhold = splitThreshhold;
		CollisionTree2<MaxDepth>::looseMargin = looseMargin;

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
			Utils::Lerp(Color(1, 1, 1), Color(1, 0, 0), cell.colliderCount / (float)splitThreshhold));

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

template<uint8_t MaxDepth, uint8_t splitThreshhold, uint8_t SplitThreshhold>
class CollisionTree
{
public:
	struct Cell : public BoundingBox
	{
		int colliderIndices[splitThreshhold]{};
		int colliderCount = 0;

		int childStart = -1; // the index of our first child cell in the cell pool

		Cell(glm::vec3 center = {}, float halfSize = 0) : BoundingBox(center, halfSize) {}

		constexpr bool ShouldSplit(uint8_t depth) const { return colliderCount >= SplitThreshhold && depth < MaxDepth; }
		constexpr bool IsAtCapacity() const { return colliderCount > splitThreshhold - 1; }
		constexpr bool IsLeaf() const { return childStart == -1; }

		// converts position to child slot index
		constexpr int GetChildSlot(const glm::vec3& position) const
		{
			return (position.x > center.x) | // 0-1 +
				((position.y > center.y) << 1) | // 0-2 +
				((position.z > center.z) << 2); // 0-4
		}

		void ClearColliders() { colliderCount = 0; }

		bool AddCollider(int index)
		{
			if (colliderCount >= splitThreshhold)
				return false;

			colliderIndices[colliderCount++] = index;
			return true;
		}

		constexpr void CreateChild(uint32_t childIndex, Cell& outChildCell) const
		{
			float newHalfSize = halfSize * 0.5f;
			outChildCell = Cell(center + glm::vec3(
				(childIndex & 1) ? newHalfSize : -newHalfSize,
				(childIndex & 2) ? newHalfSize : -newHalfSize,
				(childIndex & 4) ? newHalfSize : -newHalfSize),
				newHalfSize);
		}
	};

	constexpr int ColliderCount() const { return colliderCount; }

	static constexpr int CellPoolSize(uint16_t maxDepth = MaxDepth)
	{
		int total = 0;
		int levelCount = 1;

		for (int i = 0; i <= maxDepth; i++)
		{
			total += levelCount;
			levelCount *= 8;
		}

		return total;
	}

	// expose these constants for external use
	static constexpr uint8_t maxDepth = MaxDepth;
	static constexpr uint8_t cellCapacity = splitThreshhold;
	static constexpr uint8_t splitThreshhold = SplitThreshhold;

private:
	Collider** colliders = nullptr;
	int colliderCount = 0;

	Cell cellPool[CellPoolSize()];
	int cellCount = 1;

	float halfSize = 0;

public:
	void Reset(float halfSize, glm::vec3 center = glm::vec3())
	{
		CollisionTree::halfSize = halfSize;

		cellCount = 1; // index 0 is the root, already occupied
		cellPool[0] = Cell{};
		cellPool[0].center = center;
		cellPool[0].halfSize = halfSize;
	}

	void Build(Collider** newColliders, uint32_t count, float halfSize, uint32_t& outOverflowCount)
	{
		colliders = newColliders;
		colliderCount = count;
		outOverflowCount = 0;

		Reset(halfSize);

		for (int i = 0; i < count; i++)
			InsertAt(0, i, 0, outOverflowCount);
	}

	void Query(const Collider* collider, std::vector<int>& out) const { QueryAt(0, collider, out); }

	void Draw() const { DrawAt(0); }

private:
	// reserves a given range of indices in the cell pool
	int ReserveCells(uint8_t count = 8)
	{
		if (cellCount + count > CellPoolSize())
			return -1; // throw error instead

		int result = cellCount;
		cellCount += count;

		return result;
	}

	void InsertAt(uint32_t rootIndex, uint32_t colliderIndex, uint32_t depth, uint32_t& outOverflowCount)
	{
		Cell& cell = cellPool[rootIndex];
		Collider* collider = colliders[colliderIndex];

		if (!cell.IsLeaf())
		{
			int childCellIndex = cell.childStart +
				cell.GetChildSlot(colliders[colliderIndex]->position);

			if (collider->FitsAABB(cellPool[childCellIndex]))
			{
				InsertAt(childCellIndex, colliderIndex, depth + 1, outOverflowCount);
				return;
			}

			outOverflowCount += !cell.AddCollider(colliderIndex);
			return;
		}

		// check if we could add the collider to cell
		if (!cell.AddCollider(colliderIndex))
		{
			outOverflowCount++;
			return;
		}

		// check if we should split the cell
		if (!(cell.colliderCount >= SplitThreshhold && depth < (int)MaxDepth))
			return;

		int baseIndex = ReserveCells(8);

		cell.childStart = baseIndex;
		for (int i = 0; i < 8; i++)
			cell.CreateChild(i, cellPool[baseIndex + i]);

		int oldIndices[splitThreshhold];
		int oldCount = cell.colliderCount;
		for (int i = 0; i < oldCount; i++)
			oldIndices[i] = cell.colliderIndices[i];

		cell.ClearColliders();

		for (int i = 0; i < oldCount; i++)
			InsertAt(rootIndex, oldIndices[i], depth, outOverflowCount);
	}

	void QueryAt(const int rootIndex, const Collider* collider, std::vector<int>& out) const
	{
		const Cell& cell = cellPool[rootIndex];

		for (uint16_t i = 0; i < cell.colliderCount; i++)
		{
			Collider* other = colliders[cell.colliderIndices[i]];

			float rSum = collider->radius + other->radius;
			if (glm::length2(other->position - collider->position) <= rSum * rSum)
				out.push_back(cell.colliderIndices[i]);
		}

		if (cell.IsLeaf()) return;

		for (size_t i = 0; i < 8; i++)
			if (collider->OverlapsAABB(cellPool[cell.childStart + i]))
				QueryAt(cell.childStart + i, collider, out);
	}

	void DrawAt(int rootIndex) const
	{
		const Cell& cell = cellPool[rootIndex];

		//if (cell.colliderCount)
		DebugLines::DrawCube(cell.center, glm::quat(),
			glm::vec3(cell.halfSize),
			Utils::Lerp(
				Color(1, 1, 1),
				Color(1, 0, 0),
				cell.colliderCount / (float)splitThreshhold));

		if (!cell.IsLeaf())
			for (size_t i = 0; i < 8; i++)
				DrawAt(cell.childStart + i);
	}
};