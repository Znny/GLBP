//
// Created by Ryanc on 10/10/2024.
//

#include "Transform.h"

const glm::vec3 Transform::WorldForward{0.0f, 0.0f, 1.0f};
const glm::vec3 Transform::WorldRight{1.0f, 0.0f, 0.0f};
const glm::vec3 Transform::WorldUp{0.0f, 1.0f, 0.0f};

void Transform::Reset()
{
    Translation = (glm::vec3(0.0f)),
    Rotation = (glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),  // Identity quaternion
    Scale = (glm::vec3(1.0f));
}

void Transform::RotateLocal(const glm::vec3 &AxisOfRotation, const float Degrees)
{
    const glm::quat rotationQuat = glm::angleAxis(glm::radians(Degrees), glm::normalize(AxisOfRotation));
    Rotation = glm::normalize(rotationQuat * Rotation);
}

void Transform::RotateWorld(const glm::vec3 &AxisOfRotation, const float Degrees)
{
    const glm::quat rotationQuat = glm::angleAxis(glm::radians(Degrees), glm::normalize(AxisOfRotation));
    Translation = rotationQuat * Translation;// rotationQuat;
    Rotation = glm::normalize(rotationQuat * Rotation);
}

void Transform::RotateAroundPoint(const glm::vec3 &PointOfRotation, const glm::vec3 &AxisOfRotation, const float Degrees)
{
    Translation -= PointOfRotation;

    RotateWorld(AxisOfRotation, Degrees);

    Translation += PointOfRotation;
}

void Transform::AdjustYaw(const float Angle)
{
    //do the rotation
    RotateLocal(GetUpVector(), Angle);
}

void Transform::AdjustPitch(const float Angle)
{
    //do the rotation
    RotateLocal(GetRightVector(), Angle);
}

void Transform::AdjustRoll(const float Angle)
{
    //do the rotation
    RotateLocal(GetForwardVector(), Angle);
}
