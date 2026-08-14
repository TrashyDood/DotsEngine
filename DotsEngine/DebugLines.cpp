#include "DebugLines.h"
#include <iostream>
#include <fstream>
#include <string>
#include <streambuf>
#include "Shader.h"
#include "DotsScene.h"

std::vector<LineVertex> DebugLines::debugLines;
std::vector<const char*> DebugLines::logQueue;

DebugLines::DebugLines()
{
	lineShader = new Shader("Assets/Shaders/DebugShader/Line_VertexShader.glsl", "Assets/Shaders/DebugShader/Line_FragmentShader.glsl");
	AllocateLines(2000);
}

void DebugLines::DrawDebugLines()
{
	if (debugLines.size() >= currentLineBufferSize)
	{
		AllocateLines(currentLineBufferSize * 2);
	}

	lineShader->Bind();
	lineShader->SetUniformMat4("ModelViewProjection", DotsScene::mainCamera->GetViewProjection());

	glBindVertexArray(lineVAO);
	glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

	glBufferSubData(GL_ARRAY_BUFFER, 0, debugLines.size() * sizeof(LineVertex), debugLines.data());

	glDrawArrays(GL_LINES, 0, debugLines.size());

	glBindVertexArray(0);

	glEnable(GL_DEPTH_TEST);

	lineShader->Unbind();
	debugLines.clear();
}

void DebugLines::AllocateLines(unsigned int aSize)
{
	currentLineBufferSize = aSize;
	glGenVertexArrays(1, &lineVAO);
	glGenBuffers(1, &lineVBO);

	glBindVertexArray(lineVAO);
	glBindBuffer(GL_ARRAY_BUFFER, lineVBO);

	//allocate a ton of em
	glBufferData(GL_ARRAY_BUFFER, sizeof(LineVertex) * aSize, nullptr, GL_DYNAMIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, aPosition));

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex), (void*)offsetof(LineVertex, aColor));

	glBindVertexArray(0);
}

void DebugLines::DrawLine(glm::vec3 from, glm::vec3 to, glm::vec3 color)
{
	debugLines.push_back(LineVertex{ from, color });
	debugLines.push_back(LineVertex{ to, color });
}

void DebugLines::DrawSphere(const glm::vec3& position, float radius, const glm::vec3& color, int segments)
{
	const float step = glm::two_pi<float>() / static_cast<float>(segments);

	for (int i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);

		glm::vec3 p0 = position + glm::vec3(std::cos(a0) * radius,
			std::sin(a0) * radius,
			0.0f);

		glm::vec3 p1 = position + glm::vec3(std::cos(a1) * radius,
			std::sin(a1) * radius,
			0.0f);

		DrawLine(p0, p1, color);
	}

	for (int i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);

		glm::vec3 p0 = position + glm::vec3(std::cos(a0) * radius,
			0.0f,
			std::sin(a0) * radius);

		glm::vec3 p1 = position + glm::vec3(std::cos(a1) * radius,
			0.0f,
			std::sin(a1) * radius);

		DrawLine(p0, p1, color);
	}

	for (int i = 0; i < segments; ++i)
	{
		float a0 = step * i;
		float a1 = step * (i + 1);

		glm::vec3 p0 = position + glm::vec3(0.0f,
			std::cos(a0) * radius,
			std::sin(a0) * radius);

		glm::vec3 p1 = position + glm::vec3(0.0f,
			std::cos(a1) * radius,
			std::sin(a1) * radius);

		DrawLine(p0, p1, color);
	}
}

void DebugLines::DrawCube(const glm::vec3& center, const glm::quat& rotation, const glm::vec3& extents, const glm::vec3& color)
{
	glm::vec3 h = extents;

	glm::vec3 right = rotation * glm::vec3(1, 0, 0) * h.x;
	glm::vec3 up = rotation * glm::vec3(0, 1, 0) * h.y;
	glm::vec3 forward = rotation * glm::vec3(0, 0, 1) * h.z;

	glm::vec3 v[8] =
	{
		center - right - up - forward,
		center + right - up - forward,
		center + right + up - forward,
		center - right + up - forward,

		center - right - up + forward,
		center + right - up + forward,
		center + right + up + forward,
		center - right + up + forward
	};

	// Back face
	DrawLine(v[0], v[1], color);
	DrawLine(v[1], v[2], color);
	DrawLine(v[2], v[3], color);
	DrawLine(v[3], v[0], color);

	// Front face
	DrawLine(v[4], v[5], color);
	DrawLine(v[5], v[6], color);
	DrawLine(v[6], v[7], color);
	DrawLine(v[7], v[4], color);

	// Connections
	DrawLine(v[0], v[4], color);
	DrawLine(v[1], v[5], color);
	DrawLine(v[2], v[6], color);
	DrawLine(v[3], v[7], color);
}