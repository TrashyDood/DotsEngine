#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>

#include "Octree.h"
#include "ThreadPool.h"

namespace DotAssets
{
	static constexpr int meshCount = 3;
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline std::array<Mesh_Sphere*, 3>& Meshes()
	{
		static std::array<Mesh_Sphere*, meshCount> meshes = {
			new Mesh_Sphere(1, glm::vec3(1, 0, 0)),
			new Mesh_Sphere(2, glm::vec3(0, 1, 0)),
			new Mesh_Sphere(3, glm::vec3(0, 0, 1))
		};
		return meshes;
	}
}

struct Dot
{
	Mesh_Sphere* mesh = nullptr;

	glm::vec3 position{};
	glm::vec3 velocity{};

	float radius = 0;
	int Health = 0;

	void Refresh()
	{
		if (Health > 0 && Health < 4)
			mesh = DotAssets::Meshes()[Health - 1];

		radius = mesh->Radius();
	}
};

class DotsScene
{
	std::vector<glm::mat4> meshInstances[DotAssets::meshCount]; // stores dot instances for every different mesh
	std::vector<Dot> dots;

	FlyingCamera* flyingCamera = nullptr;
	Octree collisionTree;

public:
	static Camera* mainCamera;

	int dotCount = 200;
	float bounds = 50.0f;
	float dotSpeed = 5.0f;

	DotsScene();
	~DotsScene();

	void DotsScene_Start();

	void DotsScene_Render();
	void DotsScene_Update();

	void DotsScene_DrawBounds();

	void DotsScene_SpawnRandomDot();
	void DotsScene_SpawnDot(glm::vec3 aPosition, glm::vec3 aVelocity);

	void DotsScene_ClearDots();
	void DotsScene_LeaderBoardMode();

	void DotsScene_HandleDotsCollisions();
	void DotsScene_HandleBoundsCollisions();
	void DotsScene_HandleDeadDots();

	void DotsScene_NaiveDotRender();
	void DotsScene_InstancedDotRender();

	float RandomFloat(float min, float max);
};