//
// Created by ryan on 5/26/24.
//

#include "ShaderProgram.h"
#include <cstring>
#include <cstdlib>
#include <stdio.h>

ShaderProgram::ShaderProgram(const char* name)
{
    if(name != nullptr)
    {
        Name = strndup(name, 1024);
    }
    else
    {
        Name = strndup("unspecified", 1024);
    }

    ProgramID = glCreateProgram();
}

void ShaderProgram::Attach(ShaderObject* Object)
{
    fprintf(stdout, "attaching shader %s\n", Object->Filename);
    glAttachShader(ProgramID, Object->ObjectID);
    AttachedShaderObjects.push_back(Object);
}

bool ShaderProgram::Link()
{
    if(AttachedShaderObjects.size() < 2)
    {
        fprintf(stdout, "could not link shader, not enough shader objects attached\n");
        return false;
    }
    GLint linkStatus = GL_FALSE;
    int info_log_length = 0;

    fprintf(stdout, "linking \"%s\"...\n", Name);
    glLinkProgram(ProgramID);
    glGetProgramiv(ProgramID, GL_LINK_STATUS, &linkStatus);

    //log error if linking failed
    if (linkStatus != GL_TRUE)
    {
        GLsizei actualSize;
        glGetShaderiv(ProgramID, GL_INFO_LOG_LENGTH, &info_log_length);

        char info_log[1024];
        glGetProgramInfoLog(ProgramID, info_log_length, &actualSize, info_log);

        fprintf(stdout, "error:\n");
        fprintf(stderr, "%s\n", info_log);

        return false;
    }

    return true;
}

bool ShaderProgram::Compile()
{
    bool bSucceeding = true;
    for(int i = 0; i < AttachedShaderObjects.size() && bSucceeding; i++)
    {
        bSucceeding |= AttachedShaderObjects[i]->Compile();
    }

    return bSucceeding;
}

void ShaderProgram::Reload()
{
    bool bSucceeding = true;
    for(int i = 0; i < AttachedShaderObjects.size(); i++)
    {
        bSucceeding = bSucceeding && AttachedShaderObjects[i]->Reload();
    }
    if(bSucceeding)
    {
        if(Link())
        {
            fprintf(stdout, "reloaded shader \"%s\"\n", Name);
        }
    }
}
