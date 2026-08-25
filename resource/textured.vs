#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 vertexUV;

uniform mat4 ViewProjectionMatrix;

out vec2 uv;
void main()
{
    uv = vertexUV;
    gl_Position = ViewProjectionMatrix * vec4(vertexPosition, 1.0);
};
