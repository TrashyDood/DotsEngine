#pragma once
#include <vector>
#include <Camera/FlyingCamera.h>
#include <array>
#include <memory>

#include "Mesh_Sphere.h"
#include "Shader.h"

#include "Octree.h"
#include "ThreadPool.h"

namespace DotAssets
{
	static constexpr int meshCount = 4;
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline std::array<Mesh_Sphere*, meshCount>& Meshes()
	{
		static std::array<Mesh_Sphere*, meshCount> meshes = {
			new Mesh_Sphere(1, glm::vec3(1, 0, 0)),
			new Mesh_Sphere(2, glm::vec3(0, 1, 0)),
			new Mesh_Sphere(3, glm::vec3(0, 0, 1)),
			new Mesh_Sphere(4, glm::vec3(1, 0 ,1))
		};
		return meshes;
	}
}

struct Dot : public Collider
{
	Mesh_Sphere* mesh = nullptr;
	int Health = 0;

	// returns true if dot is alive
	bool SetHealth(const int newHealth)
	{
		Health = newHealth;

		if (Health > 0 && Health < DotAssets::meshCount + 1)
			mesh = DotAssets::Meshes()[Health - 1];
		else
			return false;

		radius = mesh->Radius();
		return true;
	}
};

class DotsScene
{
	// stores dot instances for every different mesh
	std::vector<glm::mat4> meshModelInstances[DotAssets::meshCount];

	uint32_t dotsCount = 0;
	Dot* dots = nullptr;
	Collider** colliders = nullptr;

	FlyingCamera* flyingCamera = nullptr;
	ThreadPool<32> threadPool;
	CollisionTree<1, 1, 1> collisionTree;
	CollisionTree2<5> collisionTree2;

public:
	static Camera* mainCamera;

	int pendingDotsCount = 200;
	float bounds = 50.0f;
	float dotSpeed = 5.0f;
	bool isSimulating = false;

	DotsScene();
	~DotsScene();

	void DotsScene_Start();
	void DotsScene_Pause();
	void DotsScene_Resume();

	void DotsScene_Render();
	void DotsScene_Update();

	void DotsScene_DrawBounds();

	Dot DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity);
	Dot DotsScene_CreateRandomDot();
	void DotsScene_ReplaceDot(const Dot& dot, size_t index);

	void DotsScene_ClearDots();
	void DotsScene_LeaderBoardMode();

	void DotsScene_HandleDotsCollisions();
	void DotsScene_HandleDotsCollisions2();
	void DotsScene_HandleDotsCollisions2_Threaded();
	void DotsScene_HandleBoundsCollisions();
	void DotsScene_ResolveCollision(int dot1, int dot2);

	void DotsScene_NaiveDotRender();
	void DotsScene_InstancedDotRender();

	float RandomFloat(float min, float max);
};