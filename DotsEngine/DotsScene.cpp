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
	LOG("Dots simulation started!");

	DotsScene_DeleteDots();

	dotsCount = pendingDotsCount;
	dots = new Dot[dotsCount];
	colliders = new Collider*[dotsCount];

	for (size_t i = 0; i < dotsCount; i++)
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), i);

	isSimulating = true;
}

void DotsScene::DotsScene_Pause()
{
	isSimulating = false;
}

void DotsScene::DotsScene_Resume()
{
	isSimulating = true;
}

void DotsScene::DotsScene_Render()
{
	DotsScene_InstancedDotRender();
	DotsScene_ClearInstanceBuffers();

	DotsScene_DrawBounds();

	if (renderTree)
		collisionTree2.DrawCell();
}

void DotsScene::DotsScene_Update()
{
	if (!isSimulating) return;

	flyingCamera->Update();

	//Handle velocity
	for (size_t i = 0; i < dotsCount; i++)
		dots[i].position = dots[i].position + glm::normalize(dots[i].velocity) *
		(dotSpeed / dots[i].radius) * Time::deltaTime;

	DotsScene_HandleBoundsCollisions();

	//DotsScene_HandleDotsCollisions();
	//DotsScene_HandleDotsCollisions2();
	DotsScene_HandleCollisions2_Threaded();

	//Look for nan's
	//for (size_t i = 0; i < dotsCount; i++)
	//	if (glm::any(glm::isnan(dots[i].position)))
	//		std::cout << "NAN FOUND IN DOT" << std::endl;
}

void DotsScene::DotsScene_DrawBounds()
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0),
		glm::quat(),
		glm::vec3(bounds + 5.f, bounds + 5.f, bounds + 5.f),
		glm::vec3(1, 1, 0));
}

Dot DotsScene::DotsScene_CreateDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.SetMesh(DotAssets::Meshes()[2]);
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

void DotsScene::DotsScene_DeleteDots()
{
	delete[] dots;
	delete[] colliders;

	colliders = nullptr;
	dots = nullptr;
	dotsCount = 0;
}

void DotsScene::DotsScene_HandleCollisions()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		uint32_t overflowCount = 0;
		collisionTree.Build(colliders, dotsCount, bounds, overflowCount);
	}

	std::vector<std::vector<int>> nearbyPerDot(dotsCount);

	{
		ZoneScopedNC("Query Collisions", Colors::Collision);

		uint32_t chunkSize = (dotsCount + threadPool.threadCount - 1) / threadPool.threadCount;
		uint32_t futureCount = 0;

		std::future<void> threadResults[threadPool.threadCount];

		for (uint32_t start = 0; start < dotsCount; start += chunkSize)
		{
			uint32_t end = std::min(start + chunkSize, dotsCount);

			threadResults[futureCount++] = threadPool.Submit([this, start, end, &nearbyPerDot]()
			{
				for (uint32_t dot1 = start; dot1 < end; dot1++)
					collisionTree.Query(&dots[dot1], nearbyPerDot[dot1]);
			});
		}

		for (uint32_t i = 0; i < futureCount; i++)
			threadResults[i].get();
	}
	
	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);
		for (size_t dot1 = 0; dot1 < dotsCount; dot1++)
		{
			for (size_t dot2 : nearbyPerDot[dot1])
			{
				if (dot2 > dot1)
					DotsScene_ResolveCollision(dot1, dot2);
			}
		}
	}
}

void DotsScene::DotsScene_HandleCollisions2()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		collisionTree2.BuildRoot(colliders, dotsCount, bounds, 15, DotAssets::Meshes()[2]->Radius());
		collisionTree2.BuildCell(0, 0);
	}

	int childCount;
	std::vector<std::pair<Collider*, Collider*>> pairs;

	{
		ZoneScopedNC("Query Collision Tree", Colors::Collision);
		collisionTree2.QueryCell(pairs); // starts at root (default cellIndex = 0), handles leaf-root case too
	}

	{
		ZoneScopedNC("Resolve Collisions", Colors::Collision);

		for (auto& pair : pairs)
			DotsScene_ResolveCollision(static_cast<Dot*>(pair.first) - dots,
				static_cast<Dot*>(pair.second) - dots);
	}
}

void DotsScene::DotsScene_HandleCollisions2_Threaded()
{
	{
		ZoneScopedNC("Create Collision Tree", Colors::Collision);

		//collisionTree2.BuildRoot(colliders, dotsCount, bounds, 15, DotAssets::Meshes()[2]->Radius());

		collisionTree2.BuildRoot(colliders, dotsCount, bounds, 15);
		collisionTree2.BuildCell(0, 0);

		int childIndices[8];
		int childCount;
		collisionTree2.GetRootChildren(childIndices, childCount);

		std::future<void> threadResults[8];

		for (int i = 0; i < childCount; i++)
			threadResults[i] = threadPool.Submit([this, childIndices, i]()
				{
					collisionTree2.BuildCell(childIndices[i], 1);
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
		collisionTree2.GetRootChildren(childIndices, childCount);

		if (childCount == 0)
		{
			// root never split — query it directly, single-threaded
			collisionTree2.QueryCell(subtrees[0], 0);
			childCount = 1;
		}
		else
		{
			std::future<void> threadResults[8];
			for (int i = 0; i < childCount; i++)
				threadResults[i] = threadPool.Submit([this, childIndices, i, &subtrees]() {
				collisionTree2.QueryCell(subtrees[i], childIndices[i]);
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

	DotsScene_OnDotCollide(dots[dot1]);
	DotsScene_OnDotCollide(dots[dot2]);
}

void DotsScene::DotsScene_OnDotCollide(int dot)
{
	dots[dot].health--;
	if (dots[dot].health <= 0)
	{
		DotsScene_ReplaceDot(DotsScene_CreateRandomDot(), dot);
		return;
	}

	dots[dot].SetMesh(DotAssets::Meshes()[dots[dot].health - 1]);
	DotsScene_AddMeshInstance(dots[dot].health - 1, glm::translate(glm::mat4(1.0f), dots[dot].position));
}

void DotsScene::DotsScene_InstancedDotRender()
{
	ZoneScopedNC("Instanced Rendering", Colors::Render);

	using namespace DotAssets;

	for (auto& instances : meshInstances)
		instances.clear();

	// add a mesh instance for every dot
	for (size_t i = 0; i < dotsCount; i++)
		meshInstances[dots[i].health - 1].push_back(glm::translate(glm::mat4(1.0f), dots[i].position));

	SphereShader()->Bind();
	SphereShader()->SetUniformMat4("ViewProjection", mainCamera->GetViewProjection());
	SphereShader()->Unbind();

	// update mesh instances and then render them
	for (size_t i = 0; i < meshCount; i++)
	{
		Meshes()[i]->Mesh_UpdateInstanceData(meshInstances[i]);
		Meshes()[i]->Mesh_RenderInstanced(SphereShader(), meshInstances[i].size());
	}
}

void DotsScene::DotsScene_ClearInstanceBuffers()
{
	
}

void DotsScene::DotsScene_AddMeshInstance(size_t meshIndex, const glm::mat4& transform)
{

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
	for (auto& mesh : DotAssets::Meshes())
		delete mesh;

	DotsScene_DeleteDots();
}

void DotsScene::DotsScene_LeaderBoardMode()
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	DotsScene_DeleteDots();
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = pendingDotsCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start();
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}