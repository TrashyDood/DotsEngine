#include "Mesh.h"
#include <glad/glad.h>
#include "Shader.h"
#include "DotsScene.h"
#include <Camera/Camera.h>

void Mesh::Mesh_CreateMesh()
{
	// nothing burger
}

void Mesh::Mesh_Setup()
{
	Mesh_CreateMesh();

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

void Mesh::Mesh_Render(Shader* aShader, glm::vec3 position)
{
	aShader->Bind();

	glm::mat4 model = glm::translate(glm::mat4(1.0f), position);
	aShader->SetUniformMat4("ModelViewProjection", DotsScene::mainCamera->GetViewProjection() * model); 
	aShader->SetUniformVec3("Color", color); 

	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);

	aShader->Unbind();
}
