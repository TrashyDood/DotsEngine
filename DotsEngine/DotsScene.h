#pragma once
#include <vector>
#include <Camera/FlyingCamera.h>
#include <array>

#include "Mesh_Sphere.h"
#include "Shader.h"

#include "CollisionTree.h"
#include "ThreadPool.h"


namespace DotAssets
{
	static constexpr uint16_t meshCount = 3;
	static constexpr uint8_t lodCount = 3;
	static inline Shader*& SphereShader() { static Shader* s = new Shader("Assets/Shaders/VertexShader.glsl", "Assets/Shaders/FragmentShader.glsl"); return s; }
	static inline std::array<Mesh_Sphere*, meshCount>& Meshes()
	{
		static std::array<Mesh_Sphere*, meshCount> meshes = {
			new Mesh_Sphere(1, glm::vec3(1, 0, 0), 3.5f),
			new Mesh_Sphere(2, glm::vec3(0, 1, 0), 3.5f),
			new Mesh_Sphere(3, glm::vec3(0, 0, 1), 3.5f) };
		return meshes;
	}
}

using TransformBuffer = std::unique_ptr<glm::mat4[]>;
using TransformBufferOffsets = std::array<uint32_t, DotAssets::meshCount + 1>;

class DotsScene
{
	CollisionTree treeA, treeB;
	CollisionTree* readTree = &treeA, *writeTree = &treeB;
	CollisionPairs collisionPairsA, collisionPairsB;
	CollisionPairs* readCollisionPairs = &collisionPairsA, *writeCollisionPairs = &collisionPairsB;
	std::future<void> treeTask;

	std::unique_ptr<int[]> dotsHealths, dotsMeshIndices;
	std::unique_ptr<float[]> dotsRadii;
	std::unique_ptr<glm::vec3[]> dotsPositions, dotsVelocities;
	std::future<void> simulationTask;

	TransformBuffer transfBuffersA, transfBuffersB;
	TransformBuffer* readTransfBuffers = &transfBuffersA,
		*writeTransfBuffers = &transfBuffersB;
	TransformBufferOffsets transfBufferOffsetsA, transfBufferOffsetsB;
	TransformBufferOffsets* readTransfBufferOffsets = &transfBufferOffsetsA,
		*writeTransfOffsetsBuffer = &transfBufferOffsetsB;

	std::future<void> populateBuffersTask;

public:
	static const uint32_t dotsCount_Default = 40000;

	FlyingCamera camera;

	float bounds = 50.0f,
		dotsSpeed = 5.0f;
	bool drawTree = false,
		asyncSimulation = true;

	uint32_t dotsCount = 0;

	DotsScene();
	~DotsScene();

	void DotsScene_Start(uint32_t dotsCount = dotsCount_Default);
	void DotsScene_LeaderBoardMode(uint32_t dotsCount = dotsCount_Default);
	void DotsScene_CollisionTest();

	void DotsScene_Tick();
	void DotsScene_Simulate();
	void DotsScene_Simulate_Async();
	void DotsScene_Render() const;

	void DotsScene_InitDotsData();
	void DotsScene_ClearDotsData();
	void DotsScene_UpdateDotsData(bool parallel = true);
	void DotsScene_SetDotData(int dot, glm::vec3 position = {}, glm::vec3 velocity = {}, int health = 3);
	void DotsScene_SetDotHealth(int dot, int health);

	void DotsScene_BuildAndQueryTree_Async();
	void DotsScene_ResolveCollisions();
	void DotsScene_ResolveCollision(int dot1, int dot2);
	void DotsScene_HandleBoundsCollisions();

	void DotsScene_InitRenderBuffers();
	void DotsScene_PopulateMeshBuffers(TransformBuffer& transformBuffers,
		TransformBufferOffsets& bufferOffsets) const;
	void DotsScene_PopulateMeshBuffers_Async();
	void DotsScene_RenderMeshBuffers(const TransformBuffer& transformBuffers,
		const TransformBufferOffsets& bufferOffsets) const;
	void DotsScene_DrawBounds() const;

	float RandomFloat(float min, float max) const;
};