static const char* vertex_shader_text =
#version 410
uniform mat4 MVP;
attribute vec3 vCol;
attribute vec2 vPos;
out vec3 color;

void main()
{
    gl_Position = MVP * vec4(vPos, 0.0, 1.0);
    color = vCol;
};

