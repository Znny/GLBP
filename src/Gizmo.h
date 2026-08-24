#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "Transform.h"

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
// Visualization only - dragging the handles to actually apply a transform is not implemented; picking
// which handle is under the cursor and turning mouse movement into a Delta are still TODO.
class TransformGizmo
{
public:
    TransformGizmo() = default;

    void Initialize();

    void SetMode(EGizmoMode NewMode) { Mode = NewMode; }
    EGizmoMode GetMode() const { return Mode; }

    void Draw(const Transform& Target, const glm::vec3& CameraLocation, const glm::mat4& ViewProjectionMatrix);

    // Applies a single drag step to Target for the current Mode, along Axis, by Delta - world units for
    // Translate/Scale, degrees for Rotate. Dispatches to the mode-specific stub below.
    void ApplyTransformDelta(Transform& Target, EGizmoAxis Axis, float Delta) const;

private:
    // TODO: translate Target along Axis (world-space) by Delta world units.
    void ApplyTranslationDelta(Transform& Target, EGizmoAxis Axis, float Delta) const;

    // TODO: rotate Target around Axis (world-space) by Delta degrees.
    void ApplyRotationDelta(Transform& Target, EGizmoAxis Axis, float Delta) const;

    // TODO: scale Target along Axis by Delta (added to that axis' current scale component).
    void ApplyScaleDelta(Transform& Target, EGizmoAxis Axis, float Delta) const;


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

    std::shared_ptr<Rendering::ShaderProgram> Shader;
    bool bInitialized = false;
};
