//
// Created by ryan on 5/26/24.
//
#pragma once

#include <memory>
#include <string>
#include <vector>

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    class ShaderProgram;

    class ShaderObject
    {
        friend class ShaderProgram;
    public:
        explicit ShaderObject(const std::string& filename, GLenum shaderType);
        ~ShaderObject();

        bool Load(const std::string& filename, GLenum shaderType);

        bool Reload();

        bool Compile();

    protected:
        //openGL shader object ID, used for bindings and attachment
        GLuint ObjectID;

        //type of shader object, valid values are defined by openGL
        GLenum Type;

        //file source was loaded from
        std::string Filename;

        //soure of the shader, loaded from a file
        std::string ShaderSource;
        std::vector<std::shared_ptr<ShaderProgram>> ProgramsIncludedIn;
        bool bCompiled = false;
    };
}

#undef GLuint
#undef GLenum
