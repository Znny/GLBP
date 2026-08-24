#version 400

layout(location = 0) in vec2 vertexPosition;
layout(location = 1) in vec2 vertexUV;

uniform mat4 ProjectionMatrix;

out vec2 uv;
void main()
{
    uv = vertexUV;
    gl_Position = ProjectionMatrix * vec4(vertexPosition, 0.0, 1.0);
};
