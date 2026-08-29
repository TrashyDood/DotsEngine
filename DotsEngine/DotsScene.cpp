#include "DotsScene.h"
#include "DebugLines.h"
#include "Utilities.h"
#include "tracy/Tracy.hpp"

#include <Time/Time.h>

#include <iostream>
#include <random>
#include <algorithm>

#include <glm/gtc/random.hpp>

#define LOG(x) std::cout << x << "\n";

DotsScene::DotsScene() : camera(FlyingCamera()) {}

DotsScene::~DotsScene()
{
	if (simulationTask.valid()) simulationTask.get();
	if (treeTask.valid()) treeTask.get();

	delete DotAssets::SphereShader();
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;

	DotsScene_ClearDotsData();
	renderContext.DeleteBuffers();
}

void DotsScene::DotsScene_Start(uint32_t dotsCount)
{
	if (simulationTask.valid()) simulationTask.get();
	if (treeTask.valid()) treeTask.get();

	LOG("Dots simulation started with " << dotsCount <<
		" dots and threaded simulation " << ((asyncSimulation) ? "on!" : "off!"));

	DotsScene::dotsCount = dotsCount;

	renderContext.Init(dotsCount);
	DotsScene_InitDotsData();

	camera.SetPosition(glm::vec3(0, 0, bounds * 2));
	camera.LookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}

void DotsScene::DotsScene_Tick()
{
	camera.Update();

	if (asyncSimulation)
		DotsScene_Simulate_Async();
	else
		DotsScene_Simulate();

	DotsScene_Render(renderContext);
}

void DotsScene::DotsScene_Render(RenderContext& context) const
{
	DotsScene_RenderMeshBuffers(context);
	DotsScene_DrawBounds();

	if (drawTree)
		for (auto& cell : readTree->Cells())
			if (cell.IsLeaf())
				DebugLines::DrawCube(
					cell.center,
					glm::quat(),
					glm::vec3(cell.halfExtent, cell.halfExtent, cell.halfExtent),
					glm::vec3(1, 1, 1));
}

void DotsScene::DotsScene_Simulate()
{	
	ZoneScopedNC("Simulation", HexColors::Purple);

	std::vector<std::pair<int, int>> collisionPairs;
	DotsScene_BuildAndQueryTree(collisionPairs, *writeTree);
	DotsScene_ResolveCollisions(collisionPairs);

	DotsScene_HandleBoundsCollisions();
	DotsScene_UpdateDotsData();

	DotsScene_UpdateTransformBuffers();
}

void DotsScene::DotsScene_Simulate_Async()
{
	if (simulationTask.valid()) simulationTask.get();

	simulationTask = StaticThreadPool().Submit(
		[this]() { DotsScene_Simulate(); });
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

void DotsScene::DotsScene_UpdateDotsData(bool parallel)
{
	{
		ZoneScopedNC("Update Dots Position", HexColors::Update);

		if (parallel)
			StaticThreadPool().For_Parallel<16>(dotsCount,
				[this](uint16_t thread, uint32_t chunkStart, size_t dot)
				{
					DotsScene_SetDotHealth(dot, dotsHealths[dot]);
					dotsPositions[dot] += glm::normalize(dotsVelocities[dot]) * (dotsSpeed / dotsRadii[dot]) * StaticTime().DeltaTime();
				});
		else
			for (size_t dot = 0; dot < dotsCount; dot++)
			{
				DotsScene_SetDotHealth(dot, dotsHealths[dot]);
				dotsPositions[dot] += glm::normalize(dotsVelocities[dot]) * (dotsSpeed / dotsRadii[dot]) * StaticTime().DeltaTime();
			}
	}
}

void DotsScene::DotsScene_UpdateTransformBuffers()
{
	ZoneScopedNC("Update Transform Buffers ", HexColors::Update);

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
void DotsScene::DotsScene_BuildAndQueryTree(std::vector<std::pair<int, int>>& pairs, CollisionTree& tree, bool async)
{
	if (async)
	{
		if (treeTask.valid()) treeTask.get();

		std::swap(readTree, writeTree);

		treeTask = StaticThreadPool().Submit(
			[this]()
			{
				ZoneScopedNC("Build And Query Collisions Async", HexColors::Collision);
				writeTree->Build(dotsPositions, dotsRadii,
					dotsCount, bounds, 15, 6, DotAssets::Meshes()[2]->Radius());


			});
	}
	else
	{
		ZoneScopedNC("Build And Query Collisions", HexColors::Collision);

		tree.Build(dotsPositions, dotsRadii,
			dotsCount, bounds, 15, 6, DotAssets::Meshes()[2]->Radius());
		tree.QueryPairs(pairs);
	}
}

void DotsScene::DotsScene_ResolveCollisions(std::vector<std::pair<int, int>>& pairs)
{
	ZoneScopedNC("Resolve Collisions", HexColors::Collision);
	for (auto pair : pairs)
		DotsScene_ResolveCollision(pair.first, pair.second);
}

void DotsScene::DotsScene_ResolveCollision(int dot1, int dot2)
{
	glm::vec3 diff = dotsPositions[dot1] - dotsPositions[dot2];
	float distSqr = glm::length2(diff);
	float radiusSum = dotsRadii[dot1] + dotsRadii[dot2];

	constexpr float minDistSqrEpsilon = 0.0001f;
	if (distSqr > radiusSum * radiusSum || distSqr < minDistSqrEpsilon)
		return;

	glm::vec3 normal = diff / std::sqrt(distSqr);

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
	ZoneScopedNC("Handle Bounds Collision", HexColors::Collision);

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
	ZoneScopedNC("Instanced Rendering", HexColors::Render);

	using namespace DotAssets;

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", camera.GetViewProjection());

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

void DotsScene::DotsScene_LeaderBoardMode(uint32_t dotsCount)
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = dotsCount * volumePerEntity;
	dotsSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start(dotsCount);
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}

void DotsScene::DotsScene_CollisionTest()
{
	DotsScene_Start(2);
	DotsScene_SetDotData(0, glm::vec3(8, 0, 0), glm::vec3(-1, 0, 0));
	DotsScene_SetDotData(1, glm::vec3(-8, 0, 0), glm::vec3(1, 0, 0));
	dotsSpeed = 50.f;
	bounds = 15.f;
}
#pragma endregion