#version 330 core

layout(location = 0) in vec3 aPos;

uniform mat4 ModelViewProjection;
uniform vec3 Color;
out vec3 color;

void main()
{
    gl_Position = ModelViewProjection * vec4(aPos, 1.0);
    color = Color;
}