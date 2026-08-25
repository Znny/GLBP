#version 330

in vec2 uv;
out vec4 frag_colour;

uniform sampler2D TexSampler;

// Must exactly match the block declared in textured.vs - GLSL requires each stage referencing a
// uniform block to redeclare it itself, it isn't shared automatically like a varying is.
layout(std140) uniform FrameConstants
{
    mat4 ViewProjectionMatrix;
    vec2 Resolution;
    vec2 CursorPosition;
    float Time;
};

void main()
{
    vec4 TexColor = texture(TexSampler, uv);

    //gentle brightness pulse driven by Time
    float Pulse = 0.7 + 0.3 * sin(Time * 2.0);

    //soft glow following the mouse cursor. gl_FragCoord is bottom-left-origin; CursorPosition comes
    //from GLFW, which is top-left-origin, so flip Y before comparing
    vec2 CursorScreenPos = vec2(CursorPosition.x, Resolution.y - CursorPosition.y);
    float DistanceToCursor = length(gl_FragCoord.xy - CursorScreenPos);
    float Glow = smoothstep(140.0, 0.0, DistanceToCursor);

    vec3 Color = TexColor.rgb * Pulse + vec3(1.0, 0.85, 0.4) * Glow * 0.5;
    frag_colour = vec4(Color, TexColor.a);
};
