#pragma once

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    class UniformBuffer
    {
    public:
        // Allocates SizeInBytes of uninitialized storage - callers upload actual data via SetData(),
        // typically once per frame, so no initial data pointer is taken here.
        UniformBuffer(unsigned long long SizeInBytes, GLenum Usage);
        ~UniformBuffer();

        UniformBuffer(const UniformBuffer&) = delete;
        UniformBuffer& operator=(const UniformBuffer&) = delete;

        UniformBuffer(UniformBuffer&& Other) noexcept;
        UniformBuffer& operator=(UniformBuffer&& Other) noexcept;

        void Bind() const;
        void Unbind() const;

        // Overwrites SizeInBytes bytes starting at Offset within the buffer.
        void SetData(const void* Data, unsigned long long SizeInBytes, unsigned long long Offset = 0);

        // Binds the whole buffer to BindingPoint, matching the binding a shader's uniform block is
        // pointed at via glUniformBlockBinding.
        void BindToPoint(unsigned int BindingPoint) const;

        GLuint GetBufferID() const { return BufferID; }

    private:
        GLuint BufferID = 0;
    };
}

#undef GLuint
#undef GLenum
