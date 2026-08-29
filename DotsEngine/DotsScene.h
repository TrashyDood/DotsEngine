#pragma once
#include <vector>
#include <Camera/FlyingCamera.h>
#include <array>

#include "Mesh_Sphere.h"
#include "Shader.h"

#include "CollisionTree.h"
#include "ThreadPool.h"

namespace DotAssets
{
	static constexpr uint16_t meshCount = 3;
	static constexpr uint8_t lodCount = 3;
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline std::array<Mesh_Sphere*, meshCount>& Meshes()
	{
		static std::array<Mesh_Sphere*, meshCount> meshes = {
			new Mesh_Sphere(1, glm::vec3(1, 0, 0), 3.5f),
			new Mesh_Sphere(2, glm::vec3(0, 1, 0), 3.5f),
			new Mesh_Sphere(3, glm::vec3(0, 0, 1), 3.5f) };
		return meshes;
	}
}

struct RenderContext
{
	glm::mat4* transformBuffers[2][DotAssets::meshCount]{};
	uint32_t countBuffers[2][DotAssets::meshCount]{};
	std::atomic<int> frontIndex{ 0 };

	RenderContext() = default;
	~RenderContext() { DeleteBuffers(); }

	void Init(const uint32_t bufferSize)
	{
		DeleteBuffers();

		for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
			for (int set = 0; set < 2; set++)
					transformBuffers[set][mesh] = new glm::mat4[bufferSize];
	}

	void DeleteBuffers()
	{
		for (int set = 0; set < 2; set++)
		{
			for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
			{
				delete[] transformBuffers[set][mesh];
				transformBuffers[set][mesh] = nullptr;
			}
		}
	}

	uint8_t GetWriteBufferIndex() { return 1 - frontIndex.load(std::memory_order_relaxed); }
	uint8_t GetReadBufferIndex() { return frontIndex.load(std::memory_order_acquire); }
	void SwapBuffers(uint8_t writeIndex) { frontIndex.store(writeIndex, std::memory_order_release); }
};

class DotsScene
{
	CollisionTree treeA, treeB;
	CollisionTree* readTree = &treeA, *writeTree = &treeB;
	std::future<void> treeTask;

	std::future<void> simulationTask;

	int* dotsHealths = nullptr;
	glm::vec3* dotsPositions = nullptr;
	glm::vec3* dotsVelocities = nullptr;
	float* dotsRadii = nullptr;
	int* dotsMeshIndices = nullptr;

public:
	static const uint32_t dotsCount_Default = 40000;

	FlyingCamera camera;
	RenderContext renderContext;

	float bounds = 50.0f,
		dotsSpeed = 5.0f;
	bool drawTree = false,
		asyncSimulation = true;

	uint32_t dotsCount = 0;

	DotsScene();
	~DotsScene();

	void DotsScene_Start(uint32_t dotsCount = dotsCount_Default);
	void DotsScene_LeaderBoardMode(uint32_t dotsCount = dotsCount_Default);
	void DotsScene_CollisionTest();

	void DotsScene_Tick();
	void DotsScene_Render(RenderContext& context) const;
	void DotsScene_Simulate();
	void DotsScene_Simulate_Async();

	void DotsScene_DrawBounds() const;

	void DotsScene_InitDotsData();
	void DotsScene_ClearDotsData();
	void DotsScene_UpdateDotsData(bool parallel = true);
	void DotsScene_UpdateTransformBuffers();
	void DotsScene_SetDotData(int dot, glm::vec3 position = {}, glm::vec3 velocity = {}, int health = 3);
	void DotsScene_SetDotHealth(int dot, int health);

	void DotsScene_BuildAndQueryTree(std::vector<std::pair<int, int>>& pairs, CollisionTree& tree, bool async = true);
	void DotsScene_ResolveCollisions(std::vector<std::pair<int, int>>& pairs);
	void DotsScene_ResolveCollision(int dot1, int dot2);
	void DotsScene_HandleBoundsCollisions();

	void DotsScene_RenderMeshBuffers(RenderContext& context) const;

	float RandomFloat(float min, float max) const;
};