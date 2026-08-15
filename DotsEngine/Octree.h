#pragma once

#include <vector>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "DebugLines.h"

struct CollisionItem { size_t index; glm::vec3 pos; float radius; };

template<size_t MaxDepth, size_t MaxItems>
class CollisionTree
{
public:
	struct Node
	{
		inline constexpr bool IsLeaf() const { return children[0] == -1; }
		inline constexpr int GetChildIndex(const glm::vec3 position) const
		{
			return (position.x > center.x) | // 0-1 +
				((position.y > center.y) << 1) | // 0-2 +
				((position.z > center.z) << 2); // 0-4
		}

		glm::vec3 center{};
		float halfSize = 0;
		int children[8] = { -1,-1,-1,-1,-1,-1,-1,-1 };
		CollisionItem items[MaxItems + 1] {}; // +1 for temporary overflow before split
		int itemCount = 0;

		bool AddItem(const CollisionItem& item)
		{
			if (itemCount >= MaxItems + 1) return false;
			items[itemCount++] = item;
			return true;
		}
	};

	static constexpr int PoolSize(int maxDepth = MaxDepth)
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
	Node nodePool[PoolSize()];
	float halfSize;
	int maxPerNode;
	size_t lastIndex = 0;

public:
	CollisionTree(float halfSize = 0, int maxPerNode = 3) :
		halfSize(halfSize), maxPerNode(maxPerNode) {}

	void Reset(float aHalfSize, glm::vec3 center = glm::vec3())
	{
		halfSize = aHalfSize;
		lastIndex = 1; // index 0 is the root, already occupied
		nodePool[0] = Node{};
		nodePool[0].center = center;
		nodePool[0].halfSize = halfSize;
	}

	void Insert(size_t index, glm::vec3 pos, float radius)
	{
		InsertAt(0, { index, pos, radius }, 0);
	}

	void Query(glm::vec3 pos, float radius, std::vector<size_t>& out) const
	{
		QueryAt(0, pos, radius, out);
	}

	void DrawBounds() const
	{
		DrawNode(0);
	}

private:
	int Reserve(size_t count)
	{
		if (lastIndex + count > PoolSize()) return -1;
		int result = (int)lastIndex;
		lastIndex += count;
		return result;
	}

	bool FitsChild(const CollisionItem& item, const Node& child) const
	{
		glm::vec3 d = glm::abs(item.pos - child.center);
		return std::max({ d.x, d.y, d.z }) + item.radius <= child.halfSize;
	}

	void InsertAt(int nodeIndex, const CollisionItem& item, int depth)
	{
		Node& node = nodePool[nodeIndex];

		if (!node.IsLeaf())
		{
			int child = node.children[node.GetChildIndex(item.pos)];

			if (FitsChild(item, nodePool[child]))
			{
				InsertAt(child, item, depth + 1);
				return;
			}

			node.AddItem(item);
			return;
		}

		node.AddItem(item);

		if (node.itemCount > maxPerNode && depth < (int)MaxDepth)
		{
			int baseIndex = Reserve(8);

			if (baseIndex < 0)
				return;

			float halfsize = node.halfSize * 0.5f;
			glm::vec3 center = node.center;

			for (int i = 0; i < 8; i++)
			{
				Node& child = nodePool[baseIndex + i];
				child = Node{};
				child.center = center + glm::vec3((i & 1) ? halfsize : -halfsize, (i & 2) ? halfsize : -halfsize, (i & 4) ? halfsize : -halfsize);
				child.halfSize = halfsize;
				node.children[i] = baseIndex + i;
			}

			CollisionItem old[MaxItems + 1];
			int oldCount = node.itemCount;
			for (int i = 0; i < oldCount; i++) old[i] = node.items[i];
			node.itemCount = 0;

			for (int i = 0; i < oldCount; i++)
				InsertAt(nodeIndex, old[i], depth);
		}
	}

	void QueryAt(int nodeIndex, glm::vec3 pos, float radius, std::vector<size_t>& out) const
	{
		const Node& node = nodePool[nodeIndex];

		for (size_t i = 0; i < node.itemCount; i++)
		{
			float rSum = radius + node.items[i].radius;
			if (glm::length2(node.items[i].pos - pos) <= rSum * rSum) out.push_back(node.items[i].index);
		}

		if (node.IsLeaf()) return;

		for (int c : node.children)
			if (SphereBoxOverlap(pos, radius, nodePool[c].center, nodePool[c].halfSize))
				QueryAt(c, pos, radius, out);
	}

	void DrawNode(int nodeIndex) const
	{
		const Node& node = nodePool[nodeIndex];

		if (node.itemCount)
			DebugLines::DrawCube(node.center, glm::quat(), glm::vec3(node.halfSize), glm::vec3(1, 1, 1));

		if (!node.IsLeaf())
			for (int c : node.children) DrawNode(c);
	}

	static bool SphereBoxOverlap(glm::vec3 pos, float radius, glm::vec3 c, float hs)
	{
		glm::vec3 closest = glm::clamp(pos, c - glm::vec3(hs), c + glm::vec3(hs));
		return glm::length2(closest - pos) <= radius * radius;
	}
};