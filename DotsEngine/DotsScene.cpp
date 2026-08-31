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

thread_local bool inTracyAlloc = false;

void* operator new(size_t count)
{
	void* ptr = malloc(count);
	if (!inTracyAlloc)
	{
		inTracyAlloc = true;
		TracyAllocS(ptr, count, 10);
		inTracyAlloc = false;
	}
	return ptr;
}

void operator delete(void* ptr) noexcept
{
	if (!inTracyAlloc)
	{
		inTracyAlloc = true;
		TracyFreeS(ptr, 10);
		inTracyAlloc = false;
	}
	free(ptr);
}

DotsScene::DotsScene() : camera(FlyingCamera()) {}

DotsScene::~DotsScene()
{
	if (simulationTask.valid()) simulationTask.get();
	if (treeTask.valid()) treeTask.get();

	delete DotAssets::SphereShader();
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;
}

void DotsScene::DotsScene_Start(uint32_t dotsCount)
{
	if (simulationTask.valid()) simulationTask.get();
	if (treeTask.valid()) treeTask.get();

	LOG("Dots simulation started with " << dotsCount << " dots!");

	DotsScene::dotsCount = dotsCount;

	DotsScene_InitDotsData();
	DotsScene_InitRenderBuffers();

	readCollisionPairs->clear();
	writeCollisionPairs->clear();

	camera.SetPosition(glm::vec3(0, 0, bounds * 2));
	camera.LookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
}

void DotsScene::DotsScene_Tick()
{
	ZoneScopedNC("Tick", HexColors::Yellow);

	camera.Update();

	DotsScene_PopulateMeshBuffers_Async();
	DotsScene_Render();
	DotsScene_Simulate_Async();
}

void DotsScene::DotsScene_Simulate()
{
	ZoneScopedNC("Simulation", HexColors::Update);

	DotsScene_BuildAndQueryTree_Async();
	DotsScene_ResolveCollisions();

	DotsScene_HandleBoundsCollisions();
	DotsScene_UpdateDotsData();
}

void DotsScene::DotsScene_Simulate_Async()
{
	if (simulationTask.valid()) simulationTask.get();

	simulationTask = StaticThreadPool().Submit(
		[this]() { DotsScene_Simulate(); });
}

void DotsScene::DotsScene_Render() const
{
	ZoneScopedNC("Render", HexColors::Render);

	DotsScene_RenderMeshBuffers(*readTransfBuffers, *readTransfBufferOffsets);
	DotsScene_DrawBounds();
}

#pragma region Dot Management
void DotsScene::DotsScene_InitDotsData()
{
	dotsHealths = std::make_unique<int[]>(dotsCount);
	dotsMeshIndices = std::make_unique<int[]>(dotsCount);
	dotsRadii = std::make_unique<float[]>(dotsCount);
	dotsPositions = std::make_unique<glm::vec3[]>(dotsCount);
	dotsVelocities = std::make_unique<glm::vec3[]>(dotsCount);

	for (size_t dot = 0; dot < dotsCount; dot++)
		DotsScene_SetDotData(dot,
			{ RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds),
			RandomFloat(-bounds, bounds) },
			glm::sphericalRand(1.0f));
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
void DotsScene::DotsScene_BuildAndQueryTree_Async()
{
	if (treeTask.valid()) treeTask.get();

	std::swap(readTree, writeTree);
	std::swap(readCollisionPairs, writeCollisionPairs);

	treeTask = StaticThreadPool().Submit(
		[this]()
		{
			ZoneScopedNC("Build And Query Collisions Async", HexColors::Collision);
			writeTree->Build(dotsPositions.get(), dotsRadii.get(),
				dotsCount, bounds, 15, 6, DotAssets::Meshes()[2]->Radius());

			writeTree->QueryPairs(*writeCollisionPairs);
		});
}

void DotsScene::DotsScene_ResolveCollisions()
{
	ZoneScopedNC("Resolve Collisions", HexColors::Collision);
	for (auto& pair : *readCollisionPairs)
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
void DotsScene::DotsScene_InitRenderBuffers()
{
	for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
	{
		transfBuffersA = std::make_unique<glm::mat4[]>(dotsCount);
		transfBuffersB = std::make_unique<glm::mat4[]>(dotsCount);
	}

	transfBufferOffsetsA.fill(0);
	transfBufferOffsetsB.fill(0);
}

void DotsScene::DotsScene_PopulateMeshBuffers(TransformBuffer& transformBuffers,
	TransformBufferOffsets& bufferOffsets) const
{
	ZoneScopedNC("Populate Buffers ", HexColors::Update);

	uint32_t meshCounts[DotAssets::meshCount]{};
	for (size_t dot = 0; dot < dotsCount; dot++)
		meshCounts[dotsMeshIndices[dot]]++;

	uint32_t offset = 0;
	for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
	{
		bufferOffsets[mesh] = offset;
		offset += meshCounts[mesh];
	}

	bufferOffsets[DotAssets::meshCount] = offset;

	uint32_t cursor[DotAssets::meshCount];
	std::copy(std::begin(bufferOffsets), std::begin(bufferOffsets) + DotAssets::meshCount, std::begin(cursor));

	for (size_t dot = 0; dot < dotsCount; dot++)
	{
		uint32_t mesh = dotsMeshIndices[dot];
		transformBuffers[cursor[mesh]++] = glm::translate(glm::mat4(1.0f), dotsPositions[dot]);
	}
}

void DotsScene::DotsScene_PopulateMeshBuffers_Async()
{
	if (populateBuffersTask.valid()) populateBuffersTask.get();

	std::swap(readTransfBuffers, writeTransfBuffers);
	std::swap(readTransfBufferOffsets, writeTransfOffsetsBuffer);

	populateBuffersTask = StaticThreadPool().Submit(
		[this]()
		{
			ZoneScopedNC("Build And Query Collisions Async", HexColors::Render);
			DotsScene_PopulateMeshBuffers(*writeTransfBuffers, *writeTransfOffsetsBuffer);
		});
}

void DotsScene::DotsScene_RenderMeshBuffers(const TransformBuffer& transformBuffers,
	const TransformBufferOffsets& bufferOffsets) const
{
	ZoneScopedNC("Render Buffers", HexColors::Render);

	using namespace DotAssets;

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", camera.GetViewProjection());

	for (size_t mesh = 0; mesh < DotAssets::meshCount; mesh++)
	{
		uint32_t count = bufferOffsets[mesh + 1] - bufferOffsets[mesh];
		const glm::mat4* meshTransforms = transformBuffers.get() + bufferOffsets[mesh];

		Meshes()[mesh]->Mesh_UpdateInstanceData(meshTransforms, count);
		Meshes()[mesh]->Mesh_RenderInstanced(SphereShader(), count);
	}

	SphereShader()->Unbind();
}

void DotsScene::DotsScene_DrawBounds() const
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0),
		glm::quat(),
		glm::vec3(bounds + 5.f, bounds + 5.f, bounds + 5.f),
		glm::vec3(1, 1, 0));
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