#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "glTypes.h"

namespace Rendering
{
    class VertexArray;

    // Plain CPU-side geometry - positions/normals/colors/UVs as separate (SoA/"planar") arrays
    // rather than one interleaved buffer, so shape-generation code can build/edit one attribute at
    // a time without hand-computing strides or offsets. Has no GL dependency and needs no live GL
    // context, unlike VertexArray - only UploadMesh() below touches the GPU. Positions is the only
    // required field; Normals/Colors/UVs are optional (empty = not present) and Indices empty means
    // a non-indexed draw.
    struct FMeshData
    {
        std::vector<glm::vec3> Positions;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec3> Colors;
        std::vector<glm::vec2> UVs;
        std::vector<GLuint> Indices;
    };

    // Interleaves whichever attributes MeshData has populated into a single VertexBuffer (plus an
    // IndexBuffer, if MeshData.Indices is non-empty) and returns a ready-to-draw VertexArray.
    // Attribute locations are assigned in a fixed order - Position is always 0, then Normal/Color/UV
    // take the next free location in that order, skipping whichever are empty. This matches every
    // shader in this codebase so far (passthrough.vs: 0=position,1=color; textured.vs:
    // 0=position,1=uv); a shader consuming a mesh with a different attribute combination will need
    // its layout(location=..) qualifiers updated to match.
    std::unique_ptr<VertexArray> UploadMesh(const FMeshData& MeshData);
}
