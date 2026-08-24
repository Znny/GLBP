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

void Camera::UpdateProjectionMatrix()
{
    ProjectionMatrix =
        ProjectionMode == ECameraProjectionMode::Perspective
            ? glm::perspective(glm::radians(VerticalFieldOfView), ClipWidth / ClipHeight, NearClipDistance, FarClipDistance)
            : glm::ortho(0.0, ClipWidth, 0.0, ClipHeight, NearClipDistance, FarClipDistance);
}
