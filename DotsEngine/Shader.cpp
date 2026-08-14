#include "Shader.h"
#include <iostream>
#include <sstream>
#include <fstream>

Shader::Shader(const char* vertexPath, const char* fragmentPath)
{
    std::string vpath = vertexPath;
    std::string fpath = fragmentPath;
    Recompile(vpath + "|" + fpath);
}

void Shader::Compile(const char* vertexSrc, const char* fragmentSrc)
{
    GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    if (vertexShader == 0)
    {
        std::cout << "VertexShader failed" << std::endl;
        return;
    }

    GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (fragmentShader == 0)
    {
        std::cout << "FragmentShader failed" << std::endl;
        return;
    }

    GLuint program = LinkProgram(vertexShader, fragmentShader);
    if (program == 0)
    {
        std::cout << "Linking shaders failed" << std::endl;
        return;
    }

    ID = program;

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::~Shader()
{
    if (ID) glDeleteProgram(ID);
}

GLuint Shader::CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);

        std::string typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";

        std::cout << "[" << typeStr << " SHADER ERROR]\n" << infoLog << std::endl;

        return 0;
    }

    return shader;
}

GLuint Shader::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cout << "[SHADER] link failed" << std::endl;
        return 0;
    }

    return program;
}

void Shader::Bind() const
{
    glUseProgram(ID);
}

void Shader::Unbind() const
{
    glUseProgram(0);
}

void Shader::SetUniformMat4(const char* name, const glm::mat4& mat) const
{
    GLint location = glGetUniformLocation(ID, name);
    if (location == -1) std::cout << "Warning: uniform '" << name << "' not found!" << std::endl;
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::SetUniformVec3(const char* name, const glm::vec3& vec) const
{
    GLint location = glGetUniformLocation(ID, name);
    if (location == -1) std::cout << "Warning: uniform '" << name << "' not found!" << std::endl;
    glUniform3fv(location, 1, glm::value_ptr(vec));
}

void Shader::SetUniformInt(const char* name, const int& aInt) const
{
    GLint location = glGetUniformLocation(ID, name);
    if (location == -1) std::cout << "Warning: uniform '" << name << "' not found!" << std::endl;
    glUniform1i(glGetUniformLocation(ID, name), aInt);
}

void Shader::Recompile(std::string aPath)
{
    std::string vertPath;
    std::string fragPath;
    std::string fullPath = aPath;
    std::string deliminator = "|";

    auto pos = fullPath.find(deliminator);

    vertPath = fullPath.substr(0, pos);
    fullPath.erase(0, pos + deliminator.length());
    fragPath = fullPath;

    Compile(LoadTextFile(vertPath).c_str(), LoadTextFile(fragPath).c_str());
}

std::string Shader::LoadTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}