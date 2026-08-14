#pragma once
#include <vector>
#include <glm/glm.hpp>

class Shader;

class Mesh
{
public:

	void Mesh_Setup();
	void Mesh_Render(Shader* aShader, glm::vec3 position);

protected:
	virtual void Mesh_CreateMesh();
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	glm::vec3 color;

private:
	unsigned int VAO, VBO, EBO;
};

