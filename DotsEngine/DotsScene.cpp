#include "DotsScene.h"
#include "DebugLines.h"
#include <Time/Time.h>
#include <iostream>

#include <random>
#include <glm/gtc/random.hpp>

#include "tracy/Tracy.hpp"

#include "DotsEditor.h"

#define LOG(x) std::cout << x << std::endl;

namespace Colors
{
	constexpr uint32_t Red = 0xFF0000;
	constexpr uint32_t Green = 0x00FF00;
	constexpr uint32_t Blue = 0x0000FF;
	constexpr uint32_t Yellow = 0xFFFF00;
	constexpr uint32_t Magenta = 0xFF00FF;
	constexpr uint32_t Cyan = 0x00FFFF;
	constexpr uint32_t Orange = 0xFFA500;
	constexpr uint32_t Purple = 0x800080;
	constexpr uint32_t Pink = 0xFFC0CB;
	constexpr uint32_t Brown = 0xA52A2A;
	constexpr uint32_t White = 0xFFFFFF;
	constexpr uint32_t Black = 0x000000;
	constexpr uint32_t Gray = 0x808080;
	constexpr uint32_t LightGray = 0xD3D3D3;
	constexpr uint32_t DarkGray = 0x404040;
	constexpr uint32_t Teal = 0x008080;
	constexpr uint32_t Navy = 0x000080;
	constexpr uint32_t Lime = 0x32CD32;
	constexpr uint32_t Gold = 0xFFD700;
	constexpr uint32_t Indigo = 0x4B0082;
	constexpr uint32_t Maroon = 0x800000;
	constexpr uint32_t Olive = 0x808000;
	constexpr uint32_t Turquoise = 0x40E0D0;
	constexpr uint32_t Salmon = 0xFA8072;
	constexpr uint32_t Coral = 0xFF7F50;

	constexpr uint32_t Update = Green;
	constexpr uint32_t Render = Blue;
	constexpr uint32_t Collision = Red;
	constexpr uint32_t Spawn = Gold;
	constexpr uint32_t Cleanup = Gray;
}

Camera* DotsScene::mainCamera;

DotsScene::DotsScene()
{
	mainCamera = new Camera();
	flyingCamera = new FlyingCamera(mainCamera);

	mainCamera->SetPosition(glm::vec3(0, 0, bounds * 4));
}

DotsScene::~DotsScene()
{
	delete mainCamera;
	delete flyingCamera;

	delete DotAssets::SphereShader();
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;

	DotsScene_DeleteDots(); //AoS
	DotsScene_DeleteDotsData(); //SoA
	DotsScene_DeleteMeshBuffers();
}

void DotsScene::DotsScene_Start()
{
	LOG("Dots simulation started!");

	dotsCount = pendingDotsCount;
	DotsScene_InitMeshBuffers();

	DotsScene_InitDotsData();

	isSimulating = true;
}

void DotsScene::DotsScene_Render()
{
	DotsScene_RenderMeshBuffers();
	DotsScene_ClearInstanceBuffers();

	DotsScene_DrawBounds();

	if (drawTree)
		collisionTree.DrawCell();
}

void DotsScene::DotsScene_Update()
{
	if (!isSimulating) return;

	flyingCamera->Update();

	DotsScene_HandleBoundsCollisionsSOA();
	DotsScene_HandleCollisionsSOA_Threaded();

	for (size_t dot = 0; dot < dotsCount; dot++)
		DotsScene_UpdateDotData(dot, Time::deltaTime);
}

void DotsScene::DotsScene_DrawBounds()
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0),
		glm::quat(),
		glm::vec3(bounds + 5.f, bounds + 5.f, bounds + 5.f),
		glm::vec3(1, 1, 0));
}

void DotsScene::DotsScene_Pause() { isSimulating = false; }
void DotsScene::DotsScene_Resume() { isSimulating = true; }

#pragma region AoS Dot Management
void DotsScene::DotsScene_InitDots()
{
	DotsScene_DeleteDots();

	dots = new Dot[dotsCount];
	colliders = new Collider* [dotsCount];

	for (size_t i = 0; i < dotsCount; i++)
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), i);
}

void DotsScene::DotsScene_DeleteDots()
{
	delete[] dots;
	delete[] colliders;

	colliders = nullptr;
	dots = nullptr;
}

void DotsScene::DotsScene_UpdateDot(int dot, float deltaTime)
{
	// velocity
	dots[dot].position = dots[dot].position + glm::normalize(dots[dot].velocity) *
		(dotSpeed / dots[dot].radius) * deltaTime;

	DotsScene_AddMeshInstance(dots[dot].meshIndex, dots[dot].position);
}

Dot DotsScene::DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.SetMesh(2);
	newDot.position = aPosition;
	newDot.velocity = aVelocity;

	return newDot;
}

Dot DotsScene::DotsScene_CreateRandomDot()
{
	return DotsScene_CreateDot({
		RandomFloat(-bounds, bounds),
		RandomFloat(-bounds, bounds),
		RandomFloat(-bounds, bounds) },
		glm::sphericalRand(1.0f));
}

void DotsScene::DotsScene_ReplaceDot(const Dot& dot, size_t index)
{
	if (index >= dotsCount)
		return;

	dots[index] = dot;
	colliders[index] = &dots[index];
}
#pragma endregion

#pragma region AoS Collision Management
void DotsScene::DotsScene_HandleCollisions()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		collisionTree.BuildRoot(colliders, dotsCount, bounds, 15, DotAssets::Meshes()[2]->Radius());
		collisionTree.BuildCell(0, 0);
	}

	int childCount;
	std::vector<std::pair<Collider*, Collider*>> pairs;

	{
		ZoneScopedNC("Query Collision Tree", Colors::Collision);
		collisionTree.QueryCell(pairs); // starts at root (default cellIndex = 0), handles leaf-root case too
	}

	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);

		for (auto& pair : pairs)
			DotsScene_ResolveCollision(static_cast<Dot*>(pair.first) - dots,
				static_cast<Dot*>(pair.second) - dots);
	}
}

void DotsScene::DotsScene_HandleCollisions_Threaded()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		//collisionTree2.BuildRoot(colliders, dotsCount, bounds, 15, DotAssets::Meshes()[2]->Radius());

		collisionTree.BuildRoot(colliders, dotsCount, bounds, 15);
		collisionTree.BuildCell(0, 0);

		int childIndices[8];
		int childCount;
		collisionTree.GetRootChildren(childIndices, childCount);

		std::future<void> threadResults[8];

		for (int i = 0; i < childCount; i++)
			threadResults[i] = threadPool.Submit([this, childIndices, i]()
				{
					collisionTree.BuildCell(childIndices[i], 1);
				});

		for (int i = 0; i < childCount; i++)
			threadResults[i].get();
	}

	std::vector<std::pair<Collider*, Collider*>> subtrees[8];
	int childCount;

	{
		ZoneScopedNC("Query Collision Tree", Colors::Collision);

		//collisionTree2.QueryCell(pairs);

		int childIndices[8];
		collisionTree.GetRootChildren(childIndices, childCount);

		if (childCount == 0)
		{
			// root never split — query it directly, single-threaded
			collisionTree.QueryCell(subtrees[0], 0);
			childCount = 1;
		}
		else
		{
			std::future<void> threadResults[8];
			for (int i = 0; i < childCount; i++)
				threadResults[i] = threadPool.Submit([this, childIndices, i, &subtrees]() {
				collisionTree.QueryCell(subtrees[i], childIndices[i]);
					});

			for (int i = 0; i < childCount; i++)
				threadResults[i].get();
		}
	}

	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);

		for (int i = 0; i < childCount; i++)
			for (auto& pair : subtrees[i])
				DotsScene_ResolveCollision(static_cast<Dot*>(pair.first) - dots,
					static_cast<Dot*>(pair.second) - dots);
	}
}

void DotsScene::DotsScene_ResolveCollision(int dot1, int dot2)
{
	float radiusSum = dots[dot1].radius + dots[dot2].radius;
	glm::vec3 diff = dots[dot1].position - dots[dot2].position;
	float distSqr = glm::length2(diff);

	if (!(distSqr < radiusSum * radiusSum && distSqr > 0.0001f))
		return;

	float dist = std::sqrt(distSqr);
	glm::vec3 normal = diff / dist;

	float overlap = radiusSum - dist;
	glm::vec3 correction = normal * (overlap * 0.5f * 0.2f);
	dots[dot1].position += correction;
	dots[dot2].position -= correction;

	dots[dot1].velocity = glm::reflect(dots[dot1].velocity, normal);
	dots[dot2].velocity = glm::reflect(dots[dot2].velocity, -normal);

	DotsScene_OnDotCollide(dot1);
	DotsScene_OnDotCollide(dot2);
}

void DotsScene::DotsScene_OnDotCollide(int dot)
{
	dots[dot].health--;
	if (dots[dot].health <= 0)
	{
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), dot);
		return;
	}

	dots[dot].SetMesh(dots[dot].health - 1);
}

void DotsScene::DotsScene_HandleBoundsCollisions()
{
	ZoneScopedNC("Handle Bounds Collision", Colors::Collision);

	for (size_t i = 0; i < dotsCount; i++)
	{
		Dot& dot = dots[i];

		if (dot.position.x > bounds - dot.radius || dot.position.x < -bounds + dot.radius) dot.velocity.x *= -1;
		if (dot.position.y > bounds - dot.radius || dot.position.y < -bounds + dot.radius) dot.velocity.y *= -1;
		if (dot.position.z > bounds - dot.radius || dot.position.z < -bounds + dot.radius) dot.velocity.z *= -1;

		dot.position.x = glm::clamp(dot.position.x, -bounds + dot.radius, bounds - dot.radius);
		dot.position.y = glm::clamp(dot.position.y, -bounds + dot.radius, bounds - dot.radius);
		dot.position.z = glm::clamp(dot.position.z, -bounds + dot.radius, bounds - dot.radius);
	}
}
#pragma endregion

#pragma region SoA Dot Management
void DotsScene::DotsScene_InitDotsData()
{
	DotsScene_DeleteDotsData();

	dotsHealths = new int[dotsCount];
	dotsPositions = new glm::vec3[dotsCount];
	dotsVelocities = new glm::vec3[dotsCount];
	dotsRadii = new float[dotsCount];
	dotsMeshIndices = new int[dotsCount];

	for (size_t dot = 0; dot < dotsCount; dot++)
		DotsScene_SetDotData(dot,
			{ RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds) },
			glm::sphericalRand(1.0f));
}

void DotsScene::DotsScene_DeleteDotsData()
{
	delete[] dotsHealths;
	delete[] dotsPositions;
	delete[] dotsVelocities;
	delete[] dotsRadii;
	delete[] dotsMeshIndices;

	dotsHealths = nullptr;
	dotsPositions = nullptr;
	dotsVelocities = nullptr;
	dotsRadii = nullptr;
	dotsMeshIndices = nullptr;
}

void DotsScene::DotsScene_UpdateDotData(int dot, float deltaTime)
{
	dotsPositions[dot] = dotsPositions[dot] + glm::normalize(dotsVelocities[dot]) *
		(dotSpeed / dotsRadii[dot]) * deltaTime;

	DotsScene_AddMeshInstance(dotsMeshIndices[dot], dotsPositions[dot]);
}

void DotsScene::DotsScene_SetDotData(int dot, glm::vec3 position, glm::vec3 velocity, int health)
{
	DotsScene_SetDotHealth(dot, health);
	dotsPositions[dot] = position;
	dotsVelocities[dot] = velocity;
}

bool DotsScene::DotsScene_SetDotHealth(int dot, int health)
{
	dotsHealths[dot] = health;
	dotsMeshIndices[dot] = std::max(0, std::min(DotAssets::meshCount - 1, health - 1));
	dotsRadii[dot] = DotAssets::Meshes()[dotsMeshIndices[dot]]->Radius();

	return dotsHealths[dot] > 0;
}
#pragma endregion

#pragma region SoA Collision Management
void DotsScene::DotsScene_HandleCollisionsSOA_Threaded()
{
	{
		ZoneScopedNC("Create Collision Tree Threaded SoA", Colors::Collision);

		collisionTreeSOA.BuildRoot(dotsPositions, dotsRadii, dotsCount, bounds, 15);
		collisionTreeSOA.BuildCell(0, 0);

		int childIndices[8];
		int childCount;
		collisionTreeSOA.GetRootChildren(childIndices, childCount);

		std::future<void> threadResults[8];

		for (int i = 0; i < childCount; i++)
			threadResults[i] = threadPool.Submit([this, childIndices, i]()
				{
					collisionTreeSOA.BuildCell(childIndices[i], 1);
				});

		for (int i = 0; i < childCount; i++)
			threadResults[i].get();
	}

	std::vector<std::pair<int, int>> subtrees[8];
	int childCount;

	{
		ZoneScopedNC("Query Collision Tree Threaded SoA", Colors::Collision);

		int childIndices[8];
		collisionTreeSOA.GetRootChildren(childIndices, childCount);

		if (childCount == 0)
		{
			// root never split — query it directly, single-threaded
			collisionTreeSOA.QueryCell(subtrees[0], 0);
			childCount = 1;
		}
		else
		{
			std::future<void> threadResults[8];
			for (int i = 0; i < childCount; i++)
				threadResults[i] = threadPool.Submit([this, childIndices, i, &subtrees]() {
				collisionTreeSOA.QueryCell(subtrees[i], childIndices[i]);
					});

			for (int i = 0; i < childCount; i++)
				threadResults[i].get();
		}
	}

	{
		ZoneScopedNC("Resolve Collisions SoA", Colors::Collision);

		for (int i = 0; i < childCount; i++)
			for (auto& pair : subtrees[i])
				DotsScene_ResolveCollisionSOA(pair.first, pair.second);
	}
}

void DotsScene::DotsScene_ResolveCollisionSOA(int dot1, int dot2)
{
	float radiusSum = dotsRadii[dot1] + dotsRadii[dot2];
	glm::vec3 diff = dotsPositions[dot1] - dotsPositions[dot2];
	float distSqr = glm::length2(diff);

	if (!(distSqr < radiusSum * radiusSum && distSqr > 0.0001f))
		return;

	float dist = std::sqrt(distSqr);
	glm::vec3 normal = diff / dist;

	float overlap = radiusSum - dist;
	glm::vec3 correction = normal * (overlap * 0.5f * 0.2f);
	dotsPositions[dot1] += correction;
	dotsPositions[dot2] -= correction;

	dotsVelocities[dot1] = glm::reflect(dotsVelocities[dot1], normal);
	dotsVelocities[dot2] = glm::reflect(dotsVelocities[dot2], -normal);

	DotsScene_OnDotCollideSOA(dot1);
	DotsScene_OnDotCollideSOA(dot2);
}

void DotsScene::DotsScene_OnDotCollideSOA(int dot)
{
	if (!DotsScene_SetDotHealth(dot, dotsHealths[dot] - 1))
	{
		DotsScene_SetDotData(dot,
			{ RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds) },
			glm::sphericalRand(1.0f));
		return;
	}
}

void DotsScene::DotsScene_HandleBoundsCollisionsSOA()
{
	ZoneScopedNC("Handle Bounds Collision SoA", Colors::Collision);

	for (size_t i = 0; i < dotsCount; i++)
	{
		float radius = dotsRadii[i];

		if (dotsPositions[i].x > bounds - radius || dotsPositions[i].x < -bounds + radius) dotsVelocities[i].x *= -1;
		if (dotsPositions[i].y > bounds - radius || dotsPositions[i].y < -bounds + radius) dotsVelocities[i].y *= -1;
		if (dotsPositions[i].z > bounds - radius || dotsPositions[i].z < -bounds + radius) dotsVelocities[i].z *= -1;

		dotsPositions[i].x = glm::clamp(dotsPositions[i].x, -bounds + radius, bounds - radius);
		dotsPositions[i].y = glm::clamp(dotsPositions[i].y, -bounds + radius, bounds - radius);
		dotsPositions[i].z = glm::clamp(dotsPositions[i].z, -bounds + radius, bounds - radius);
	}
}
#pragma endregion

#pragma region Rendering
void DotsScene::DotsScene_InitMeshBuffers()
{
	DotsScene_DeleteMeshBuffers();

	for (size_t i = 0; i < DotAssets::meshCount; i++)
	{
		mInstances[i] = new glm::mat4[dotsCount];
		instanceCounts[i] = new int(0);
	}
}

void DotsScene::DotsScene_DeleteMeshBuffers()
{
	for (size_t i = 0; i < DotAssets::meshCount; i++)
	{
		delete[] mInstances[i];
		delete[] instanceCounts[i];

		mInstances[i] = nullptr;
		instanceCounts[i] = nullptr;
	}
}

void DotsScene::DotsScene_RenderMeshBuffers()
{
	ZoneScopedNC("Instanced Rendering", Colors::Render);

	using namespace DotAssets;

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", mainCamera->GetViewProjection());

	// update mesh instances and then render them
	for (size_t mesh = 0; mesh < meshCount; mesh++)
	{
		Meshes()[mesh]->Mesh_UpdateInstanceData(mInstances[mesh], *instanceCounts[mesh]);
		Meshes()[mesh]->Mesh_RenderInstanced(SphereShader(), *instanceCounts[mesh]);
	}

	SphereShader()->Unbind();
}

void DotsScene::DotsScene_ClearInstanceBuffers()
{
	for (size_t i = 0; i < DotAssets::meshCount; i++)
		(*instanceCounts[i]) = 0;
}

void DotsScene::DotsScene_AddMeshInstance(const size_t meshIndex, const glm::vec3& position)
{
	if (meshIndex >= DotAssets::meshCount)
		return;

	mInstances[meshIndex][*instanceCounts[meshIndex]] = glm::translate(glm::mat4(1.0f), position);
	(*instanceCounts[meshIndex])++;
}
#pragma endregion

#pragma region No Touch Zone
float DotsScene::RandomFloat(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(gen);
}

void DotsScene::DotsScene_LeaderBoardMode()
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = pendingDotsCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start();
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}
#pragma endregion