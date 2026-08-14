#pragma once
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <string>

class Shader
{
public:
    Shader() = default;
    Shader(const char* vertexSrc, const char* fragmentSrc);
    void Compile(const char* vertexSrc, const char* fragmentSrc);

    ~Shader();

    void Bind() const;
    void Unbind() const;

    void SetUniformMat4(const char* name, const glm::mat4& mat) const;
    void SetUniformVec3(const char* name, const glm::vec3& vec) const;
    void SetUniformInt(const char* name, const int& aInt) const;

    void Recompile(std::string aPath);
    std::string LoadTextFile(const std::string& path);

private:

    unsigned int ID = 0;

    unsigned int CompileShader(GLenum type, const char* source);
    unsigned int LinkProgram(GLuint vertexShader, GLuint fragmentShader);


};