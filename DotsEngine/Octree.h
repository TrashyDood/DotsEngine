#pragma once

#include <vector>
#include <memory>
#include <math.h>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "DebugLines.h"
#include "tracy/Tracy.hpp"

struct BoundingBox
{
	glm::vec3 center;
	float halfSize;

	BoundingBox(glm::vec3 center, float halfSize) :
		center(center), halfSize(halfSize) { }
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
}

struct Collider
{
	glm::vec3 position;
	float radius;

	Collider(glm::vec3 position = {}, float radius = 0) :
		position(position), radius(radius) { }

	constexpr bool FitsAABB(const BoundingBox& box) const { return Utils::SphereFitsAABB(&position, radius, box); }
	bool OverlapsAABB(const BoundingBox& box) const { return Utils::SphereOverlapsAABB(&position, radius, box); }
 };

template<unsigned char MaxDepth, unsigned char CellCapacity>
class CollisionTree
{
public:
	struct Cell : public BoundingBox
	{
		//std::vector<int> colliderIndices{};
		int colliderIndices[CellCapacity]{};
		int colliderCount = 0;

		int childrenBaseIndex = -1; // the index of our first child cell in the cell pool

		Cell(glm::vec3 center = {}, float halfSize = 0) : BoundingBox(center, halfSize) {}

		constexpr bool IsLeaf() const { return childrenBaseIndex == -1; }

		// converts position to child index
		constexpr int GetChildIndex(const glm::vec3& position) const
		{
			return (position.x > center.x) | // 0-1 +
				((position.y > center.y) << 1) | // 0-2 +
				((position.z > center.z) << 2); // 0-4
		}

		void ClearColliders()
		{
			//colliderIndices.clear();
			colliderCount = 0;
		}

		bool AddCollider(int index, bool resize = false)
		{
			if (colliderCount >= CellCapacity/* && !resize*/)
				return false;

			//colliderIndices.push_back(index);
			colliderIndices[colliderCount] = index;
			colliderCount++;

			return true;
		}
	};

	constexpr int OverflowCount() const { return overflowCount; }
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

	static constexpr unsigned char maxDepth = MaxDepth;
	static constexpr unsigned char cellCapacity = CellCapacity;

private:
	Collider** colliders = nullptr;
	int colliderCount = 0;

	Cell cellPool[CellPoolSize()];
	int occupiedCellCount = 1;

	float halfSize = 0;
	int overflowCount = 0;
	unsigned char cellSplitThreshhold = 0;

public:
	CollisionTree(unsigned char cellSplitThreshhold = CellCapacity) :
		cellSplitThreshhold(cellSplitThreshhold) { }

	void Reset(float halfSize, glm::vec3 center = glm::vec3())
	{
		CollisionTree::halfSize = halfSize;

		occupiedCellCount = 1; // index 0 is the root, already occupied
		cellPool[0] = Cell{};
		cellPool[0].center = center;
		cellPool[0].halfSize = halfSize;

		overflowCount = 0;
	}

	void Build(Collider** newColliders, int count, float halfSize)
	{
		colliders = newColliders;
		colliderCount = count;

		Reset(halfSize);

		for (int i = 0; i < count; i++)
			InsertAt(0, i, 0);
	}

	void Query(const Collider* collider, std::vector<int>& out) const { QueryAt(0, collider, out); }

	void Draw() const { DrawAt(0); }

private:
	// reserves a given range of indices in the cell pool
	int ReserveCells(int count)
	{
		if (occupiedCellCount + count > CellPoolSize())
			return -1;

		int result = (int)occupiedCellCount;
		occupiedCellCount += count;

		return result;
	}

	void InsertAt(int cellIndex, int colliderIndex, int depth)
	{
		Cell& cell = cellPool[cellIndex];
		Collider* collider = colliders[colliderIndex];

		if (!cell.IsLeaf())
		{
			int childCellIndex = cell.childrenBaseIndex +
				cell.GetChildIndex(colliders[colliderIndex]->position);

			if (collider->FitsAABB(cellPool[childCellIndex]))
			{
				InsertAt(childCellIndex, colliderIndex, depth + 1);
				return;
			}

			overflowCount += !cell.AddCollider(colliderIndex, true);
			return;
		}

		// check if we could add the collider to cell
		if (!cell.AddCollider(colliderIndex, depth >= (int)MaxDepth))
		{
			overflowCount++;
			return;
		}

		// check if we should split the cell
		if (!(cell.colliderCount >= cellSplitThreshhold && depth < (int)MaxDepth))
			return;

		int baseIndex = ReserveCells(8);
		if (baseIndex < 0)
			return;

		float halfsize = cell.halfSize * 0.5f;
		cell.childrenBaseIndex = baseIndex;

		// allocate child indices, then recreate them
		for (int i = 0; i < 8; i++)
			cellPool[baseIndex + i] = Cell(cell.center + glm::vec3(
				(i & 1) ? halfsize : -halfsize,
				(i & 2) ? halfsize : -halfsize,
				(i & 4) ? halfsize : -halfsize),
				halfsize);

		uint32_t oldIndices[CellCapacity];
		int oldCount = cell.colliderCount;
		for (int i = 0; i < oldCount; i++)
			oldIndices[i] = cell.colliderIndices[i];

		cell.ClearColliders();

		for (int i = 0; i < oldCount; i++)
			 InsertAt(cellIndex, oldIndices[i], depth);
	}

	void QueryAt(const int cellIndex, const Collider* collider, std::vector<int>& out) const
	{
		const Cell& cell = cellPool[cellIndex];

		for (uint16_t i = 0; i < cell.colliderCount; i++)
		{
			Collider* other = colliders[cell.colliderIndices[i]];

			float rSum = collider->radius + other->radius;
			if (glm::length2(other->position - collider->position) <= rSum * rSum)
				out.push_back(cell.colliderIndices[i]);
		}

		if (cell.IsLeaf()) return;

		for (size_t i = 0; i < 8; i++)
			if (collider->OverlapsAABB(cellPool[cell.childrenBaseIndex + i]))
					QueryAt(cell.childrenBaseIndex + i, collider, out);
	}

	void DrawAt(int cellIndex) const
	{
		const Cell& cell = cellPool[cellIndex];

		if (cell.colliderCount)
			DebugLines::DrawCube(cell.center, glm::quat(), glm::vec3(cell.halfSize), glm::vec3(1, 1, 1));

		if (!cell.IsLeaf())
			for (size_t i = 0; i < 8; i++)
				DrawAt(cell.childrenBaseIndex + i);
	}
};

//// SoA tree
//template<unsigned char MaxDepth, unsigned char CellCapacity>
//class CollisionTree
//{
//public:
//	struct Cell : public BoundingBox
//	{
//		int colliderIndices[CellCapacity]{};
//		int colliderCount = 0;
//
//		int childrenBaseIndex = -1; // the index of our first child cell in the cell pool
//
//		Cell(glm::vec3 center = {}, float halfSize = 0) : BoundingBox(center, halfSize) {}
//
//		constexpr bool IsLeaf() const { return childrenBaseIndex == -1; }
//
//		// converts position to child index
//		constexpr int GetChildIndex(const glm::vec3* position) const
//		{
//			return (position->x > center.x) | // 0-1 +
//				((position->y > center.y) << 1) | // 0-2 +
//				((position->z > center.z) << 2); // 0-4
//		 }
//
//		void ClearColliders() { colliderCount = 0; }
//
//		bool AddCollider(int index)
//		{
//			if (colliderCount >= CellCapacity)
//				return false;
//
//			colliderIndices[colliderCount] = index;
//			colliderCount++;
//
//			return true;
//		}
//	};
//
//	constexpr int OverflowCount() const { return overflowCount; }
//	constexpr int ColliderCount() const { return colliderCount; }
//
//	static constexpr int CellPoolSize(uint16_t maxDepth = MaxDepth)
//	{
//		int total = 0;
//		int levelCount = 1;
//
//		for (int i = 0; i <= maxDepth; i++)
//		{
//			total += levelCount;
//			levelCount *= 8;
//		}
//
//		return total;
//	}
//
//	static constexpr unsigned char maxDepth = MaxDepth;
//	static constexpr unsigned char cellCapacity = CellCapacity;
//
//private:
//	glm::vec3* positions = nullptr;
//	float* radii = nullptr;
//	size_t colliderCount = 0;
//
//	Cell cellPool[CellPoolSize()];
//	int occupiedCellsCount = 1;
//
//	float halfSize = 0;
//	int overflowCount = 0;
//	unsigned char cellSplitThreshhold = 0;
//
//public:
//	CollisionTree(unsigned char cellSplitThreshhold = CellCapacity) :
//		cellSplitThreshhold(cellSplitThreshhold) { }
//
//	void Reset(float halfSize, glm::vec3 center = glm::vec3())
//	{
//		CollisionTree::halfSize = halfSize;
//
//		occupiedCellsCount = 1; // index 0 is the root, already occupied
//		cellPool[0] = Cell{};
//		cellPool[0].center = center;
//		cellPool[0].halfSize = halfSize;
//
//		colliderCount = 0;
//		overflowCount = 0;
//	}
//
//	void Build(glm::vec3* positions, float* radii, size_t count, float halfSize)
//	{
//		Reset(halfSize);
//
//		colliderCount = count;
//		CollisionTree::positions = positions;
//		CollisionTree::radii = radii;
//
//		for (int i = 0; i < count; i++)
//			InsertAt(0, i, 0);
//	}
//
//	void Query(const glm::vec3* position, float radius, std::vector<int>& out) const { QueryAt(0, position, radius, out); }
//	void Draw() const { DrawAt(0); }
//
//private:
//	// reserves a given range of indices in the cell pool
//	int ReserveCells(int count)
//	{
//		if (occupiedCellsCount + count > CellPoolSize())
//			return -1;
//
//		int result = (int)occupiedCellsCount;
//		occupiedCellsCount += count;
//
//		return result;
//	}
//
//	void InsertAt(int cellIndex, int colliderIndex, int depth)
//	{
//		Cell& cell = cellPool[cellIndex];
//
//		glm::vec3* position = &positions[colliderIndex];
//		float* radius = &radii[colliderIndex];
//
//		if (!cell.IsLeaf())
//		{
//			int childCellIndex = cell.childrenBaseIndex + cell.GetChildIndex(position);
//
//			if (Utils::SphereFitsAABB(position, *radius, cellPool[childCellIndex]))
//			{
//				InsertAt(childCellIndex, colliderIndex, depth + 1);
//				return;
//			}
//
//			overflowCount += !cell.AddCollider(colliderIndex);
//			return;
//		}
//
//		if (!cell.AddCollider(colliderIndex))
//		{
//			overflowCount++;
//			return;
//		}
//
//		if (!(cell.colliderCount >= cellSplitThreshhold && depth < (int)MaxDepth))
//			return;
//
//		int baseIndex = ReserveCells(8);
//		if (baseIndex < 0)
//			return;
//
//		float halfsize = cell.halfSize * 0.5f;
//		cell.childrenBaseIndex = baseIndex;
//
//		for (int i = 0; i < 8; i++)
//			cellPool[baseIndex + i] = Cell(
//				cell.center + glm::vec3(
//					(i & 1) ? halfsize : -halfsize,
//					(i & 2) ? halfsize : -halfsize,
//					(i & 4) ? halfsize : -halfsize),
//				halfsize);
//
//		uint32_t oldIndices[CellCapacity];
//		int oldCount = cell.colliderCount;
//
//		for (int i = 0; i < oldCount; i++)
//			oldIndices[i] = cell.colliderIndices[i];
//
//		cell.ClearColliders();
//
//		for (int i = 0; i < oldCount; i++)
//			InsertAt(cellIndex, oldIndices[i], depth);
//	}
//
//	void QueryAt(const int cellIndex, const glm::vec3* position, float radius, std::vector<int>& out) const
//	{
//		const Cell& cell = cellPool[cellIndex];
//
//		for (uint16_t i = 0; i < cell.colliderCount; i++)
//		{
//			glm::vec3* otherPosition = &positions[cell.colliderIndices[i]];
//			float* otherRadius = &radii[cell.colliderIndices[i]];
//
//			float rSum = radius + *otherRadius;
//
//			if (glm::length2(*otherPosition - *position) <= rSum * rSum)
//				out.push_back(cell.colliderIndices[i]);
//		}
//
//		if (cell.IsLeaf())
//			return;
//
//		for (size_t i = 0; i < 8; i++)
//			if (Utils::SphereOverlapsAABB(
//				position,
//				radius,
//				cellPool[cell.childrenBaseIndex + i]))
//				QueryAt(cell.childrenBaseIndex + i, position, radius, out);
//	}
//
//	void DrawAt(int cellIndex) const
//	{
//		const Cell& cell = cellPool[cellIndex];
//
//		if (cell.colliderCount)
//			DebugLines::DrawCube(cell.center, glm::quat(), glm::vec3(cell.halfSize), glm::vec3(1, 1, 1));
//
//		if (!cell.IsLeaf())
//			for (size_t i = 0; i < 8; i++)
//				DrawAt(cell.childrenBaseIndex + i);
//	}
//};