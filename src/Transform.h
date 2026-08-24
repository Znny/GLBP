#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

class Transform
{
public:
    Transform()
        : Translation(glm::vec3(0.0f)),
          Rotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f)),  // Identity quaternion
          Scale(glm::vec3(1.0f))
    {}

    static const glm::vec3 WorldForward;
    static const glm::vec3 WorldRight;
    static const glm::vec3 WorldUp;

    void Reset();

    // Getters
    glm::vec3 GetTranslation() const { return Translation; }
    glm::vec3 GetLocation() const { return Translation; }
    glm::quat GetRotation() const { return Rotation; }
    glm::vec3 GetScale() const { return Scale; }

    // Setters
    void SetTranslation(const glm::vec3& v) { Translation = v; }
    void SetLocation(const glm::vec3& v) { Translation = v; }
    void SetRotation(const glm::vec3& eulerAngles) { Rotation = glm::quat(glm::radians(eulerAngles)); }
    void SetRotation(const glm::mat3& rotationMat) { Rotation = glm::quat(rotationMat); }
    void SetRotation(const glm::quat& rotationQuat) { Rotation = rotationQuat; }
    void SetScale(const glm::vec3& v) { Scale = v; }

    // Directional Vectors
    glm::vec3 GetForwardVector() const { return glm::normalize(Rotation * WorldForward); }
    glm::vec3 GetRightVector() const { return glm::normalize(Rotation * WorldRight); }
    glm::vec3 GetUpVector() const { return glm::normalize(Rotation * WorldUp); }

    // Transform Modifiers
    void AddTranslation(const glm::vec3& v) { Translation += v; }

    void RotateLocal(const glm::vec3& AxisOfRotation, float Degrees);
    void RotateWorld(const glm::vec3& AxisOfRotation, float Degrees);
    void RotateAroundPoint(const glm::vec3& PointOfRotation, const glm::vec3& AxisOfRotation, float Degrees);

     /***
     * @brief Yaw by a given angle (default 90 degrees)
     * @param Angle - the angle to rotate by
     */
    void AdjustYaw(float Angle = 90.0f);

    /***
     * @brief Pitch by a given angle(default 90 degrees)
     * @param Angle - the angle to rotate by
     */
    void AdjustPitch(float Angle = 90.0f);

    /***
     * @brief Roll by a given angle(default 90 degrees)
     * @param Angle - the angle to rotate by
     */
    void AdjustRoll(float Angle = 90.0f);

    // Transformation Matrix
    glm::mat4 GetMatrix()
    {
        const glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), Translation);
        const glm::mat4 rotationMatrix = glm::toMat4(Rotation);
        const glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), Scale);
        return Matrix = translationMatrix * rotationMatrix * scaleMatrix;
    }

protected:
    glm::vec3 Translation = glm::vec3(0);
    glm::quat Rotation = glm::quat();
    glm::vec3 Scale = glm::vec3(1);
    glm::mat4 Matrix = glm::mat4();
};
