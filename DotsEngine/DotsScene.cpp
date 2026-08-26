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
	// wait for sim thread to close
	isSimulating.store(false, std::memory_order_relaxed);
	if (simulationThread.valid()) simulationThread.get();

	// wait for build thread to close
	if (buildTask.valid()) buildTask.get();

	delete mainCamera;
	delete flyingCamera;

	delete DotAssets::SphereShader();
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;

	DotsScene_ClearDotsData();
	renderContext.DeleteBuffers();
}

void DotsScene::DotsScene_Start(uint32_t dotsCount, bool threadedSimulation)
{
	// if there is a previous sim thread active, close it
	isSimulating.store(false, std::memory_order_relaxed);
	if (simulationThread.valid()) simulationThread.get();

	// if there is a previous build thread active, close it
	if (buildTask.valid()) buildTask.get();


	LOG("Dots simulation started with " << dotsCount <<
		" dots and threaded simulation " << ((threadedSimulation) ? "on!" : "off!"));


	DotsScene::dotsCount = dotsCount;
	DotsScene::threadedSimulation = threadedSimulation;

	renderContext.Init(dotsCount);
	DotsScene_InitDotsData();

	DotsScene_BuildCollisionTree_Async(); // start building the first tree with dots data

	if (!threadedSimulation) return;

	// open up a new thread for dots simulation
	isSimulating.store(true, std::memory_order_relaxed);
	simulationThread = StaticThreadPool().Submit([this]()
	{
		tracy::SetThreadName("Simulation Thread");

		const float targetFrameTime = 1.f / 60.f; // fps cap
		Time::sim_LastFrameTime = (float)glfwGetTime();

		while (isSimulating.load(std::memory_order_relaxed))
		{
			float currentTime = (float)glfwGetTime();
			Time::UpdateSimulation(currentTime);

			DotsScene_Simulate(Time::sim_DeltaTime);

			float elapsed = (float)glfwGetTime() - currentTime;
			while (elapsed < targetFrameTime)
				elapsed = (float)glfwGetTime() - currentTime;

			FrameMarkNamed("Simulation");
		}
	});
}

void DotsScene::DotsScene_Render(RenderContext& context) const
{
	DotsScene_RenderMeshBuffers(context);
	DotsScene_DrawBounds();

	//if (drawTree)
	//	readTree->DrawCell();
}

void DotsScene::DotsScene_Simulate(float deltaTime)
{	
	DotsScene_HandleBoundsCollisions();
	DotsScene_UpdateDotsData(deltaTime);

	DotsScene_BuildCollisionTree_Async(); // starts building this frame's tree asynchronously
	DotsScene_HandleCollisions();

	DotsScene_UpdateTransformBuffers(); // updates transform buffers for rendering
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

void DotsScene::DotsScene_UpdateDotsData(float deltaTime)
{
	{
		ZoneScopedNC("Update Dots Position", Colors::UpdateMain);

		StaticThreadPool().For_Parallel<16>(dotsCount,
			[this, deltaTime](uint32_t thread, uint32_t chunkStart, uint32_t dot)
			{
				DotsScene_SetDotHealth(dot, dotsHealths[dot]);
				dotsPositions[dot] += glm::normalize(dotsVelocities[dot]) * (dotSpeed / dotsRadii[dot]) * deltaTime;
			});
	}
}

void DotsScene::DotsScene_UpdateTransformBuffers()
{
	ZoneScopedNC("Update Transform Buffers ", Colors::UpdateMain);

	constexpr uint32_t threadCount = 32;

	uint32_t counts[threadCount][DotAssets::meshCount]{};

	{
		StaticThreadPool().For_Parallel<threadCount>(dotsCount,
			[this, &counts](uint32_t thread, uint32_t chunkStart, uint32_t dot)
			{
				counts[thread][dotsMeshIndices[dot]]++;
			});
	}

	uint32_t offsets[threadCount][DotAssets::meshCount];
	uint32_t totals[DotAssets::meshCount]{};

	{
		for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
		{
			uint32_t running = 0;
			for (uint32_t t = 0; t < threadCount; t++)
			{
				offsets[t][mesh] = running;
				running += counts[t][mesh];
			}
			totals[mesh] = running;
		}
	}

	int writeSet = renderContext.GetWriteBufferIndex();

	{
		StaticThreadPool().For_Parallel<threadCount>(dotsCount,
			[this, writeSet, &offsets](uint32_t thread, uint32_t chunkStart, uint32_t dot)
			{
				uint16_t mesh = dotsMeshIndices[dot];
				uint32_t slot = offsets[thread][mesh]++;
				renderContext.transformBuffers[writeSet][mesh][slot] = glm::translate(glm::mat4(1.0f), dotsPositions[dot]);
			});

		for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
			renderContext.countBuffers[writeSet][mesh] = totals[mesh];
	}

	renderContext.SwapBuffers(writeSet);
}

void DotsScene::DotsScene_SetDotData(int dot, glm::vec3 position, glm::vec3 velocity, int health)
{
	DotsScene_SetDotHealth(dot, health);
	dotsPositions[dot] = position;
	dotsVelocities[dot] = velocity;
}

void DotsScene::DotsScene_SetDotHealth(int dot, int health)
{
	dotsHealths[dot] = health;
	dotsMeshIndices[dot] = std::max(0, std::min(DotAssets::meshCount - 1, health - 1));
	dotsRadii[dot] = DotAssets::Meshes()[dotsMeshIndices[dot]]->Radius();

	if (dotsHealths[dot] > 0) return;

	DotsScene_SetDotData(dot,
		{ RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds) },
		glm::sphericalRand(1.0f));
}
#pragma endregion

#pragma region Collision Management
void DotsScene::DotsScene_BuildCollisionTree_Async()
{
	//{
	//	ZoneScopedNC("Build Tree", Colors::Collision);
	//	readTree->Build(dotsPositions, dotsRadii, dotsCount, bounds, 25);
	//}

	//return;

	if (buildTask.valid()) // wait for the previous tree to finish building
		buildTask.get();

	std::swap(readTree, writeTree);

	buildTask = StaticThreadPool().Submit(
	[this]()
	{
		ZoneScopedNC("Build Tree", Colors::Collision);
		writeTree->Build(dotsPositions, dotsRadii, dotsCount, bounds, 15);
	});
}

void DotsScene::DotsScene_HandleCollisions()
{
	std::vector<std::pair<int, int>> pairs;

	{
		ZoneScopedNC("Query Collisions", Colors::Collision);
		readTree->QueryPairs(pairs);
	}

	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);
		for (auto pair : pairs)
			DotsScene_ResolveCollision(pair.first, pair.second);
	}

	//std::vector<std::pair<int, int>> subtrees[8];
	//int childCount;

	//{
	//	ZoneScopedNC("Query Tree", Colors::Collision);

	//	int childIndices[8];
	//	readTree->GetRootChildren(childIndices, childCount);

	//	if (childCount == 0)
	//	{
	//		readTree->QueryCell(subtrees[0], 0);
	//		childCount = 1;
	//	}
	//	else
	//	{
	//		std::future<void> threadResults[8];
	//		for (int i = 0; i < childCount; i++)
	//			threadResults[i] = StaticThreadPool().Submit(
	//				[this, &subtrees, childIndices, i]()
	//				{
	//					readTree->QueryCell(subtrees[i], childIndices[i]);
	//				});

	//		for (int i = 0; i < childCount; i++)
	//			threadResults[i].get();
	//	}
	//}

	//{
	//	ZoneScopedNC("Resolve Collitions", Colors::Collision);

	//	for (int i = 0; i < childCount; i++)
	//		for (auto& pair : subtrees[i])
	//			DotsScene_ResolveCollision(pair.first, pair.second);
	//}
}

void DotsScene::DotsScene_ResolveCollision(int dot1, int dot2)
{
	glm::vec3 diff = dotsPositions[dot1] - dotsPositions[dot2];
	float distSqr = glm::length2(diff);
	float radiusSum = dotsRadii[dot1] + dotsRadii[dot2];

	constexpr float minDistSqrEpsilon = 0.0001f;
	if (distSqr > radiusSum * radiusSum || distSqr < minDistSqrEpsilon)
		return;

	float dist = std::sqrt(distSqr);
	glm::vec3 normal = diff / dist;

	// push apart so they're not still overlapping next frame
	//glm::vec3 correction = normal * (radiusSum - dist);
	//dotsPositions[dot1] += correction * 0.5f;
	//dotsPositions[dot2] -= correction * 0.5f;

	dotsVelocities[dot1] -= 2.0f * glm::dot(dotsVelocities[dot1], normal) * normal;
	dotsVelocities[dot2] -= 2.0f * glm::dot(dotsVelocities[dot2], normal) * normal;

	dotsHealths[dot1]--;
	dotsHealths[dot2]--;
}

void DotsScene::DotsScene_HandleBoundsCollisions()
{
	ZoneScopedNC("Handle Bounds Collision", Colors::Collision);

	StaticThreadPool().For_Parallel<12>(dotsCount,
		[this](uint32_t thread, uint32_t chunkStart, uint32_t dot)
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
void DotsScene::DotsScene_DrawBounds() const
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0),
		glm::quat(),
		glm::vec3(bounds + 5.f, bounds + 5.f, bounds + 5.f),
		glm::vec3(1, 1, 0));
}

void DotsScene::DotsScene_RenderMeshBuffers(RenderContext& context) const
{
	ZoneScopedNC("Instanced Rendering", Colors::Render);

	using namespace DotAssets;

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", mainCamera->GetViewProjection());

	int readSet = context.GetReadBufferIndex();
	for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
	{
		Meshes()[mesh]->Mesh_UpdateInstanceData(context.transformBuffers[readSet][mesh], context.countBuffers[readSet][mesh]);
		Meshes()[mesh]->Mesh_RenderInstanced(SphereShader(), context.countBuffers[readSet][mesh]);
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

void DotsScene::DotsScene_LeaderBoardMode(uint32_t dotsCount, bool threadedSimulation)
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = dotsCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start(dotsCount, threadedSimulation);
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}
#pragma endregion