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
	glm::vec3 velocity{};
	int Health = 0;

	const glm::vec3 GetPosition() const { return position; }
	void SetPosition(const glm::vec3 newPosition) { position = newPosition; }

	const float GetRadius() const { return radius; }
	void SetRadius(const float newRadius) { radius = newRadius; }

	// returns true if dot is alive
	bool SetHealth(const int newHealth)
	{
		Health = newHealth;

		if (Health > 0 && Health < DotAssets::meshCount + 1)
			mesh = DotAssets::Meshes()[Health - 1];
		else
			return false;

		radius = mesh->SetRadius();
		return true;
	}
};

class DotsScene
{
	// stores dot instances for every different mesh
	std::vector<glm::mat4> meshModelInstances[DotAssets::meshCount];
	//std::unique_ptr<Dot[]> dots;
	//std::unique_ptr<Collider* []> dotColliders;
	Dot* dots = nullptr;
	Collider** dotColliders = nullptr;

	FlyingCamera* flyingCamera = nullptr;
	CollisionTree<6, 15> collisionTree = CollisionTree<6, 15>(7);
	ThreadPool<32> threadPool;

public:
	static Camera* mainCamera;

	int dotCount = 200;
	float bounds = 50.0f;
	float dotSpeed = 5.0f;
	bool isRunning = false;

	DotsScene();
	~DotsScene();

	void DotsScene_Start();

	void DotsScene_Render();
	void DotsScene_Update();

	void DotsScene_DrawBounds();

	Dot DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity);
	Dot DotsScene_CreateRandomDot();
	void DotsScene_ReplaceDot(const Dot& dot, size_t index);

	void DotsScene_ClearDots();
	void DotsScene_LeaderBoardMode();

	void DotsScene_HandleDotsCollisions();
	void DotsScene_HandleDotsCollide(int dot1, int dot2);
	void DotsScene_HandleBoundsCollisions();
	void DotsScene_HandleDeadDots();

	void DotsScene_NaiveDotRender();
	void DotsScene_InstancedDotRender();

	float RandomFloat(float min, float max);
};