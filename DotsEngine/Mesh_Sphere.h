#pragma once
#include "Mesh.h"

class Mesh_Sphere : public InstancedMesh
{
protected:
	float radius;

public:
	Mesh_Sphere(float aRadius, glm::vec3 aColor);

	const inline float SetRadius() const { return radius; }

protected:
	void Mesh_ConstructMesh() override;
};