#include "Grid.h"

#include <glad/glad.h>

#include "Camera.h"
#include "ShaderManager.h"
#include "ShaderProgram.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"

namespace
{
    // X=red, Y=green, Z=blue - matches Gizmo.cpp's AxisColor convention.
    glm::vec3 ColorForAxis(const glm::vec3& Axis)
    {
        if(Axis.x != 0.0f) return {0.9f, 0.15f, 0.15f};
        if(Axis.y != 0.0f) return {0.15f, 0.85f, 0.15f};
        return {0.2f, 0.4f, 0.95f};
    }
}

void Grid::Initialize()
{
    if(bInitialized)
    {
        return;
    }

    Shader = Rendering::ShaderManager::Get()->LoadShaderProgram("grid", "/resource/grid.vs", "/resource/grid.fs");

    const float QuadVerts[] =
    {
        //x,     y  (NDC space, no transform needed - see resource/grid.vs)
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };
    const GLuint QuadIndices[] = {0, 1, 2, 2, 3, 0};

    QuadMesh = std::make_unique<Rendering::VertexArray>();
    QuadMesh->AddVertexBuffer(
        Rendering::VertexBuffer(QuadVerts, sizeof(QuadVerts), GL_STATIC_DRAW),
        {Rendering::FVertexAttribute{0, 2, GL_FLOAT, false}},
        2 * sizeof(float));
    QuadMesh->SetIndexBuffer(Rendering::IndexBuffer(QuadIndices, 6, GL_STATIC_DRAW));

    bInitialized = true;
}

void Grid::Draw(Camera& ViewportCamera, const glm::vec3& PlaneTangent, const glm::vec3& PlaneBitangent) const
{
    if(!bInitialized || !Shader || !QuadMesh)
    {
        return;
    }

    const glm::mat4 ViewProjectionMatrix = ViewportCamera.GetViewProjectionMatrix();
    const glm::mat4 InverseViewProjectionMatrix = glm::inverse(ViewProjectionMatrix);
    const glm::vec3 CameraWorldPosition = ViewportCamera.GetLocation();
    const glm::vec3 TangentAxisColor = ColorForAxis(PlaneTangent);
    const glm::vec3 BitangentAxisColor = ColorForAxis(PlaneBitangent);

    const GLuint ProgramID = Shader->GetProgramID();
    glUseProgram(ProgramID);
    glUniformMatrix4fv(glGetUniformLocation(ProgramID, "InverseViewProjectionMatrix"), 1, GL_FALSE, &InverseViewProjectionMatrix[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(ProgramID, "ViewProjectionMatrix"), 1, GL_FALSE, &ViewProjectionMatrix[0][0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "CameraWorldPosition"), 1, &CameraWorldPosition[0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "PlaneTangent"), 1, &PlaneTangent[0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "PlaneBitangent"), 1, &PlaneBitangent[0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "TangentAxisColor"), 1, &TangentAxisColor[0]);
    glUniform3fv(glGetUniformLocation(ProgramID, "BitangentAxisColor"), 1, &BitangentAxisColor[0]);

    //feathered/faded edges need to blend over whatever's already in the framebuffer (scene
    //geometry drawn this frame, or the clear color) - restored to disabled after, since nothing
    //else in the per-viewport render loop currently relies on blending being enabled
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    QuadMesh->Draw(GL_TRIANGLES);

    glDisable(GL_BLEND);
}
