#include "Mesh.h"
#include <glad/glad.h>
#include "Shader.h"
#include "DotsScene.h"
#include <Camera/Camera.h>

void Mesh::Mesh_Init()
{
	Mesh_ConstructMesh();

	// generate new buffers and return the IDs
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindVertexArray(0);
}

void Mesh::Mesh_RenderSingle(Shader* aShader, glm::vec3 position)
{
	//aShader->Bind();

	//glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	//aShader->SetUniformMat4("ModelViewProjection", DotsScene::mainCamera->GetViewProjection() * model); 
	//aShader->SetUniformVec3("Color", color); 

	//glBindVertexArray(VAO);
	//glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	//glBindVertexArray(0);

	//aShader->Unbind();
}

// ---- Instanced Mesh ----

void InstancedMesh::Mesh_Init()
{
	Mesh::Mesh_Init();

	glBindVertexArray(GetVAO());
	glGenBuffers(1, &instanceVBO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	for (int i = 0; i < 4; i++)
	{
		glVertexAttribPointer(1 + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(sizeof(glm::vec4) * i));
		glEnableVertexAttribArray(1 + i);
		glVertexAttribDivisor(1 + i, 1);
	}

	glBindVertexArray(0);
}

void InstancedMesh::Mesh_UpdateInstanceData(const glm::mat4* models, const size_t modelCount)
{
	if (!modelCount) return;

	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	if (modelCount > instanceCapacity)
	{
		// grow (or first-time allocate) the store
		glBufferData(GL_ARRAY_BUFFER, modelCount * sizeof(glm::mat4), models, GL_DYNAMIC_DRAW);
		instanceCapacity = modelCount;
	}
	else
	{
		// reuse existing allocation, no reallocation
		glBufferSubData(GL_ARRAY_BUFFER, 0, modelCount * sizeof(glm::mat4), models);
	}
}

void InstancedMesh::Mesh_RenderInstanced(const Shader* shader, const size_t instanceCount)
{
	if (!instanceCount) return;

	shader->SetUniformVec3("Color", color);

	glBindVertexArray(GetVAO());
	glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0, (GLsizei)instanceCount);
	glBindVertexArray(0);
}