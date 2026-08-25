#pragma once

#include <glm/glm.hpp>

typedef unsigned int GLuint;

namespace Rendering
{
    // Binding point FrameConstants is bound to (UniformBuffer::BindToPoint) and every consuming
    // shader program is pointed at (BindFrameConstantsBlock). Shaders declare the block without an
    // explicit "binding=" layout qualifier - that syntax needs GL 4.2+, and this project's required
    // floor is 3.3 - so binding is assigned from C++ via glUniformBlockBinding instead.
    constexpr unsigned int FrameConstantsBindingPoint = 0;

    // Hand-mirrors the std140 layout of the "FrameConstants" uniform block declared in shaders that
    // opt in (see resource/textured.vs). Field order/padding must exactly match the GLSL block:
    //   layout(std140) uniform FrameConstants
    //   {
    //       mat4 ViewProjectionMatrix;
    //       vec2 Resolution;
    //       vec2 CursorPosition;
    //       float Time;
    //   };
    // std140 base-aligns vec2 to 8 bytes and rounds the whole block's size up to a multiple of 16,
    // hence the explicit trailing padding below - there's nothing to reorder around it.
    struct FFrameConstants
    {
        glm::mat4 ViewProjectionMatrix; // offset 0,  64 bytes
        glm::vec2 Resolution;           // offset 64,  8 bytes
        glm::vec2 CursorPosition;       // offset 72,  8 bytes
        float Time;                     // offset 80,  4 bytes
        float _Pad[3];                  // offset 84, 12 bytes (pads total size to 96)
    };
    static_assert(sizeof(FFrameConstants) == 96, "FFrameConstants must match the std140 layout exactly");

    // Points ProgramID's "FrameConstants" uniform block (if it declares one) at FrameConstantsBindingPoint.
    // No-op if the program doesn't reference the block. Called once per shader program after linking.
    void BindFrameConstantsBlock(GLuint ProgramID);
}
