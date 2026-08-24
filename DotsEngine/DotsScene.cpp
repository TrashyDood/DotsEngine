#include "DotsScene.h"
#include "DebugLines.h"

#include <Time/Time.h>

#include <iostream>
#include <random>
#include <algorithm>

#include <glm/gtc/random.hpp>

#include "tracy/Tracy.hpp"

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

	constexpr uint32_t UpdateMain = Green;
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
	isSimulating.store(false, std::memory_order_relaxed);
	if (simulationThread.valid()) simulationThread.get();

	delete mainCamera;
	delete flyingCamera;

	delete DotAssets::SphereShader();
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;

	DotsScene_ClearDotsData();
	renderContext.DeleteBuffers();
}

void DotsScene::DotsScene_Start(bool threadedSimulation, uint32_t dotsCount)
{
	LOG("Dots simulation started with " << dotsCount <<
		" dots and threaded simulation " << ((threadedSimulation) ? "on!" : "off!"));

	isSimulating.store(false, std::memory_order_relaxed);
	if (simulationThread.valid()) simulationThread.get();

	DotsScene::dotsCount = dotsCount;
	DotsScene::threadedSimulation = threadedSimulation;

	renderContext.Init(dotsCount);
	DotsScene_InitDotsData();

	if (!threadedSimulation) return;

	// open up a new thread for dots simulation
	isSimulating.store(true, std::memory_order_relaxed);
	simulationThread = GlobalThreadPool().Submit([this]()
	{
		tracy::SetThreadName("Simulation Thread");

		const float targetFrameTime = 1.f / 1000.f; // fps cap
		Time::sim_LastFrameTime = (float)glfwGetTime();

		while (isSimulating.load(std::memory_order_relaxed))
		{
			float currentTime = (float)glfwGetTime();
			Time::UpdateSimulation(currentTime);

			DotsScene_Simulate(Time::sim_DeltaTime);

			float elapsed = (float)glfwGetTime() - currentTime;
			while (elapsed < targetFrameTime)
				elapsed = (float)glfwGetTime() - currentTime;

			FrameMarkNamed("Threaded Simulation");
		}
	});
}

void DotsScene::DotsScene_Render(RenderContext& context) const
{
	DotsScene_RenderMeshBuffers_Threaded(context);
	DotsScene_DrawBounds();

	if (drawTree)
		collisionTree.DrawCell();
}

void DotsScene::DotsScene_Simulate(float deltaTime)
{
	DotsScene_HandleBoundsCollisions_Threaded();
	DotsScene_HandleCollisions_Threaded();

	DotsScene_UpdateDotData_Threaded(deltaTime);
}

void DotsScene::DotsScene_DrawBounds() const
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0),
		glm::quat(),
		glm::vec3(bounds + 5.f, bounds + 5.f, bounds + 5.f),
		glm::vec3(1, 1, 0));
}

#pragma region Dot Management
void DotsScene::DotsScene_InitDotsData()
{
	DotsScene_ClearDotsData(); // cleanup before init

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

void DotsScene::DotsScene_ClearDotsData()
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

void DotsScene::DotsScene_UpdateDotData_Threaded(float deltaTime)
{
	ZoneScopedNC("Update Dots Threaded", Colors::UpdateMain);

	constexpr int threadCount = 8;
	constexpr size_t maxPerThread = 100000; // must exceed the max dots any single thread's chunk could assign to one mesh
	static glm::mat4 localBuffers[threadCount][DotAssets::meshCount][maxPerThread];
	static uint32_t localCounts[threadCount][DotAssets::meshCount];
	memset(localCounts, 0, sizeof(localCounts));

	{
		ZoneScopedN("Write");

		GlobalThreadPool().ParallelFor<threadCount>(dotsCount, [this, deltaTime](int thread, uint32_t dot)
			{
				dotsPositions[dot] = dotsPositions[dot] + glm::normalize(dotsVelocities[dot]) *
					(dotSpeed / dotsRadii[dot]) * deltaTime;

				uint16_t mesh = dotsMeshIndices[dot];
				uint32_t slot = localCounts[thread][mesh]++;
				localBuffers[thread][mesh][slot] = glm::translate(glm::mat4(1.0f), dotsPositions[dot]);
			});
	}

	{
		ZoneScopedN("Merge");

		int writeSet = 1 - renderContext.frontIndex.load(std::memory_order_relaxed);
		for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
		{
			uint32_t offset = 0;
			for (int thread = 0; thread < threadCount; thread++)
			{
				uint32_t count = localCounts[thread][mesh];
				memcpy(renderContext.instanceTransformBuffers[writeSet][mesh] + offset,
					localBuffers[thread][mesh], count * sizeof(glm::mat4));
				offset += count;
			}
			renderContext.instanceCounts[writeSet][mesh] = offset;
		}

		renderContext.frontIndex.store(writeSet, std::memory_order_release);
	}
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

#pragma region Collision Management
void DotsScene::DotsScene_HandleCollisions_Threaded()
{
	ZoneScopedNC("Handle Collisions", Colors::Collision);

	{
		ZoneScopedN("Create Collision Tree Threaded");

		collisionTreeSOA.BuildRoot(dotsPositions, dotsRadii, dotsCount, bounds, 15);

		int childIndices[8];
		int childCount;
		collisionTreeSOA.GetRootChildren(childIndices, childCount);

		std::future<void> threadResults[8];

		for (int i = 0; i < childCount; i++)
		threadResults[i] = GlobalThreadPool().Submit([this, childIndices, i]()
		{
			collisionTreeSOA.BuildCell(childIndices[i], 1);
		});

		for (int i = 0; i < childCount; i++)
			threadResults[i].get();
	}

	std::vector<std::pair<int, int>> subtrees[8];
	int childCount;

	{
		ZoneScopedN("Query Collision Tree Threaded");

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
				threadResults[i] = GlobalThreadPool().Submit([this, childIndices, i, &subtrees]()
				{
					collisionTreeSOA.QueryCell(subtrees[i], childIndices[i]);
				});

			for (int i = 0; i < childCount; i++)
				threadResults[i].get();
		}
	}

	{
		ZoneScopedN("Resolve Collisions");

		for (int i = 0; i < childCount; i++)
			for (auto& pair : subtrees[i])
				DotsScene_ResolveCollision(pair.first, pair.second);
	}
}

void DotsScene::DotsScene_ResolveCollision(int dot1, int dot2)
{
	float radiusSum = dotsRadii[dot1] + dotsRadii[dot2];
	glm::vec3 diff = dotsPositions[dot1] - dotsPositions[dot2];
	float distSqr = glm::length2(diff);

	constexpr float minDistSqrEpsilon = 0.0001f;
	if (!(distSqr < radiusSum * radiusSum && distSqr > minDistSqrEpsilon))
		return;

	float dist = std::sqrt(distSqr);
	glm::vec3 normal = diff / dist;

	constexpr float positionCorrectionFactor = 0.1f;
	float overlap = radiusSum - dist;
	glm::vec3 correction = normal * (overlap * positionCorrectionFactor);
	dotsPositions[dot1] += correction;
	dotsPositions[dot2] -= correction;

	float velocityNormal1 = glm::dot(dotsVelocities[dot1], normal);
	float velocityNormal2 = glm::dot(dotsVelocities[dot2], normal);

	glm::vec3 before1 = dotsVelocities[dot1];

	dotsVelocities[dot1] += (velocityNormal2 - velocityNormal1) * normal;
	dotsVelocities[dot2] += (velocityNormal1 - velocityNormal2) * normal;

	DotsScene_OnDotCollide(dot1);
	DotsScene_OnDotCollide(dot2);
}

void DotsScene::DotsScene_OnDotCollide(int dot)
{
	if (DotsScene_SetDotHealth(dot, dotsHealths[dot] - 1))
		return;

	DotsScene_SetDotData(dot,
		{ RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds) },
		glm::sphericalRand(1.0f));
}

void DotsScene::DotsScene_HandleBoundsCollisions_Threaded()
{
	ZoneScopedNC("Handle Bounds Collision Threaded", Colors::Collision);

	GlobalThreadPool().ParallelFor<8>(dotsCount, [this](uint32_t thread, uint32_t dot)
	{
		float radius = dotsRadii[dot];

		if (dotsPositions[dot].x > bounds - radius || dotsPositions[dot].x < -bounds + radius) dotsVelocities[dot].x *= -1;
		if (dotsPositions[dot].y > bounds - radius || dotsPositions[dot].y < -bounds + radius) dotsVelocities[dot].y *= -1;
		if (dotsPositions[dot].z > bounds - radius || dotsPositions[dot].z < -bounds + radius) dotsVelocities[dot].z *= -1;

		dotsPositions[dot].x = glm::clamp(dotsPositions[dot].x, -bounds + radius, bounds - radius);
		dotsPositions[dot].y = glm::clamp(dotsPositions[dot].y, -bounds + radius, bounds - radius);
		dotsPositions[dot].z = glm::clamp(dotsPositions[dot].z, -bounds + radius, bounds - radius);
	});
}
#pragma endregion

#pragma region Rendering
void DotsScene::DotsScene_RenderMeshBuffers_Threaded(RenderContext& context) const
{
	ZoneScopedNC("Instanced Rendering Threaded", Colors::Render);

	using namespace DotAssets;

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", mainCamera->GetViewProjection());

	int readSet = context.frontIndex.load(std::memory_order_acquire);
	for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
	{
		Meshes()[mesh]->Mesh_UpdateInstanceData(context.instanceTransformBuffers[readSet][mesh], context.instanceCounts[readSet][mesh]);
		Meshes()[mesh]->Mesh_RenderInstanced(SphereShader(), context.instanceCounts[readSet][mesh]);
	}

	SphereShader()->Unbind();
}
#pragma endregion

#pragma region No Touch Zone
float DotsScene::RandomFloat(float min, float max) const
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(gen);
}

void DotsScene::DotsScene_LeaderBoardMode(bool threadedSimulation, uint32_t dotsCount)
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = dotsCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start(threadedSimulation, dotsCount);
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}
#pragma endregion