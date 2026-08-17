//
// Created by ryan on 5/26/24.
//

#include "glad/glad.h"
#include "myc/logging/logging.h"
#include "ShaderObject.h"
#include "ShaderProgram.h"

namespace Rendering {
    /** ShaderProgram::ShaderProgram
     * @param FilenameToLoad
     */
    ShaderProgram::ShaderProgram(const std::string& FilenameToLoad)
    {
        if (FilenameToLoad.empty())
        {
            ProgramName = std::string("default");
        }
        else
        {
            ProgramName = FilenameToLoad;
        }

        ProgramID = glCreateProgram();
    }

    /** ShaderProgram::AttachShaderObject
     *  Attaches the given shader object to this shader program
     * @param Object
     */
    void ShaderProgram::AttachShaderObject(const std::shared_ptr<ShaderObject> &Object)
    {
        if(Object == nullptr)
        {
            return;
        }

        LogInfo("attaching shader %s\n", Object->Filename.c_str());
        glAttachShader(ProgramID, Object->ObjectID);
        AttachedShaderObjects.push_back(Object);
        Object->ProgramsIncludedIn.push_back(std::shared_ptr<ShaderProgram>(this));
    }

    /** ShaderProgram::LinkShaderProgram
     * attempts to link the shader program shader objects together into a valid pipeline
     * uncompiled shader-objects will attempt to be compiled
     * @returns true if shader program linked successfully
     */
    bool ShaderProgram::LinkShaderProgram()
    {
        if (AttachedShaderObjects.size() < 2)
        {
            LogError("could not link shader, not enough shader objects attached\n");
            return false;
        }
        for(const auto& Shader : AttachedShaderObjects)
        {
            if(!Shader->bCompiled) {
                Shader->Compile();
            }
        }
        GLint linkStatus = GL_FALSE;
        int info_log_length = 0;

        LogInfo("linking \"%s\"...\n", ProgramName.c_str());
        glLinkProgram(ProgramID);
        glGetProgramiv(ProgramID, GL_LINK_STATUS, &linkStatus);

        //log error if linking failed
        if (linkStatus != GL_TRUE)
        {
            GLsizei actualSize;
            glGetShaderiv(ProgramID, GL_INFO_LOG_LENGTH, &info_log_length);

            char info_log[1024];
            glGetProgramInfoLog(ProgramID, info_log_length, &actualSize, info_log);

            LogError("error:\n");
            LogError("%s\n", info_log);

            return false;
        }

        return true;
    }

    /*** ShaderProgram::CompileAttachedShaders
     * attempts to compile attached shaders
     * @return true if all attached shaderobjects were compiled successfully
     */
    bool ShaderProgram::CompileAttachedShaders()
    {
        bool bSucceeding = true;
        for (int i = 0; i < AttachedShaderObjects.size() && bSucceeding; i++)
        {
            bSucceeding |= AttachedShaderObjects[i]->Compile();
        }

        return bSucceeding;
    }

    /*** ShaderProgram::ReloadShaderObjects
     * attempts to reload all shader objects
     */
    void ShaderProgram::ReloadShaderObjects()
    {
        bool bReloadSucceeded = true;
        //attempt to reload each attached shader object
        for(auto& Shader : AttachedShaderObjects)
        {
            bReloadSucceeded = bReloadSucceeded && Shader->Reload();
        }

        //attempt to link if reload succeeded
        if (bReloadSucceeded)
        {
            if (LinkShaderProgram())
            {
                LogInfo("reloaded shader \"%s\"\n", ProgramName.c_str());
            }
        }
    }

    GLuint ShaderProgram::GetProgramID() const
    {
        return ProgramID;
    }
}