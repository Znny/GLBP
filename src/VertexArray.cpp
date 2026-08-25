#include "VertexArray.h"

#include "glad/glad.h"

namespace
{
    unsigned long long SizeOfGLType(GLenum Type)
    {
        switch(Type)
        {
            case GL_FLOAT: return sizeof(GLfloat);
            case GL_UNSIGNED_INT: return sizeof(GLuint);
            case GL_INT: return sizeof(GLint);
            default: return sizeof(GLfloat);
        }
    }
}

namespace Rendering
{
    VertexArray::VertexArray()
    {
        glGenVertexArrays(1, &ArrayID);
    }

    VertexArray::~VertexArray()
    {
        if(glIsVertexArray(ArrayID))
        {
            glDeleteVertexArrays(1, &ArrayID);
            ArrayID = 0;
        }
    }

    VertexArray::VertexArray(VertexArray&& Other) noexcept
        : ArrayID(Other.ArrayID),
          VertexBuffers(std::move(Other.VertexBuffers)),
          IndexBufferObject(std::move(Other.IndexBufferObject)),
          VertexCount(Other.VertexCount)
    {
        Other.ArrayID = 0;
        Other.VertexCount = 0;
    }

    VertexArray& VertexArray::operator=(VertexArray&& Other) noexcept
    {
        if(this != &Other)
        {
            if(glIsVertexArray(ArrayID))
            {
                glDeleteVertexArrays(1, &ArrayID);
            }

            ArrayID = Other.ArrayID;
            VertexBuffers = std::move(Other.VertexBuffers);
            IndexBufferObject = std::move(Other.IndexBufferObject);
            VertexCount = Other.VertexCount;
            Other.ArrayID = 0;
            Other.VertexCount = 0;
        }

        return *this;
    }

    void VertexArray::AddVertexBuffer(VertexBuffer&& Buffer, const std::vector<FVertexAttribute>& Attributes, int Stride)
    {
        glBindVertexArray(ArrayID);
        Buffer.Bind();

        unsigned long long Offset = 0;
        for(const FVertexAttribute& Attribute : Attributes)
        {
            glVertexAttribPointer(Attribute.Index, Attribute.ComponentCount, Attribute.Type,
                                   Attribute.Normalized ? GL_TRUE : GL_FALSE, Stride, (const void*)Offset);
            glEnableVertexAttribArray(Attribute.Index);
            Offset += Attribute.ComponentCount * SizeOfGLType(Attribute.Type);
        }

        VertexCount = (int)(Buffer.GetSizeInBytes() / (unsigned long long)Stride);
        VertexBuffers.push_back(std::move(Buffer));
    }

    void VertexArray::SetIndexBuffer(IndexBuffer&& Buffer)
    {
        glBindVertexArray(ArrayID);
        Buffer.Bind();
        IndexBufferObject = std::move(Buffer);
    }

    void VertexArray::Bind() const
    {
        glBindVertexArray(ArrayID);
    }

    void VertexArray::Unbind() const
    {
        glBindVertexArray(0);
    }

    void VertexArray::Draw(GLenum PrimitiveType) const
    {
        Bind();
        if(IndexBufferObject.has_value())
        {
            glDrawElements(PrimitiveType, (GLsizei)IndexBufferObject->GetCount(), GL_UNSIGNED_INT, nullptr);
        }
        else
        {
            glDrawArrays(PrimitiveType, 0, VertexCount);
        }
    }
}
