#pragma once

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    class IndexBuffer
    {
    public:
        IndexBuffer(const GLuint* Indices, unsigned int Count, GLenum Usage);
        ~IndexBuffer();

        IndexBuffer(const IndexBuffer&) = delete;
        IndexBuffer& operator=(const IndexBuffer&) = delete;

        IndexBuffer(IndexBuffer&& Other) noexcept;
        IndexBuffer& operator=(IndexBuffer&& Other) noexcept;

        void Bind() const;
        void Unbind() const;

        GLuint GetBufferID() const { return BufferID; }
        unsigned int GetCount() const { return Count; }

    private:
        GLuint BufferID = 0;
        unsigned int Count = 0;
    };
}

#undef GLuint
#undef GLenum
