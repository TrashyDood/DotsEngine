#pragma once

#include <vector>
#include <memory>

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

#include "DebugLines.h"

struct OctItem { size_t index; glm::vec3 pos; float radius; };

class Octree
{
public:
	Octree(glm::vec3 center = glm::vec3(0, 0, 0), float halfSize = 0, int maxDepth = 6, int maxPerNode = 3)
		: center(center), halfSize(halfSize), maxDepth(maxDepth), maxPerNode(maxPerNode) {
	}

	void Clear() { items.clear(); for (auto& c : children) c.reset(); }

	void Insert(size_t index, glm::vec3 pos, float radius) { Insert({ index, pos, radius }, 0); }

	void Query(glm::vec3 pos, float radius, std::vector<size_t>& out) const
	{
		for (auto& it : items)
		{
			float rSum = radius + it.radius;
			if (glm::length2(it.pos - pos) <= rSum * rSum) out.push_back(it.index);
		}

		if (IsLeaf()) return;

		for (auto& c : children)
			if (SphereBoxOverlap(pos, radius, c->center, c->halfSize))
				c->Query(pos, radius, out);
	}

	bool HasItems() const
	{
		if (!items.empty()) return true;
		if (IsLeaf()) return false;
		for (auto& c : children)
			if (c->HasItems()) return true;
		return false;
	}

	void DrawBounds()
	{
		if (HasItems())
			DebugLines::DrawCube(center, glm::quat(), glm::vec3(halfSize, halfSize, halfSize), glm::vec3(1, 1, 1));
		if (!IsLeaf())
			for (auto& c : children)
				c->DrawBounds();
	}

private:
	glm::vec3 center;
	float halfSize;
	int maxDepth, maxPerNode;
	std::vector<OctItem> items;
	std::unique_ptr<Octree> children[8];

	bool IsLeaf() const { return children[0] == nullptr; }

	int ChildIndex(glm::vec3 position) const
	{
		return (position.x > center.x) | ((position.y > center.y) << 1) | ((position.z > center.z) << 2);
	}

	bool FitsChild(const OctItem& item, Octree* child) const
	{
		glm::vec3 d = glm::abs(item.pos - child->center);
		return std::max({ d.x, d.y, d.z }) + item.radius <= child->halfSize;
	}

	void Insert(const OctItem& item, int depth)
	{
		if (!IsLeaf())
		{
			Octree* c = children[ChildIndex(item.pos)].get();
			if (FitsChild(item, c)) { c->Insert(item, depth + 1); return; }
			items.push_back(item);
			return;
		}

		items.push_back(item);
		if ((int)items.size() > maxPerNode && depth < maxDepth)
		{
			float h = halfSize * 0.5f;
			for (int i = 0; i < 8; i++)
				children[i] = std::make_unique<Octree>(
					center + glm::vec3((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h), h, maxDepth, maxPerNode);

			std::vector<OctItem> old = std::move(items);
			items.clear();
			for (auto& it2 : old) Insert(it2, depth);
		}
	}

	static bool SphereBoxOverlap(glm::vec3 position, float radius, glm::vec3 child, float h)
	{
		glm::vec3 closest = glm::clamp(position, child - glm::vec3(h), child + glm::vec3(h));
		return glm::length2(closest - position) <= radius * radius;
	}
};

