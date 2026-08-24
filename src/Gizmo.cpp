#include "Gizmo.h"

#include <cmath>

#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

#include "ShaderManager.h"
#include "ShaderProgram.h"

namespace
{
    constexpr float TwoPi = 6.28318530717958647692f;

    // Axis 0/1/2 = X/Y/Z. AxisPerpU/V form a right-handed, cyclically-permuted basis
    // perpendicular to the axis so the same generator code produces all three handles.
    glm::vec3 AxisDirection(int Axis)
    {
        switch (Axis)
        {
            case 0:  return {1.0f, 0.0f, 0.0f};
            case 1:  return {0.0f, 1.0f, 0.0f};
            default: return {0.0f, 0.0f, 1.0f};
        }
    }

    glm::vec3 AxisPerpU(int Axis)
    {
        switch (Axis)
        {
            case 0:  return {0.0f, 1.0f, 0.0f};
            case 1:  return {0.0f, 0.0f, 1.0f};
            default: return {1.0f, 0.0f, 0.0f};
        }
    }

    glm::vec3 AxisPerpV(int Axis)
    {
        switch (Axis)
        {
            case 0:  return {0.0f, 0.0f, 1.0f};
            case 1:  return {1.0f, 0.0f, 0.0f};
            default: return {0.0f, 1.0f, 0.0f};
        }
    }

    glm::vec3 AxisColor(int Axis)
    {
        switch (Axis)
        {
            case 0:  return {0.9f, 0.15f, 0.15f}; // X - red
            case 1:  return {0.15f, 0.85f, 0.15f}; // Y - green
            default: return {0.2f, 0.4f, 0.95f};  // Z - blue
        }
    }

    void AddLine(std::vector<glm::vec3>& Positions, std::vector<glm::vec3>& Colors,
                 const glm::vec3& A, const glm::vec3& B, const glm::vec3& Color)
    {
        Positions.push_back(A);
        Positions.push_back(B);
        Colors.push_back(Color);
        Colors.push_back(Color);
    }

    void AddTriangle(std::vector<glm::vec3>& Positions, std::vector<glm::vec3>& Colors,
                      const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, const glm::vec3& Color)
    {
        Positions.push_back(A);
        Positions.push_back(B);
        Positions.push_back(C);
        Colors.push_back(Color);
        Colors.push_back(Color);
        Colors.push_back(Color);
    }

    // shaft + cone arrowhead, used by the translate handles
    void AddArrowAxis(std::vector<glm::vec3>& LinePos, std::vector<glm::vec3>& LineCol,
                       std::vector<glm::vec3>& TriPos, std::vector<glm::vec3>& TriCol, int Axis)
    {
        constexpr float ShaftLength = 0.8f;
        constexpr float HeadLength = 0.25f;
        constexpr float HeadRadius = 0.06f;
        constexpr int HeadSegments = 10;

        const glm::vec3 Dir = AxisDirection(Axis);
        const glm::vec3 U = AxisPerpU(Axis);
        const glm::vec3 V = AxisPerpV(Axis);
        const glm::vec3 Color = AxisColor(Axis);

        const glm::vec3 BaseCenter = Dir * ShaftLength;
        const glm::vec3 Apex = Dir * (ShaftLength + HeadLength);

        AddLine(LinePos, LineCol, glm::vec3(0.0f), BaseCenter, Color);

        for (int i = 0; i < HeadSegments; ++i)
        {
            const float Angle0 = TwoPi * (float)i / (float)HeadSegments;
            const float Angle1 = TwoPi * (float)(i + 1) / (float)HeadSegments;

            const glm::vec3 P0 = BaseCenter + (U * std::cos(Angle0) + V * std::sin(Angle0)) * HeadRadius;
            const glm::vec3 P1 = BaseCenter + (U * std::cos(Angle1) + V * std::sin(Angle1)) * HeadRadius;

            AddTriangle(TriPos, TriCol, Apex, P0, P1, Color);
            AddTriangle(TriPos, TriCol, BaseCenter, P1, P0, Color);
        }
    }

    // n-segment ring lying in the plane perpendicular to Axis, used by the rotate handles
    void AddRingAxis(std::vector<glm::vec3>& LinePos, std::vector<glm::vec3>& LineCol, int Axis)
    {
        constexpr float Radius = 1.0f;
        constexpr int Segments = 48;

        const glm::vec3 U = AxisPerpU(Axis);
        const glm::vec3 V = AxisPerpV(Axis);
        const glm::vec3 Color = AxisColor(Axis);

        for (int i = 0; i < Segments; ++i)
        {
            const float Angle0 = TwoPi * (float)i / (float)Segments;
            const float Angle1 = TwoPi * (float)(i + 1) / (float)Segments;

            const glm::vec3 P0 = (U * std::cos(Angle0) + V * std::sin(Angle0)) * Radius;
            const glm::vec3 P1 = (U * std::cos(Angle1) + V * std::sin(Angle1)) * Radius;

            AddLine(LinePos, LineCol, P0, P1, Color);
        }
    }

    // axis-aligned cube, used as the tip handle for the scale gizmo
    void AddBox(std::vector<glm::vec3>& TriPos, std::vector<glm::vec3>& TriCol,
                const glm::vec3& Center, float HalfExtent, const glm::vec3& Color)
    {
        const glm::vec3 Corners[8] =
        {
            Center + glm::vec3(-HalfExtent, -HalfExtent, -HalfExtent),
            Center + glm::vec3( HalfExtent, -HalfExtent, -HalfExtent),
            Center + glm::vec3( HalfExtent,  HalfExtent, -HalfExtent),
            Center + glm::vec3(-HalfExtent,  HalfExtent, -HalfExtent),
            Center + glm::vec3(-HalfExtent, -HalfExtent,  HalfExtent),
            Center + glm::vec3( HalfExtent, -HalfExtent,  HalfExtent),
            Center + glm::vec3( HalfExtent,  HalfExtent,  HalfExtent),
            Center + glm::vec3(-HalfExtent,  HalfExtent,  HalfExtent),
        };

        constexpr int FaceIndices[6][4] =
        {
            {0, 1, 2, 3}, // -Z
            {5, 4, 7, 6}, // +Z
            {4, 0, 3, 7}, // -X
            {1, 5, 6, 2}, // +X
            {4, 5, 1, 0}, // -Y
            {3, 2, 6, 7}, // +Y
        };

        for (const auto& Face : FaceIndices)
        {
            AddTriangle(TriPos, TriCol, Corners[Face[0]], Corners[Face[1]], Corners[Face[2]], Color);
            AddTriangle(TriPos, TriCol, Corners[Face[0]], Corners[Face[2]], Corners[Face[3]], Color);
        }
    }

    // shaft + box handle, used by the scale handles
    void AddScaleAxis(std::vector<glm::vec3>& LinePos, std::vector<glm::vec3>& LineCol,
                       std::vector<glm::vec3>& TriPos, std::vector<glm::vec3>& TriCol, int Axis)
    {
        constexpr float ShaftLength = 0.8f;
        constexpr float HalfBoxExtent = 0.06f;

        const glm::vec3 Dir = AxisDirection(Axis);
        const glm::vec3 Color = AxisColor(Axis);
        const glm::vec3 Tip = Dir * ShaftLength;

        AddLine(LinePos, LineCol, glm::vec3(0.0f), Tip, Color);
        AddBox(TriPos, TriCol, Tip, HalfBoxExtent, Color);
    }
}

void TransformGizmo::Initialize()
{
    if (bInitialized)
    {
        return;
    }

    Shader = Rendering::ShaderManager::Get()->LoadShaderProgram("gizmo", "/resource/gizmo.vs", "/resource/passthrough.fs");

    TranslateMesh = BuildTranslateMesh();
    RotateMesh = BuildRotateMesh();
    ScaleMesh = BuildScaleMesh();

    bInitialized = true;
}

TransformGizmo::FGizmoMesh TransformGizmo::BuildTranslateMesh()
{
    std::vector<glm::vec3> LinePos, LineCol, TriPos, TriCol;
    for (int Axis = 0; Axis < 3; ++Axis)
    {
        AddArrowAxis(LinePos, LineCol, TriPos, TriCol, Axis);
    }
    return UploadMesh(LinePos, LineCol, TriPos, TriCol);
}

TransformGizmo::FGizmoMesh TransformGizmo::BuildRotateMesh()
{
    std::vector<glm::vec3> LinePos, LineCol, TriPos, TriCol;
    for (int Axis = 0; Axis < 3; ++Axis)
    {
        AddRingAxis(LinePos, LineCol, Axis);
    }
    return UploadMesh(LinePos, LineCol, TriPos, TriCol);
}

TransformGizmo::FGizmoMesh TransformGizmo::BuildScaleMesh()
{
    std::vector<glm::vec3> LinePos, LineCol, TriPos, TriCol;
    for (int Axis = 0; Axis < 3; ++Axis)
    {
        AddScaleAxis(LinePos, LineCol, TriPos, TriCol, Axis);
    }
    return UploadMesh(LinePos, LineCol, TriPos, TriCol);
}

TransformGizmo::FGizmoMesh TransformGizmo::UploadMesh(const std::vector<glm::vec3>& LinePositions, const std::vector<glm::vec3>& LineColors,
                                                        const std::vector<glm::vec3>& TrianglePositions, const std::vector<glm::vec3>& TriangleColors)
{
    FGizmoMesh Mesh;
    Mesh.LineVertexCount = (int)LinePositions.size();
    Mesh.TriangleVertexCount = (int)TrianglePositions.size();

    std::vector<glm::vec3> AllPositions = LinePositions;
    AllPositions.insert(AllPositions.end(), TrianglePositions.begin(), TrianglePositions.end());

    std::vector<glm::vec3> AllColors = LineColors;
    AllColors.insert(AllColors.end(), TriangleColors.begin(), TriangleColors.end());

    glGenVertexArrays(1, &Mesh.VAO);
    glBindVertexArray(Mesh.VAO);

    glGenBuffers(1, &Mesh.PositionBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Mesh.PositionBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(AllPositions.size() * sizeof(glm::vec3)), AllPositions.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    glGenBuffers(1, &Mesh.ColorBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, Mesh.ColorBuffer);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(AllColors.size() * sizeof(glm::vec3)), AllColors.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    return Mesh;
}

void TransformGizmo::Draw(const Transform& Target, const glm::vec3& CameraLocation, const glm::mat4& ViewProjectionMatrix)
{
    if (!bInitialized || !Shader)
    {
        return;
    }

    const FGizmoMesh* Mesh = &TranslateMesh;
    if (Mode == EGizmoMode::Rotate)
    {
        Mesh = &RotateMesh;
    }
    else if (Mode == EGizmoMode::Scale)
    {
        Mesh = &ScaleMesh;
    }

    const glm::vec3 TargetLocation = Target.GetLocation();

    // keep a roughly constant apparent size on screen regardless of distance from the camera
    const float DistanceToCamera = glm::length(CameraLocation - TargetLocation);
    const float GizmoScale = DistanceToCamera * 0.15f;

    const glm::mat4 ModelMatrix = glm::translate(glm::mat4(1.0f), TargetLocation) * glm::scale(glm::mat4(1.0f), glm::vec3(GizmoScale));

    const GLuint ProgramID = Shader->GetProgramID();
    glUseProgram(ProgramID);
    glUniformMatrix4fv(glGetUniformLocation(ProgramID, "ViewProjectionMatrix"), 1, GL_FALSE, &ViewProjectionMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(ProgramID, "ModelMatrix"), 1, GL_FALSE, &ModelMatrix[0][0]);

    // gizmo always draws on top, like most editors' viewport overlays
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(Mesh->VAO);
    if (Mesh->LineVertexCount > 0)
    {
        glDrawArrays(GL_LINES, 0, Mesh->LineVertexCount);
    }
    if (Mesh->TriangleVertexCount > 0)
    {
        glDrawArrays(GL_TRIANGLES, Mesh->LineVertexCount, Mesh->TriangleVertexCount);
    }
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void TransformGizmo::ApplyTransformDelta(Transform& Target, EGizmoAxis Axis, float Delta) const
{
    switch (Mode)
    {
        case EGizmoMode::Translate:
            ApplyTranslationDelta(Target, Axis, Delta);
            break;
        case EGizmoMode::Rotate:
            ApplyRotationDelta(Target, Axis, Delta);
            break;
        case EGizmoMode::Scale:
            ApplyScaleDelta(Target, Axis, Delta);
            break;
    }
}

void TransformGizmo::ApplyTranslationDelta(Transform& Target, EGizmoAxis Axis, float Delta) const
{
    // TODO: once handle picking exists, translate Target along the world-space Axis by Delta,
    // e.g. Target.AddTranslation(AxisDirection(Axis) * Delta);
}

void TransformGizmo::ApplyRotationDelta(Transform& Target, EGizmoAxis Axis, float Delta) const
{
    // TODO: once handle picking exists, rotate Target around the world-space Axis by Delta degrees,
    // e.g. Target.RotateWorld(AxisDirection(Axis), Delta);
}

void TransformGizmo::ApplyScaleDelta(Transform& Target, EGizmoAxis Axis, float Delta) const
{
    // TODO: once handle picking exists, add Delta to Target's scale component along Axis,
    // e.g. Target.SetScale(Target.GetScale() + AxisDirection(Axis) * Delta);
}
