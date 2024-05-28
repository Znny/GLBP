//
// Created by ryan on 5/26/24.
//

#ifndef GLBP_SHADEROBJECT_H
#define GLBP_SHADEROBJECT_H


#include <glad/glad.h>

class ShaderObject
{
public:
    ShaderObject(const char* filename = nullptr, GLenum shaderType = GL_VERTEX_SHADER);

    bool Load(const char* filename, GLenum shaderType);
    bool Reload();
    bool Compile();

    //openGL shader object ID, used for bindings and attachment
    GLuint ObjectID;

    //type of shader object, valid values are defined by openGL
    GLenum Type;

    //file source was loaded from
    char* Filename;

    //soure of the shader, loaded from a file
    char* ShaderSource;
};


#endif //GLBP_SHADEROBJECT_H
