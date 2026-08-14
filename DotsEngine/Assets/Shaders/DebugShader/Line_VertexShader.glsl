#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 ModelViewProjection;

out vec3 VertexColor;

void main()
{
    gl_Position = ModelViewProjection * vec4(aPosition, 1.0);
    VertexColor = aColor;
}
