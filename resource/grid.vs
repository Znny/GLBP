#version 330

layout(location = 0) in vec2 vertexPosition; // already NDC [-1,1], no transform needed - see blit.vs

out vec2 ndc;
void main()
{
    ndc = vertexPosition;
    gl_Position = vec4(vertexPosition, 0.0, 1.0);
};
