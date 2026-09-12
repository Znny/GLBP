#include "MeshData.h"

#include "glad/glad.h"

#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace Rendering
{
    std::unique_ptr<VertexArray> UploadMesh(const FMeshData& MeshData)
    {
        const bool bHasNormals = !MeshData.Normals.empty();
        const bool bHasColors = !MeshData.Colors.empty();
        const bool bHasUVs = !MeshData.UVs.empty();

        std::vector<FVertexAttribute> Attributes = { FVertexAttribute{0, 3, GL_FLOAT, false} };
        GLuint NextLocation = 1;
        if(bHasNormals) Attributes.push_back(FVertexAttribute{NextLocation++, 3, GL_FLOAT, false});
        if(bHasColors)  Attributes.push_back(FVertexAttribute{NextLocation++, 3, GL_FLOAT, false});
        if(bHasUVs)     Attributes.push_back(FVertexAttribute{NextLocation++, 2, GL_FLOAT, false});

        const int FloatsPerVertex = 3 + (bHasNormals ? 3 : 0) + (bHasColors ? 3 : 0) + (bHasUVs ? 2 : 0);

        const size_t VertexCount = MeshData.Positions.size();
        std::vector<float> Interleaved;
        Interleaved.reserve(VertexCount * (size_t)FloatsPerVertex);
        for(size_t Index = 0; Index < VertexCount; Index++)
        {
            const glm::vec3& Position = MeshData.Positions[Index];
            Interleaved.insert(Interleaved.end(), {Position.x, Position.y, Position.z});
            if(bHasNormals)
            {
                const glm::vec3& Normal = MeshData.Normals[Index];
                Interleaved.insert(Interleaved.end(), {Normal.x, Normal.y, Normal.z});
            }
            if(bHasColors)
            {
                const glm::vec3& Color = MeshData.Colors[Index];
                Interleaved.insert(Interleaved.end(), {Color.x, Color.y, Color.z});
            }
            if(bHasUVs)
            {
                const glm::vec2& UV = MeshData.UVs[Index];
                Interleaved.insert(Interleaved.end(), {UV.x, UV.y});
            }
        }

        auto Result = std::make_unique<VertexArray>();
        Result->AddVertexBuffer(
            VertexBuffer(Interleaved.data(), Interleaved.size() * sizeof(float), GL_STATIC_DRAW),
            Attributes,
            FloatsPerVertex * (int)sizeof(float));

        if(!MeshData.Indices.empty())
        {
            Result->SetIndexBuffer(IndexBuffer(MeshData.Indices.data(), (unsigned int)MeshData.Indices.size(), GL_STATIC_DRAW));
        }

        return Result;
    }
}
