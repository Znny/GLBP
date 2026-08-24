#version 330

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;

uniform mat4 ViewProjectionMatrix;
uniform mat4 ModelMatrix;

out vec3 color;
void main()
{
    color = vertexColor;
    gl_Position = ViewProjectionMatrix * ModelMatrix * vec4(vertexPosition, 1.0);
};
