#version 330

layout(location = 0) in vec2 vertexPosition; // already NDC [-1,1], no transform needed
layout(location = 1) in vec2 vertexUV;

out vec2 uv;
void main()
{
    uv = vertexUV;
    gl_Position = vec4(vertexPosition, 0.0, 1.0);
};
