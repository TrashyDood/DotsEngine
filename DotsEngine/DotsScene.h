#pragma once
#include "Mesh_Sphere.h"
#include "Shader.h"
#include <vector>
#include <Camera/FlyingCamera.h>

#include "Octree.h"
#include "ThreadPool.h"

namespace DotAssets
{
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline Mesh_Sphere*& SphereMeshDef() { static Mesh_Sphere* m = new Mesh_Sphere(1, glm::vec3(1, 1, 1)); return m; }
	static inline Mesh_Sphere*& SphereMesh1() { static Mesh_Sphere* m = new Mesh_Sphere(1, glm::vec3(1, 0, 0)); return m; }
	static inline Mesh_Sphere*& SphereMesh2() { static Mesh_Sphere* m = new Mesh_Sphere(2, glm::vec3(0, 1, 0)); return m; }
	static inline Mesh_Sphere*& SphereMesh3() { static Mesh_Sphere* m = new Mesh_Sphere(3, glm::vec3(0, 0, 1)); return m; }
}

struct Dot
{
	Mesh_Sphere* mesh = nullptr;
	//Shader* shader;

	glm::vec3 position{};
	glm::vec3 velocity{};

	float radius = 0;
	int Health = 0;

	void Dot_Render()
	{
		mesh->Mesh_Render(DotAssets::SphereShader(), position);
	}

	void ReCreate()
	{
		//glm::vec3 color = glm::vec3(1, 1, 1);

		switch (Health)
		{
		case 1:
			//color = glm::vec3(1, 0, 0);
			mesh = DotAssets::SphereMesh1();
			radius = 1;
			break;
		case 2:
			//color = glm::vec3(0, 1, 0);
			mesh = DotAssets::SphereMesh2();
			radius = 2;
			break;
		case 3:
			//color = glm::vec3(0, 0, 1);
			mesh = DotAssets::SphereMesh3();
			radius = 3;
			break;
		default:
			mesh = DotAssets::SphereMeshDef();
			radius = 1;
			break;
		}

		//mesh = new Mesh_Sphere(radius, color);
		//shader = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl");
	}
};

class DotsScene
{
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

	void DotsScene_CreateCollisionTree();
	void DotsScene_HandleDotsCollisions();
	void DotsScene_HandleBoundsCollisions();
	void DotsScene_HandleDeadDots();

	float RandomFloat(float min, float max);

private:

	std::vector<Dot> dots;

	FlyingCamera* flyingCamera = nullptr;
	Octree collisionTree;
};