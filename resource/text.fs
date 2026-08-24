#version 330

in vec2 uv;
out vec4 frag_colour;

uniform sampler2D FontAtlas;
uniform vec3 TextColor;

void main()
{
    float Alpha = texture(FontAtlas, uv).r;
    frag_colour = vec4(TextColor, Alpha);
};
