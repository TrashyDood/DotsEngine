#pragma once
#include "Mesh.h"
class Mesh_Sphere : public Mesh
{
public:
	Mesh_Sphere(float aRadius, glm::vec3 aColor);

	const inline float Radius() const { return radius; }
protected:
	void Mesh_CreateMesh() override;
	float radius;
};

