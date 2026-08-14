#pragma once
#include <glm/glm.hpp>
#include "Shader.h"

struct LineVertex
{
	glm::vec3 aPosition;
	glm::vec3 aColor;
};

struct LineData
{
	LineData()
	{
		points[0].aPosition = glm::vec3(0);
		points[0].aColor = glm::vec3(0);
		points[1].aPosition = glm::vec3(0);
		points[1].aColor = glm::vec3(0);
	};

	LineData(glm::vec3 from, glm::vec3 to, glm::vec3 color)
	{
		points[0].aPosition = from;
		points[0].aColor = color;

		points[1].aPosition = to;
		points[1].aColor = color;
	}

	LineVertex points[2] = { LineVertex{}, LineVertex{} };

	bool operator==(const LineData& other) const
	{
		return
			points[0].aPosition == other.points[0].aPosition &&
			points[0].aColor == other.points[0].aColor &&
			points[1].aPosition == other.points[1].aPosition &&
			points[1].aColor == other.points[1].aColor;
	}
};




