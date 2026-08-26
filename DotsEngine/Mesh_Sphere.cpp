#include "Mesh_Sphere.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

Mesh_Sphere::Mesh_Sphere(float aRadius, glm::vec3 aColor, float detailLevel)
{
	radius = aRadius;
	color = aColor;
	Mesh_Sphere::tessellation = detailLevel;

	Mesh_Init();
}

void Mesh_Sphere::Mesh_ConstructMesh()
{
	const static int minSegments = 4, maxSegments = 16;

	int stacks = glm::clamp((int)(radius * tessellation), minSegments, maxSegments);
	int sectors = glm::clamp((int)(radius * tessellation), minSegments, maxSegments);

	for (int i = 0; i <= stacks; ++i)
	{
		float stackAngle = glm::pi<float>() / 2 - i * glm::pi<float>() / stacks;
		float xy = radius * cos(stackAngle);
		float z = radius * sin(stackAngle);

		for (int j = 0; j <= sectors; ++j)
		{
			float sectorAngle = j * 2 * glm::pi<float>() / sectors;

			float x = xy * cos(sectorAngle);
			float y = xy * sin(sectorAngle);

			vertices.push_back(x);
			vertices.push_back(y);
			vertices.push_back(z);
		}
	}

	for (int i = 0; i < stacks; ++i)
	{
		int k1 = i * (sectors + 1);
		int k2 = k1 + sectors + 1;

		for (int j = 0; j < sectors; ++j, ++k1, ++k2)
		{
			if (i != 0)
			{
				indices.push_back(k1);
				indices.push_back(k2);
				indices.push_back(k1 + 1);
			}

			if (i != (stacks - 1))
			{
				indices.push_back(k1 + 1);
				indices.push_back(k2);
				indices.push_back(k2 + 1);
			}
		}
	}
}
