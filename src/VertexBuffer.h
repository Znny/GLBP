#pragma once

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    class VertexBuffer
    {
    public:
        VertexBuffer(const void* Data, unsigned long long SizeInBytes, GLenum Usage);
        ~VertexBuffer();

        VertexBuffer(const VertexBuffer&) = delete;
        VertexBuffer& operator=(const VertexBuffer&) = delete;

        VertexBuffer(VertexBuffer&& Other) noexcept;
        VertexBuffer& operator=(VertexBuffer&& Other) noexcept;

        void Bind() const;
        void Unbind() const;

        GLuint GetBufferID() const { return BufferID; }
        unsigned long long GetSizeInBytes() const { return SizeInBytes; }

    private:
        GLuint BufferID = 0;
        unsigned long long SizeInBytes = 0;
    };
}

#undef GLuint
#undef GLenum
