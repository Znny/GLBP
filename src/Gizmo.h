#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Transform.h"
#include "Framebuffer.h"

namespace Rendering { class ShaderProgram; }

enum class EGizmoMode
{
    Translate,
    Rotate,
    Scale
};

// Which handle a drag interaction is currently operating on. None means no handle is being dragged.
enum class EGizmoAxis
{
    None,
    X,
    Y,
    Z
};

// Procedurally generated translate/rotate/scale handles, drawn as an overlay at a target's location.
class TransformGizmo
{
public:
    TransformGizmo() = default;

    void Initialize();

    void SetMode(EGizmoMode NewMode);
    EGizmoMode GetMode() const { return Mode; }

    // Draws at Target's location - axes are currently always world-aligned (no rotation applied to
    // the gizmo mesh), but Target is a full Transform (not just a position) so a future world-space-
    // vs-local-space toggle can orient the drawn axes to Target's rotation without an API change.
    void Draw(const Transform& Target, const glm::vec3& CameraLocation);

    // Applies a single drag step to Target for the current Mode, along Axis (resolved to a world-space
    // direction via GetAxisDirection), by Delta - world units for Translate/Scale, degrees for Rotate.
    void ApplyTransformDelta(Transform& Target, EGizmoAxis Axis, float Delta) const;

    // Same as above but takes an explicit direction rather than resolving one from Axis - used by
    // callers that have already converted the drag direction into a parented target's local frame
    // (a child node's drag direction isn't one of the raw X/Y/Z axes once its parent's rotation/scale
    // is accounted for).
    void ApplyTransformDeltaAlongDirection(Transform& Target, const glm::vec3& Direction, float Delta) const;

    // World-space unit direction for a given axis (X=1,0,0 / Y=0,1,0 / Z=0,0,1).
    static glm::vec3 GetAxisDirection(EGizmoAxis Axis);

    // Matches Draw()'s "keep a roughly constant apparent size on screen" scale computation, exposed so
    // picking/dragging code can reproduce the exact same on-screen handle size Draw() actually rendered.
    static float ComputeScale(const glm::vec3& CameraLocation, const glm::vec3& TargetLocation);

    // Ray-vs-handle-geometry hit test for the gizmo's current Mode, drawn at TargetLocation with the
    // given GizmoScale (see ComputeScale). Returns which axis handle (if any) the ray passes near.
    EGizmoAxis PickAxis(const glm::vec3& RayOrigin, const glm::vec3& RayDirection, const glm::vec3& TargetLocation, float GizmoScale) const;

    GLuint GetShaderID() const;

private:
    void ApplyTranslationDelta(Transform& Target, const glm::vec3& Direction, float Delta) const;
    void ApplyRotationDelta(Transform& Target, const glm::vec3& Direction, float Delta) const;
    void ApplyScaleDelta(Transform& Target, const glm::vec3& Direction, float Delta) const;

    EGizmoAxis PickLinearAxis(const glm::vec3& RayOrigin, const glm::vec3& RayDirection, const glm::vec3& TargetLocation, float GizmoScale) const;
    EGizmoAxis PickRotateAxis(const glm::vec3& RayOrigin, const glm::vec3& RayDirection, const glm::vec3& TargetLocation, float GizmoScale) const;

    struct FGizmoMesh
    {
        unsigned int VAO = 0;
        unsigned int PositionBuffer = 0;
        unsigned int ColorBuffer = 0;
        int LineVertexCount = 0;
        int TriangleVertexCount = 0;
    };

    static FGizmoMesh BuildTranslateMesh();
    static FGizmoMesh BuildRotateMesh();
    static FGizmoMesh BuildScaleMesh();

    static FGizmoMesh UploadMesh(const std::vector<glm::vec3>& LinePositions, const std::vector<glm::vec3>& LineColors,
                                  const std::vector<glm::vec3>& TrianglePositions, const std::vector<glm::vec3>& TriangleColors);

    EGizmoMode Mode = EGizmoMode::Translate;

    FGizmoMesh TranslateMesh;
    FGizmoMesh RotateMesh;
    FGizmoMesh ScaleMesh;
    FGizmoMesh* CurrentMesh = nullptr;

    std::shared_ptr<Rendering::ShaderProgram> Shader;
    bool bInitialized = false;
};
