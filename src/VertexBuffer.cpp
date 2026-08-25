#include "VertexBuffer.h"

#include "glad/glad.h"

namespace Rendering
{
    VertexBuffer::VertexBuffer(const void* Data, unsigned long long InSizeInBytes, GLenum Usage)
        : SizeInBytes(InSizeInBytes)
    {
        glGenBuffers(1, &BufferID);
        glBindBuffer(GL_ARRAY_BUFFER, BufferID);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)SizeInBytes, Data, Usage);
    }

    VertexBuffer::~VertexBuffer()
    {
        if(glIsBuffer(BufferID))
        {
            glDeleteBuffers(1, &BufferID);
            BufferID = 0;
        }
    }

    VertexBuffer::VertexBuffer(VertexBuffer&& Other) noexcept
        : BufferID(Other.BufferID), SizeInBytes(Other.SizeInBytes)
    {
        Other.BufferID = 0;
        Other.SizeInBytes = 0;
    }

    VertexBuffer& VertexBuffer::operator=(VertexBuffer&& Other) noexcept
    {
        if(this != &Other)
        {
            if(glIsBuffer(BufferID))
            {
                glDeleteBuffers(1, &BufferID);
            }

            BufferID = Other.BufferID;
            SizeInBytes = Other.SizeInBytes;
            Other.BufferID = 0;
            Other.SizeInBytes = 0;
        }

        return *this;
    }

    void VertexBuffer::Bind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, BufferID);
    }

    void VertexBuffer::Unbind() const
    {
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}
