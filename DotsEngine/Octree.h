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
	float radius;

	Collider(glm::vec3 position = {}, float radius = 0) :
		position(position), radius(radius) { }

	inline constexpr bool FitsAABB(const BoundingBox& box) const { return Utils::SphereFitsAABB(&position, radius, box); }
	inline bool OverlapsAABB(const BoundingBox& box) const { return Utils::SphereOverlapsAABB(&position, radius, box); }
 };


template<uint8_t MaxDepth, uint8_t splitThreshhold, uint8_t SplitThreshhold>
class OldCollisionTree
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
		OldCollisionTree::halfSize = halfSize;

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