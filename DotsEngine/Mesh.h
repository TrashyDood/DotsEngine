#pragma once
#include <vector>
#include <glm/glm.hpp>

class Shader;

class Mesh
{
	unsigned int VAO, VBO, EBO; // IDs for buffer data stored in GPU memory

protected:
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	glm::vec3 color;

public:
	const inline unsigned int GetVAO() const { return VAO; }
	const inline unsigned int GetVBO() const { return VBO; }
	const inline unsigned int GetEBO() const { return EBO; }

	void Mesh_RenderSingle(Shader* aShader, glm::vec3 position);

protected:
	virtual void Mesh_Init();
	virtual void Mesh_ConstructMesh() = 0;
};

class InstancedMesh : public Mesh
{
	unsigned int instanceVBO;

public:
	const inline unsigned int GetInstanceVBO() const { return instanceVBO; }

	void Mesh_UpdateInstanceData(const std::vector<glm::mat4>& models);
	void Mesh_RenderInstanced(const Shader* aShader, const size_t instanceCount);

protected:
	void Mesh_Init() override;
};
