//
// Created by Ryanc on 10/28/2024.
//
#pragma once

#include "Transform.h"

enum class ECameraProjectionMode
{
    Perspective = 0,
    Orthographic
};

class Camera : public Transform
{
public:
    explicit Camera(double Width = 800,
                    double Height = 600,
                    double Near = 0.1,
                    double Far = 1000.0,
                    ECameraProjectionMode Mode = ECameraProjectionMode::Perspective,
                    double VerticalFOV = 45.0 );

    void SetProjectionMode(const ECameraProjectionMode& Mode);

    // Width/Height mean different things depending on ProjectionMode: pixel dimensions in
    // Perspective mode (only their ratio matters, for aspect ratio), but world-space frustum
    // size in Orthographic mode (a centered/symmetric frustum -HalfWidth..HalfWidth,
    // -HalfHeight..HalfHeight).
    void SetClipDimensions(double Width, double Height, double Near, double Far);
    void SetClipWidth(double Width);
    void SetClipHeight(double Height);
    void SetClipNear(double Near);
    void SetClipFar(double Far);

    const glm::mat4& GetProjectionMatrix() const;
    glm::mat4 GetViewProjectionMatrix();

    // Directional movement, relative to the camera's current orientation.
    void MoveForward(float Distance);
    void MoveBackward(float Distance);
    void MoveRight(float Distance);
    void MoveLeft(float Distance);

    // World-space vertical movement, independent of the camera's pitch/yaw.
    void MoveUp(float Distance);
    void MoveDown(float Distance);

    // Forward/backward movement projected onto the world XZ (ground) plane, so height never
    // changes regardless of the camera's current pitch - used by LMB glide.
    void GlideForward(float Distance);
    void GlideBackward(float Distance);

    // Yaw around the world up axis.
    void Yaw(float Degrees);

    // Pitch around the camera's local right axis, clamped to +/-MaxPitchDegrees so it can't flip
    // over. Tracks accumulated pitch internally so the clamp holds regardless of caller.
    void Pitch(float Degrees);

    // Screen-space pan: translate along the camera's own local Right/Up plane, no dolly. Shared by
    // MB3 pan, ortho-viewport drag-pan, and ortho scroll-zoom's cursor-recentering.
    void Pan(float RightAmount, float UpAmount);

protected:
    void UpdateProjectionMatrix();

private:
    ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
    double VerticalFieldOfView = 45.0;
    double ClipWidth = 800.0;
    double ClipHeight = 600.0;
    double NearClipDistance = 0.1;
    double FarClipDistance = 1000.0;
    glm::mat4 ProjectionMatrix = glm::mat4();

    static constexpr float MaxPitchDegrees = 89.0f;
    float AccumulatedPitchDegrees = 0.0f;
};
