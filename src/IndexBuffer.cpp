#include "IndexBuffer.h"

#include "glad/glad.h"

namespace Rendering
{
    IndexBuffer::IndexBuffer(const GLuint* Indices, unsigned int InCount, GLenum Usage)
        : Count(InCount)
    {
        glGenBuffers(1, &BufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(Count * sizeof(GLuint)), Indices, Usage);
    }

    IndexBuffer::~IndexBuffer()
    {
        if(glIsBuffer(BufferID))
        {
            glDeleteBuffers(1, &BufferID);
            BufferID = 0;
        }
    }

    IndexBuffer::IndexBuffer(IndexBuffer&& Other) noexcept
        : BufferID(Other.BufferID), Count(Other.Count)
    {
        Other.BufferID = 0;
        Other.Count = 0;
    }

    IndexBuffer& IndexBuffer::operator=(IndexBuffer&& Other) noexcept
    {
        if(this != &Other)
        {
            if(glIsBuffer(BufferID))
            {
                glDeleteBuffers(1, &BufferID);
            }

            BufferID = Other.BufferID;
            Count = Other.Count;
            Other.BufferID = 0;
            Other.Count = 0;
        }

        return *this;
    }

    void IndexBuffer::Bind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, BufferID);
    }

    void IndexBuffer::Unbind() const
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
