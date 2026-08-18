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

void DotsScene::DotsScene_Start()
{
	LOG("started!");

	DotsScene_ClearDots();

	dotsCount = pendingDotCount;
	dots = new Dot[dotsCount];
	colliders = new Collider*[dotsCount];

	for (size_t i = 0; i < dotsCount; i++)
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), i);

	isRunning = true;
}

void DotsScene::DotsScene_Render()
{
	if (!isRunning) return;

	//DotsScene_NaiveDotRender();
	DotsScene_InstancedDotRender();
	DotsScene_DrawBounds();
}

void DotsScene::DotsScene_Update()
{
	if (!isRunning) return;

	flyingCamera->Update();

	//Handle velocity
	for (size_t i = 0; i < dotsCount; i++)
		dots[i].position = dots[i].position + glm::normalize(dots[i].velocity) *
		(dotSpeed / dots[i].radius) * Time::deltaTime;

	DotsScene_HandleBoundsCollisions();
	DotsScene_HandleDotsCollisions();

	//Look for nan's
	//for (size_t i = 0; i < dotsCount; i++)
	//	if (glm::any(glm::isnan(dots[i].position)))
	//		std::cout << "NAN FOUND IN DOT" << std::endl;
}

void DotsScene::DotsScene_DrawBounds()
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0), glm::quat(), glm::vec3(bounds, bounds, bounds), glm::vec3(1, 0, 0));
}

Dot DotsScene::DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.SetHealth(3);
	newDot.position = aPosition;
	newDot.velocity = aVelocity;

	return newDot;
}

Dot DotsScene::DotsScene_CreateRandomDot()
{
	return DotsScene_CreateDot({ RandomFloat(-bounds, bounds),
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

void DotsScene::DotsScene_ClearDots()
{
	delete[] dots;
	delete[] colliders;

	colliders = nullptr;
	dots = nullptr;
	dotsCount = 0;
}

void DotsScene::DotsScene_HandleDotsCollisions()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		collisionTree.Build(colliders, dotsCount, bounds);

		LOG("Overflow: " << collisionTree.OverflowCount() <<
			"(" << (float)collisionTree.OverflowCount() / dotsCount * 100 << "%)");
	}

	std::vector<std::vector<int>> nearbyPerDot(dotsCount);

	{
		ZoneScopedNC("Query Collisions", Colors::Collision);
		DotsScene_QueryCollisions_Threaded(nearbyPerDot);
	}

	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);

		for (size_t dot1 = 0; dot1 < dotsCount; dot1++)
			for (size_t dot2 : nearbyPerDot[dot1])
				if (dot2 > dot1)
					DotsScene_ResolveCollision(dot1, dot2);
	}
}

void DotsScene::DotsScene_QueryCollisions_Threaded(std::vector<std::vector<int>>& out)
{
	size_t chunkSize = (dotsCount + threadPool.threadCount - 1) / threadPool.threadCount;
	size_t futureCount = 0;

	std::future<void> threadResults[threadPool.threadCount];

	for (size_t start = 0; start < dotsCount; start += chunkSize)
	{
		size_t end = std::min(start + chunkSize, dotsCount);

		threadResults[futureCount++] = threadPool.Submit([this, start, end, &out]()
			{
				for (size_t dot1 = start; dot1 < end; dot1++)
					collisionTree.Query(&dots[dot1], out[dot1]);
			});
	}

	for (size_t i = 0; i < futureCount; i++)
		threadResults[i].get();
}

void DotsScene::DotsScene_ResolveCollision(int dotIndex1, int dotIndex2)
{
	float radiusSum = dots[dotIndex1].radius + dots[dotIndex2].radius;
	glm::vec3 diff = dots[dotIndex1].position - dots[dotIndex2].position;
	float distSqr = glm::length2(diff);

	if (!(distSqr < radiusSum * radiusSum && distSqr > 0.0001f))
		return;

	float dist = std::sqrt(distSqr);
	glm::vec3 normal = diff / dist;

	float overlap = radiusSum - dist;
	glm::vec3 correction = normal * (overlap * 0.5f * 0.2f);
	dots[dotIndex1].position += correction;
	dots[dotIndex2].position -= correction;

	dots[dotIndex1].velocity = glm::reflect(dots[dotIndex1].velocity, normal);
	dots[dotIndex2].velocity = glm::reflect(dots[dotIndex2].velocity, -normal);

	if (!dots[dotIndex1].SetHealth(dots[dotIndex1].Health - 1))
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), dotIndex1);

	if (!dots[dotIndex2].SetHealth(dots[dotIndex2].Health - 1))
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), dotIndex2);
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

void DotsScene::DotsScene_NaiveDotRender()
{
	ZoneScopedNC("Naive Rendering", Colors::Render);

	for (size_t i = 0; i < dotsCount; i++)
		dots[i].mesh->Mesh_RenderSingle(DotAssets::SphereShader(), dots[i].position);
}

void DotsScene::DotsScene_InstancedDotRender()
{
	ZoneScopedNC("Instanced Rendering", Colors::Render);

	using namespace DotAssets;

	for (auto& instances : meshModelInstances)
		instances.clear();

	// add a mesh instance for every dot
	for (size_t i = 0; i < dotsCount; i++)
		meshModelInstances[dots[i].Health - 1].push_back(glm::translate(glm::mat4(1.0f), dots[i].position));

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", mainCamera->GetViewProjection());
	SphereShader()->Unbind();

	// update mesh instances and then render them
	for (size_t i = 0; i < meshCount; i++)
	{
		Meshes()[i]->Mesh_UpdateInstanceData(meshModelInstances[i]);
		Meshes()[i]->Mesh_RenderInstanced(SphereShader(), meshModelInstances[i].size());
	}
}

float DotsScene::RandomFloat(float min, float max)
{
	static std::random_device rd;
	static std::mt19937 gen(rd());
	std::uniform_real_distribution<float> dist(min, max);
	return dist(gen);
}

DotsScene::~DotsScene()
{
	delete mainCamera;
	delete flyingCamera;
	delete DotAssets::SphereShader();

	DotsScene_ClearDots();

	for (auto& mesh : DotAssets::Meshes())
		delete mesh;
}

void DotsScene::DotsScene_LeaderBoardMode()
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	DotsScene_ClearDots();
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = pendingDotCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start();
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}