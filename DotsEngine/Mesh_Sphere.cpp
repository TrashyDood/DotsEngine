#include "Mesh_Sphere.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

Mesh_Sphere::Mesh_Sphere(float aRadius, glm::vec3 aColor)
{
	radius = aRadius;
	color = aColor;
	Mesh_Setup();
}

void Mesh_Sphere::Mesh_CreateMesh()
{
	int stacks = 36;
	int sectors = 18;

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
