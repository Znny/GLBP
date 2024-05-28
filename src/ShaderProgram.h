//
// Created by ryan on 5/26/24.
//

#ifndef GLBP_SHADERPROGRAM_H
#define GLBP_SHADERPROGRAM_H

#include "ShaderObject.h"
#include <vector>

class ShaderProgram
{
public:
    ShaderProgram(const char* name = nullptr);

    void Attach(ShaderObject* Object);
    bool Compile();
    bool Link();
    void Reload();

    //name of the shader program
    char* Name;

    //opengl shader program identifier
    GLuint ProgramID;

    //attached shader objects
    std::vector<ShaderObject*> AttachedShaderObjects;
};


#endif //GLBP_SHADERPROGRAM_H
