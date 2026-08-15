#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 iM0;
layout(location = 2) in vec4 iM1;
layout(location = 3) in vec4 iM2;
layout(location = 4) in vec4 iM3;

uniform mat4 ViewProjection;
uniform vec3 Color;

out vec3 color;

void main()
{
    mat4 model = mat4(iM0, iM1, iM2, iM3);
    gl_Position = ViewProjection * model * vec4(aPos, 1.0);
    color = Color;
}