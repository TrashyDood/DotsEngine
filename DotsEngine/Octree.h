#pragma once

#include <vector>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "DebugLines.h"

#include <iostream>
#include <vector>

class Collider
{
public:
	glm::vec3 position{};
	float radius = 0;
};

template<uint16_t MaxDepth, uint16_t CellCapacity>
class CollisionTree
{
public:
	struct Cell
	{
		inline constexpr bool IsLeaf() const { return childrenBaseIndex == -1; }
		inline constexpr int GetChildIndex(const glm::vec3& position) const // converts position to child index
		{
			return (position.x > center.x) | // 0-1 +
				((position.y > center.y) << 1) | // 0-2 +
				((position.z > center.z) << 2); // 0-4
		}

		glm::vec3 center{};
		float halfSize = 0;

		std::vector<int> colliderIndices{};
		//int colliderIndices[CellCapacity]{};
		int colliderCount = 0;

		int childrenBaseIndex = -1; // the index of our first child cell in the cell pool

		Cell() {}
		Cell(glm::vec3 center, float halfSize) :
			center(center), halfSize(halfSize) {}

		void ClearColliders() { colliderCount = 0; }

		bool AddCollider(int index, bool resize = false)
		{
			if (colliderCount >= CellCapacity && !resize)
				return false;

			//colliderIndices[colliderCount] = index;
			colliderIndices.push_back(index);
			colliderCount++;

			return true;
		}
	};

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

private:
	Collider** colliders = nullptr;
	int colliderCount = 0;

	Cell cellPool[CellPoolSize()];
	int occupiedCellCount = 1;

	float halfSize = 0;
	int overflowCount = 0;
	uint16_t cellSplitThreshhold = 0;

public:
	CollisionTree(uint16_t cellSplitThreshhold = CellCapacity) :
		cellSplitThreshhold(cellSplitThreshhold) { }

	constexpr int OverflowCount() const { return overflowCount; }

	constexpr int ColliderCount() const { return colliderCount; }

	void ResetCells(float halfSize, glm::vec3 center = glm::vec3())
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

		ResetCells(halfSize);

		for (int i = 0; i < count; i++)
			InsertAt(0, i, 0);
	}

	void Query(const Collider* collider, std::vector<int>& out) const
	{
		QueryAt(0, collider, out);
	}

	void DrawBounds() const { DrawCell(0); }

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

		if (!cell.IsLeaf())
		{
			int childCellIndex = cell.childrenBaseIndex +
				cell.GetChildIndex(colliders[colliderIndex]->position);

			if (FitsInsideChild(colliders[colliderIndex], cellPool[childCellIndex]))
			{
				InsertAt(childCellIndex, colliderIndex, depth + 1);
				return;
			}

			overflowCount += !cell.AddCollider(colliderIndex, true);
			return;
		}

		// check if we could add the collider to cell
		if (!cell.AddCollider(colliderIndex, (depth >= (int)MaxDepth)))
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
			if (SphereBoxOverlap(collider->position, collider->radius,
				cellPool[cell.childrenBaseIndex + i].center, cellPool[cell.childrenBaseIndex + i].halfSize))
					QueryAt(cell.childrenBaseIndex + i, collider, out);
	}


	void DrawCell(int cellIndex) const
	{
		const Cell& cell = cellPool[cellIndex];

		if (cell.colliderCount)
			DebugLines::DrawCube(cell.center, glm::quat(), glm::vec3(cell.halfSize), glm::vec3(1, 1, 1));

		if (!cell.IsLeaf())
			for (size_t i = 0; i < 8; i++)
				DrawCell(cell.childrenBaseIndex + i);
	}

	// checks if collider fits entirely inside cell.
	bool FitsInsideChild(Collider* collider, Cell& cell) const
	{
		glm::vec3 d = glm::abs(collider->position - cell.center);
		return std::max({ d.x, d.y, d.z }) + collider->radius <= cell.halfSize;
	}

	static bool SphereBoxOverlap(glm::vec3 position, float radius, glm::vec3 c, float hs)
	{
		glm::vec3 closest = glm::clamp(position, c - glm::vec3(hs), c + glm::vec3(hs));
		return glm::length2(closest - position) <= radius * radius;
	}
};