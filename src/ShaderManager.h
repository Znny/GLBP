//
// Created by Ryanc on 11/22/2024.
//

#pragma once
#include <memory>
#include <string>
#include <unordered_map>

typedef unsigned int GLenum;
namespace Rendering
{
    class ShaderObject;
    class ShaderProgram;

    class ShaderManager
    {
    public:
        static ShaderManager* Get();

        static void Initialize();

        std::shared_ptr<ShaderProgram> LoadShaderProgram(const std::string& ProgramName, const std::string& VertexShaderName, const std::string& FragmentShaderName);
        std::shared_ptr<ShaderObject> LoadShader(const std::string& ShaderName, GLenum ShaderType);

    private:
        ShaderManager() = default;
        ~ShaderManager();

        std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> ShaderPrograms;
        std::unordered_map<std::string, std::shared_ptr<ShaderObject>> ShaderObjects;

        static ShaderManager* sShaderManager;
    };
}

#undef GLenum