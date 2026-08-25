#pragma once

#include <optional>
#include <vector>

#include "VertexBuffer.h"
#include "IndexBuffer.h"

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    // Describes one interleaved vertex attribute (position, color, uv, ...) within a VertexBuffer.
    // Byte offsets within the vertex are computed automatically by VertexArray::AddVertexBuffer,
    // based on the order attributes appear in the list passed to it.
    struct FVertexAttribute
    {
        GLuint Index;
        int ComponentCount;
        GLenum Type;
        bool Normalized;
    };

    class VertexArray
    {
    public:
        VertexArray();
        ~VertexArray();

        VertexArray(const VertexArray&) = delete;
        VertexArray& operator=(const VertexArray&) = delete;

        VertexArray(VertexArray&& Other) noexcept;
        VertexArray& operator=(VertexArray&& Other) noexcept;

        // Takes ownership of Buffer, binds it to this VAO, and lays out Attributes over its data
        // (interleaved, Stride bytes apart). Buffer is kept alive for as long as this VertexArray is.
        void AddVertexBuffer(VertexBuffer&& Buffer, const std::vector<FVertexAttribute>& Attributes, int Stride);

        // Takes ownership of Buffer and binds it to this VAO as its element/index buffer.
        void SetIndexBuffer(IndexBuffer&& Buffer);

        void Bind() const;
        void Unbind() const;

        // Binds this VAO and issues the appropriate draw call for it: glDrawElements if an index
        // buffer has been set, otherwise glDrawArrays over the vertex count of the last buffer
        // added via AddVertexBuffer. PrimitiveType is a GL primitive enum (GL_TRIANGLES, GL_TRIANGLE_STRIP, ...).
        void Draw(GLenum PrimitiveType) const;

        GLuint GetArrayID() const { return ArrayID; }
        const std::optional<IndexBuffer>& GetIndexBuffer() const { return IndexBufferObject; }

    private:
        GLuint ArrayID = 0;
        std::vector<VertexBuffer> VertexBuffers;
        std::optional<IndexBuffer> IndexBufferObject;
        int VertexCount = 0;
    };
}

#undef GLuint
#undef GLenum
