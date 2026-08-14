#pragma once
#include "DebugLine.h"
#include <glm/glm.hpp>
#include <vector>

class Shader;
class DebugLines
{
	
	public:

		DebugLines();
		void DrawDebugLines();

		void AllocateLines(unsigned int aSize);

		static void DrawLine(glm::vec3 from, glm::vec3 to, glm::vec3 color);
		static void DrawLine(glm::vec3 from, glm::vec3 to) { DrawLine(from, to, glm::vec3(1, 1, 1)); };
		static void DrawSphere(const glm::vec3& position, float radius, const glm::vec3& color = glm::vec3(1.0f), int segments = 8);
		static void DrawCube(const glm::vec3& center, const glm::quat& rotation, const glm::vec3& extents, const glm::vec3& color);

	private:

		unsigned int lineVAO;
		unsigned int lineVBO;

		unsigned int currentLineBufferSize;

		Shader* lineShader;

		static std::vector<const char*> logQueue;
		static std::vector<LineVertex> debugLines;
};

