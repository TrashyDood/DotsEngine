#include "DotsScene.h"
#include "DebugLines.h"
#include <Time/Time.h>

#include <random>
#include <glm/gtc/random.hpp>

#include <iostream>

#include "tracy/Tracy.hpp"

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
	for (size_t i = 0; i < dotCount; i++)
		DotsScene_SpawnRandomDot();
}

void DotsScene::DotsScene_Render()
{
	//DotsScene_NaiveDotRender();
	DotsScene_InstancedDotRender();

	//collisionTree.DrawBounds();
	DotsScene_DrawBounds();
}

void DotsScene::DotsScene_Update()
{
	flyingCamera->Update();

	//Handle velocity
	for (auto& dot : dots)
		dot.position += glm::normalize(dot.velocity) * (dotSpeed / dot.radius) * Time::deltaTime;

	DotsScene_HandleBoundsCollisions();
	DotsScene_HandleDotsCollisions();

	//Look for nan's
	for (auto& dot : dots)
		if (glm::any(glm::isnan(dot.position)))
			std::cout << "NAN FOUND IN DOT" << std::endl;

	DotsScene_HandleDeadDots();
}

void DotsScene::DotsScene_DrawBounds()
{
	DebugLines::DrawCube(glm::vec3(0, 0, 0), glm::quat(), glm::vec3(bounds, bounds, bounds), glm::vec3(1, 0, 0));
}

void DotsScene::DotsScene_SpawnRandomDot()
{
	glm::vec3 position;
	position.x = RandomFloat(-bounds, bounds);
	position.y = RandomFloat(-bounds, bounds);
	position.z = RandomFloat(-bounds, bounds);

	Dot newDot;
	newDot.Health = 3;
	newDot.Refresh();

	glm::vec3 velocity = glm::sphericalRand(1.0f);
	newDot.velocity = velocity;
	newDot.position = position;

	dots.push_back(newDot);
}

void DotsScene::DotsScene_SpawnDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.Health = 3;
	newDot.Refresh();

	newDot.velocity = aVelocity;
	newDot.position = aPosition;

	dots.push_back(newDot);
}

void DotsScene::DotsScene_ClearDots()
{
	dots.clear();
}

void DotsScene::DotsScene_HandleDotsCollisions()
{
	{
		//ZoneScopedNC("Create Collision Tree", Colors::Collision);

		collisionTree.Reset(bounds);
		for (size_t i = 0; i < dots.size(); i++)
			collisionTree.Insert(i, dots[i].position, dots[i].radius);
	}

	for (size_t dot1 = 0; dot1 < dots.size(); dot1++)
	{
		std::vector<size_t> nearby;

		{
			//ZoneScopedNC("Query Collisions", Colors::Collision);

			collisionTree.Query(dots[dot1].position, dots[dot1].radius, nearby);
		}

		{
			//ZoneScopedNC("Resolve Collisions", Colors::Collision);

			for (size_t dot2 : nearby)
			{
				if (dot2 <= dot1) continue;

				float radiusSum = dots[dot1].radius + dots[dot2].radius;
				glm::vec3 diff = dots[dot1].position - dots[dot2].position;
				float distSqr = glm::length2(diff);

				if (distSqr < radiusSum * radiusSum && distSqr > 0.0001f)
				{
					float dist = std::sqrt(distSqr);
					glm::vec3 normal = diff / dist;

					float overlap = radiusSum - dist;
					glm::vec3 correction = normal * (overlap * 0.5f * 0.2f);
					dots[dot1].position += correction;
					dots[dot2].position -= correction;

					glm::vec3 n1 = glm::normalize(dots[dot1].velocity);
					glm::vec3 n2 = glm::normalize(dots[dot2].velocity);
					dots[dot2].velocity = glm::reflect(dots[dot2].velocity, n1);
					dots[dot1].velocity = glm::reflect(dots[dot1].velocity, n2);

					dots[dot2].Health--;
					dots[dot2].Refresh();
					dots[dot1].Health--;
					dots[dot1].Refresh();
				}
			}
		}
	}
}

void DotsScene::DotsScene_HandleBoundsCollisions()
{
	//Handle bounds
	for (auto& dot : dots)
	{
		if (dot.position.x > bounds - dot.radius || dot.position.x < -bounds + dot.radius) dot.velocity.x *= -1;
		if (dot.position.y > bounds - dot.radius || dot.position.y < -bounds + dot.radius) dot.velocity.y *= -1;
		if (dot.position.z > bounds - dot.radius || dot.position.z < -bounds + dot.radius) dot.velocity.z *= -1;

		dot.position.x = glm::clamp(dot.position.x, -bounds + dot.radius, bounds - dot.radius);
		dot.position.y = glm::clamp(dot.position.y, -bounds + dot.radius, bounds - dot.radius);
		dot.position.z = glm::clamp(dot.position.z, -bounds + dot.radius, bounds - dot.radius);
	}
}

void DotsScene::DotsScene_HandleDeadDots()
{
	//Handle death, spawn new dot
	std::vector<size_t> toRemove;

	for (size_t i = 0; i < dots.size(); i++)
	{
		if (dots[i].Health <= 0)
		{
			toRemove.push_back(i);
			DotsScene_SpawnRandomDot();
		}
	}

	//Handle removal from vector and spawning new dot
	for (int i = toRemove.size() - 1; i >= 0; i--)
		dots.erase(dots.begin() + toRemove[i]);
}

void DotsScene::DotsScene_NaiveDotRender()
{
	ZoneScopedNC("Naive Rendering", Colors::Render);

	for (auto& dot : dots)
		dot.mesh->Mesh_RenderSingle(DotAssets::SphereShader(), dot.position);
}

void DotsScene::DotsScene_InstancedDotRender()
{
	//ZoneScopedNC("Instanced Rendering", Colors::Render);

	using namespace DotAssets;

	for (auto& instances : meshInstances)
		instances.clear();

	// add a mesh instance for every dot
	for (auto& dot : dots)
		meshInstances[dot.Health - 1].push_back(glm::translate(glm::mat4(1.0f), dot.position));

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

	using namespace DotAssets;

	delete SphereShader();

	for (auto& mesh : DotAssets::Meshes())
		delete mesh;
}

void DotsScene::DotsScene_LeaderBoardMode()
{
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
	DotsScene_ClearDots();
	float spacing = 8.0f;
	float volumePerEntity = spacing * spacing * spacing;
	float totalVolume = dotCount * volumePerEntity;
	dotSpeed = 7.0f;
	bounds = cbrt(totalVolume);
	DotsScene_Start();
	// TOUCH THIS AND YOU DO NOT QUALIFY FOR LEADERBOARD
}
