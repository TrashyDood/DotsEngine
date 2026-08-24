#pragma once
#include <vector>
#include <Camera/FlyingCamera.h>
#include <array>
#include <memory>

#include "Mesh_Sphere.h"
#include "Shader.h"

#include "CollisionTree.h"
#include "ThreadPool.h"

namespace DotAssets
{
	static constexpr uint16_t meshCount = 3;
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline std::array<Mesh_Sphere*, meshCount>& Meshes()
	{
		static std::array<Mesh_Sphere*, meshCount> meshes = {
			new Mesh_Sphere(1, glm::vec3(1, 0, 0)),
			new Mesh_Sphere(2, glm::vec3(0, 1, 0)),
			new Mesh_Sphere(3, glm::vec3(0, 0, 1))
		};
		return meshes;
	}
}

struct Dot : public Collider
{
	glm::vec3 velocity{};

	int meshIndex = 2;
	int health = 3;

	void SetMesh(int index)
	{
		meshIndex = index;
		radius = DotAssets::Meshes()[index]->Radius();
	}
};

struct RenderContext
{
	glm::mat4* instanceTransformBuffers[2][DotAssets::meshCount] = {};
	uint32_t instanceCounts[2][DotAssets::meshCount] = {};
	std::atomic<int> frontIndex{ 0 };
	uint32_t bufferSize = 0;

	RenderContext() = default;
	~RenderContext() { DeleteBuffers(); }

	void Init(const uint32_t bufferSize)
	{
		DeleteBuffers();
		RenderContext::bufferSize = bufferSize;

		for (int set = 0; set < 2; set++)
			for (size_t i = 0; i < DotAssets::meshCount; i++)
				instanceTransformBuffers[set][i] = new glm::mat4[bufferSize];
	}

	void DeleteBuffers()
	{
		for (int set = 0; set < 2; set++)
			for (size_t i = 0; i < DotAssets::meshCount; i++)
			{
				delete[] instanceTransformBuffers[set][i];
				instanceTransformBuffers[set][i] = nullptr;
			}
	}
};

class DotsScene
{
	CollisionTree<5> collisionTree;
	CollisionTreeSOA<5> collisionTreeSOA;

	// SoA dot data
	int* dotsHealths = nullptr;
	glm::vec3* dotsPositions = nullptr;
	glm::vec3* dotsVelocities = nullptr;
	float* dotsRadii = nullptr;
	int* dotsMeshIndices = nullptr;

	std::atomic<bool> isSimulating{ true };
	std::future<void> simulationThread;

public:
	static const uint32_t defaultDotsCount = 200;

	static Camera* mainCamera;
	FlyingCamera* flyingCamera = nullptr;
	RenderContext renderContext;

	float bounds = 50.0f,
		dotSpeed = 5.0f;
	bool drawTree = false,
		threadedSimulation = false;

	uint32_t dotsCount = 0;

	DotsScene();
	~DotsScene();

	void DotsScene_Start(bool threadedSimulation, uint32_t dotsCount = defaultDotsCount);
	void DotsScene_LeaderBoardMode(bool threadedSimulation, uint32_t dotsCount = defaultDotsCount);

	void DotsScene_Render(RenderContext& context) const;
	void DotsScene_Simulate(float deltaTime);

	void DotsScene_DrawBounds() const;

	// Dot management with SoA
	void DotsScene_InitDotsData();
	void DotsScene_ClearDotsData();
	void DotsScene_UpdateDotData_Threaded(float deltaTime);
	void DotsScene_SetDotData(int dot, glm::vec3 position = {}, glm::vec3 velocity = {}, int health = 3);
	bool DotsScene_SetDotHealth(int dot, int health);

	// Collision handling with SoA
	void DotsScene_HandleCollisions_Threaded();
	void DotsScene_ResolveCollision(int dot1, int dot2);
	void DotsScene_OnDotCollide(int dot);
	void DotsScene_HandleBoundsCollisions_Threaded();

	// Rendering
	void DotsScene_RenderMeshBuffers_Threaded(RenderContext& context) const;

	float RandomFloat(float min, float max) const;
};