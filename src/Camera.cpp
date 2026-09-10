//
// Created by Ryanc on 10/28/2024.
//

#include "Camera.h"

Camera::Camera(double Width, double Height, double Near, double Far, ECameraProjectionMode Mode, double VerticalFOV)
{
    ProjectionMode = Mode;
    VerticalFieldOfView = VerticalFOV;
    SetClipDimensions(Width, Height, Near, Far);
}

void Camera::SetProjectionMode(const ECameraProjectionMode& Mode)
{
    ProjectionMode = Mode;
    UpdateProjectionMatrix();
}

void Camera::SetClipDimensions(const double Width, const double Height, const double Near, const double Far)
{
    ClipWidth = Width;
    ClipHeight = Height;
    NearClipDistance = Near;
    FarClipDistance = Far;
    UpdateProjectionMatrix();
}

void Camera::SetClipWidth(const double Width)
{
    ClipWidth = Width;
    UpdateProjectionMatrix();
}

void Camera::SetClipHeight(const double Height)
{
    ClipHeight = Height;
    UpdateProjectionMatrix();
}

void Camera::SetClipNear(const double Near)
{
    NearClipDistance = Near;
    UpdateProjectionMatrix();
}

void Camera::SetClipFar(const double Far)
{
    FarClipDistance = Far;
    UpdateProjectionMatrix();
}

const glm::mat4& Camera::GetProjectionMatrix() const
{
    return ProjectionMatrix;
}

glm::mat4 Camera::GetViewProjectionMatrix()
{
    return ProjectionMatrix * inverse(GetMatrix());
}

void Camera::MoveForward(const float Distance)
{
    AddTranslation(GetForwardVector() * Distance);
}

void Camera::MoveBackward(const float Distance)
{
    MoveForward(-Distance);
}

void Camera::MoveRight(const float Distance)
{
    AddTranslation(GetRightVector() * Distance);
}

void Camera::MoveLeft(const float Distance)
{
    MoveRight(-Distance);
}

void Camera::MoveUp(const float Distance)
{
    AddTranslation(WorldUp * Distance);
}

void Camera::MoveDown(const float Distance)
{
    MoveUp(-Distance);
}

void Camera::GlideForward(const float Distance)
{
    glm::vec3 GroundForward = GetForwardVector();
    GroundForward.y = 0.0f;
    if(glm::length(GroundForward) > 0.0001f)
    {
        AddTranslation(glm::normalize(GroundForward) * Distance);
    }
}

void Camera::GlideBackward(const float Distance)
{
    GlideForward(-Distance);
}

void Camera::Yaw(const float Degrees)
{
    RotateLocal(WorldUp, Degrees);
}

void Camera::Pitch(const float Degrees)
{
    const float ClampedDegrees = glm::clamp(AccumulatedPitchDegrees + Degrees, -MaxPitchDegrees, MaxPitchDegrees) - AccumulatedPitchDegrees;
    AccumulatedPitchDegrees += ClampedDegrees;
    RotateLocal(GetRightVector(), ClampedDegrees);
}

void Camera::Pan(const float RightAmount, const float UpAmount)
{
    AddTranslation(GetRightVector() * RightAmount + GetUpVector() * UpAmount);
}

void Camera::UpdateProjectionMatrix()
{
    ProjectionMatrix =
        ProjectionMode == ECameraProjectionMode::Perspective
            ? glm::perspective(glm::radians(VerticalFieldOfView), ClipWidth / ClipHeight, NearClipDistance, FarClipDistance)
            : glm::ortho(-ClipWidth * 0.5, ClipWidth * 0.5, -ClipHeight * 0.5, ClipHeight * 0.5, NearClipDistance, FarClipDistance);
}
