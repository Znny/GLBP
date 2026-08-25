#include "UniformBuffer.h"

#include "glad/glad.h"

namespace Rendering
{
    UniformBuffer::UniformBuffer(unsigned long long SizeInBytes, GLenum Usage)
    {
        glGenBuffers(1, &BufferID);
        glBindBuffer(GL_UNIFORM_BUFFER, BufferID);
        glBufferData(GL_UNIFORM_BUFFER, (GLsizeiptr)SizeInBytes, nullptr, Usage);
    }

    UniformBuffer::~UniformBuffer()
    {
        if(glIsBuffer(BufferID))
        {
            glDeleteBuffers(1, &BufferID);
            BufferID = 0;
        }
    }

    UniformBuffer::UniformBuffer(UniformBuffer&& Other) noexcept
        : BufferID(Other.BufferID)
    {
        Other.BufferID = 0;
    }

    UniformBuffer& UniformBuffer::operator=(UniformBuffer&& Other) noexcept
    {
        if(this != &Other)
        {
            if(glIsBuffer(BufferID))
            {
                glDeleteBuffers(1, &BufferID);
            }

            BufferID = Other.BufferID;
            Other.BufferID = 0;
        }

        return *this;
    }

    void UniformBuffer::Bind() const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, BufferID);
    }

    void UniformBuffer::Unbind() const
    {
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    void UniformBuffer::SetData(const void* Data, unsigned long long SizeInBytes, unsigned long long Offset)
    {
        glBindBuffer(GL_UNIFORM_BUFFER, BufferID);
        glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)Offset, (GLsizeiptr)SizeInBytes, Data);
    }

    void UniformBuffer::BindToPoint(unsigned int BindingPoint) const
    {
        glBindBufferBase(GL_UNIFORM_BUFFER, BindingPoint, BufferID);
    }
}
