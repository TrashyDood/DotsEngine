#pragma once
#include "Mesh.h"

class Mesh_Sphere : public InstancedMesh
{
protected:
	float radius,
		tessellation;

public:
	Mesh_Sphere(float aRadius, glm::vec3 aColor, float detailLevel = 4);

	const inline float Radius() const { return radius; }

protected:
	void Mesh_ConstructMesh() override;
};