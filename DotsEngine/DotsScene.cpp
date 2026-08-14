#include "DotsScene.h"
#include "DebugLines.h"
#include <Time/Time.h>

#include <random>
#include <glm/gtc/random.hpp>

#include <iostream>

#include "tracy/Tracy.hpp"

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
	for (auto& dot : dots)
		dot.Dot_Render();

	DotsScene_DrawBounds();

	//collisionTree.DrawBounds();
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
	newDot.ReCreate();

	glm::vec3 velocity = glm::sphericalRand(1.0f);
	newDot.velocity = velocity;
	newDot.position = position;

	dots.push_back(newDot);
}

void DotsScene::DotsScene_SpawnDot(glm::vec3 aPosition, glm::vec3 aVelocity)
{
	Dot newDot;
	newDot.Health = 3;
	newDot.ReCreate();

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
	ZoneScoped;

	//for (size_t d1 = 0; d1 < dots.size(); d1++)
	//{
	//	for (size_t d2 = d1 + 1; d2 < dots.size(); d2++)
	//	{
	//		float radiusSum = dots[d1].radius + dots[d2].radius;
	//		float distSqr = glm::length2(dots[d1].position - dots[d2].position);
	//
	//		if (distSqr < radiusSum * radiusSum)
	//		{
	//			glm::vec3 n1 = glm::normalize(dots[d1].velocity);
	//			glm::vec3 n2 = glm::normalize(dots[d2].velocity);
	//
	//			dots[d2].velocity = glm::reflect(dots[d2].velocity, n1);
	//			dots[d1].velocity = glm::reflect(dots[d1].velocity, n2);
	//
	//			dots[d2].Health--;
	//			dots[d2].ReCreate();
	//
	//			dots[d1].Health--;
	//			dots[d1].ReCreate();
	//		}
	//	}
	//}

	collisionTree = Octree(glm::vec3(0, 0, 0), bounds);
	
	for (size_t i = 0; i < dots.size(); i++)
		collisionTree.Insert(i, dots[i].position, dots[i].radius);
	
	for (size_t d1 = 0; d1 < dots.size(); d1++)
	{
		std::vector<size_t> nearby;
		collisionTree.Query(dots[d1].position, dots[d1].radius, nearby);
	
		for (size_t d2 : nearby)
		{
			if (d2 <= d1) continue; // skip self and checked pairs
	
			float radiusSum = dots[d1].radius + dots[d2].radius;
			float distSqr = glm::length2(dots[d1].position - dots[d2].position);
	
			if (distSqr < radiusSum * radiusSum)
			{
				glm::vec3 n1 = glm::normalize(dots[d1].velocity);
				glm::vec3 n2 = glm::normalize(dots[d2].velocity);
	
				dots[d2].velocity = glm::reflect(dots[d2].velocity, n1);
				dots[d1].velocity = glm::reflect(dots[d1].velocity, n2);
	
				dots[d2].Health--;
				dots[d2].ReCreate();
	
				dots[d1].Health--;
				dots[d1].ReCreate();
			}

			//float radiusSum = dots[d1].radius + dots[d2].radius;
			//glm::vec3 diff = dots[d1].position - dots[d2].position;
			//float distSqr = glm::length2(diff);
			//
			//if (distSqr < radiusSum * radiusSum)
			//{
			//	float dist = std::sqrt(distSqr);
			//	glm::vec3 normal = (dist > 0.0001f) ? (diff / dist) : glm::vec3(1, 0, 0); // fallback if exactly coincident
			//
			//	// --- Positional correction: push apart so they no longer overlap ---
			//	float overlap = radiusSum - dist;
			//	glm::vec3 correction = normal * (overlap * 0.5f); // split the correction between both dots
			//	dots[d1].position += correction;
			//	dots[d2].position -= correction;
			//
			//	// --- Velocity response (existing) ---
			//	glm::vec3 n1 = glm::normalize(dots[d1].velocity);
			//	glm::vec3 n2 = glm::normalize(dots[d2].velocity);
			//
			//	dots[d2].velocity = glm::reflect(dots[d2].velocity, n1);
			//	dots[d1].velocity = glm::reflect(dots[d1].velocity, n2);
			//
			//	dots[d2].Health--;
			//	dots[d2].ReCreate();
			//
			//	dots[d1].Health--;
			//	dots[d1].ReCreate();
			//}
		}
	}

	//collisionTree = Octree(glm::vec3(0, 0, 0), bounds);
	//
	//for (size_t i = 0; i < dots.size(); i++)
	//	collisionTree.Insert(i, dots[i].position, dots[i].radius);
	//
	//size_t threadCount = 12;//std::thread::hardware_concurrency();
	//size_t chunkSize = (dots.size() + threadCount - 1) / threadCount;
	//
	//std::vector<std::future<std::vector<std::pair<size_t, size_t>>>> futures;
	//
	//// parallel detection
	//for (size_t t = 0; t < threadCount; t++)
	//{
	//	size_t start = t * chunkSize;
	//	size_t end = std::min(start + chunkSize, dots.size());
	//	if (start >= end) continue;
	//
	//	futures.push_back(GlobalThreadPool().Submit([this, start, end]()
	//		{
	//			std::vector<std::pair<size_t, size_t>> localPairs;
	//
	//			for (size_t d1 = start; d1 < end; d1++)
	//			{
	//				std::vector<size_t> nearby;
	//				collisionTree.Query(dots[d1].position, dots[d1].radius, nearby);
	//
	//				for (size_t d2 : nearby)
	//				{
	//					if (d2 <= d1) continue;
	//
	//					float radiusSum = dots[d1].radius + dots[d2].radius;
	//					glm::vec3 diff = dots[d1].position - dots[d2].position;
	//					if (glm::length2(diff) < radiusSum * radiusSum)
	//						localPairs.emplace_back(d1, d2);
	//				}
	//			}
	//			return localPairs;
	//		}));
	//}
	//
	//// serial resolution
	//for (auto& f : futures)
	//{
	//	auto pairs = f.get();
	//	for (auto& p : pairs)
	//	{
	//		size_t d1 = p.first;
	//		size_t d2 = p.second;
	//
	//		glm::vec3 n1 = glm::normalize(dots[d1].velocity);
	//		glm::vec3 n2 = glm::normalize(dots[d2].velocity);
	//
	//		dots[d2].velocity = glm::reflect(dots[d2].velocity, n1);
	//		dots[d1].velocity = glm::reflect(dots[d1].velocity, n2);
	//
	//		dots[d2].Health--;
	//		dots[d2].ReCreate();
	//
	//		dots[d1].Health--;
	//		dots[d1].ReCreate();
	//	}
	//}
}

void DotsScene::DotsScene_HandleBoundsCollisions()
{
	ZoneScoped;

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
	ZoneScoped;

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
	delete SphereMesh1();
	delete SphereMesh2();
	delete SphereMesh3();
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
