#version 400


layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexColor;

//uniform mat4 ProjectionMatrix;
//uniform mat4 ViewMatrix;
uniform mat4 ViewProjectionMatrix;

out vec3 color;
void main() 
{
    color = vertexColor;
    gl_Position = ViewProjectionMatrix * vec4(vertexPosition, 1.0);
};
