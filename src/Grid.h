#pragma once

#include <memory>

#include <glm/glm.hpp>

#include "glTypes.h"

class Camera;
namespace Rendering
{
    class ShaderProgram;
    class VertexArray;
}

// Editor-style reference grid drawn into a viewport: a full-screen NDC quad whose fragment shader
// (resource/grid.fs) ray-casts every pixel against a plane through the world origin and draws
// anti-aliased grid lines analytically, so it stays "infinite" regardless of camera position -
// unlike a fixed-extent line mesh, it never runs out as the free-fly perspective camera moves away
// from the origin. One shared instance draws all 4 viewports, each with its own plane basis (see
// main.cpp's FViewport::GridTangent/GridBitangent) - mirrors how the single TransformGizmo
// instance (Gizmo.h) is reused across viewports via per-call parameters.
class Grid
{
public:
    Grid() = default;

    void Initialize();

    // PlaneTangent/PlaneBitangent must be orthonormal - they span the grid's plane through the
    // world origin (its normal is derived as their cross product) and define the plane's 2D
    // coordinate axes, each drawn as a highlighted line through the origin.
    void Draw(Camera& ViewportCamera, const glm::vec3& PlaneTangent, const glm::vec3& PlaneBitangent) const;

private:
    std::shared_ptr<Rendering::ShaderProgram> Shader;
    std::unique_ptr<Rendering::VertexArray> QuadMesh;
    bool bInitialized = false;
};
