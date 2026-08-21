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
	static constexpr int meshCount = 3;
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

class DotsScene
{
	FlyingCamera* flyingCamera = nullptr;
	ThreadPool<32> threadPool;
	CollisionTree<5> collisionTree;
	CollisionTreeSOA<5> collisionTreeSOA;

	// mesh instance buffers
	std::vector<glm::mat4> meshInstances[DotAssets::meshCount];
	glm::mat4* mInstances[DotAssets::meshCount] = { nullptr, nullptr, nullptr };
	int* instanceCounts[DotAssets::meshCount] = { nullptr, nullptr, nullptr };

	uint32_t dotsCount = 0;

	// AoS dot data
	Dot* dots = nullptr;
	Collider** colliders = nullptr;

	// SoA dot data
	int* dotsHealths = nullptr;
	glm::vec3* dotsPositions = nullptr;
	glm::vec3* dotsVelocities = nullptr;
	float* dotsRadii = nullptr;
	int* dotsMeshIndices = nullptr;

public:
	static Camera* mainCamera;

	int pendingDotsCount = 200;
	float bounds = 50.0f;
	float dotSpeed = 5.0f;
	bool isSimulating = false;
	bool drawTree = false;

	DotsScene();
	~DotsScene();

	void DotsScene_Start();
	void DotsScene_LeaderBoardMode();

	void DotsScene_Render();
	void DotsScene_Update();

	void DotsScene_Pause();
	void DotsScene_Resume();

	void DotsScene_DrawBounds();

	// Dot management
	void DotsScene_InitDots();
	void DotsScene_DeleteDots();
	void DotsScene_UpdateDot(int dot, float deltaTime);
	Dot DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity);
	Dot DotsScene_CreateRandomDot();
	void DotsScene_ReplaceDot(const Dot& dot, size_t index);

	// Collision handling
	void DotsScene_HandleCollisions();
	void DotsScene_HandleCollisions_Threaded();
	void DotsScene_ResolveCollision(int dot1, int dot2);
	void DotsScene_OnDotCollide(int dot);
	void DotsScene_HandleBoundsCollisions();

	// Dot management with SoA
	void DotsScene_InitDotsData();
	void DotsScene_DeleteDotsData();
	void DotsScene_UpdateDotData(int dot, float deltaTime);
	void DotsScene_SetDotData(int dot, glm::vec3 position = {}, glm::vec3 velocity = {}, int health = 3);
	bool DotsScene_SetDotHealth(int dot, int health);

	// Collision handling with SoA
	void DotsScene_HandleCollisionsSOA_Threaded();
	void DotsScene_ResolveCollisionSOA(int dot1, int dot2);
	void DotsScene_OnDotCollideSOA(int dot);
	void DotsScene_HandleBoundsCollisionsSOA();

	// Rendering
	void DotsScene_InitMeshBuffers();
	void DotsScene_DeleteMeshBuffers();
	void DotsScene_RenderMeshBuffers();
	void DotsScene_ClearInstanceBuffers();
	void DotsScene_AddMeshInstance(const size_t meshIndex, const glm::vec3& position);

	float RandomFloat(float min, float max);
};