#include "FrameConstants.h"

#include "glad/glad.h"

namespace Rendering
{
    void BindFrameConstantsBlock(GLuint ProgramID)
    {
        const GLuint BlockIndex = glGetUniformBlockIndex(ProgramID, "FrameConstants");
        if(BlockIndex != GL_INVALID_INDEX)
        {
            glUniformBlockBinding(ProgramID, BlockIndex, FrameConstantsBindingPoint);
        }
    }
}
