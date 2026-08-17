#include "ShaderManager.h"
#include "ShaderObject.h"
#include "ShaderProgram.h"
#include "glad/glad.h"
#include <iostream>

#include "myc/paths/paths.h"

// Initialize the static singleton instance
Rendering::ShaderManager* Rendering::ShaderManager::sShaderManager = nullptr;

namespace Rendering {
    // Singleton instance getter
    ShaderManager* ShaderManager::Get()
    {
        if (!sShaderManager)
        {
            sShaderManager = new ShaderManager();
        }
        return sShaderManager;
    }

    // Destructor
    ShaderManager::~ShaderManager()
    {
        ShaderPrograms.clear();
        ShaderObjects.clear();
    }

    // Initialize resources if necessary
    void ShaderManager::Initialize()
    {
        std::cout << "ShaderManager initialized." << std::endl;
    }

    // Load or retrieve a ShaderObject
    std::shared_ptr<ShaderObject> ShaderManager::LoadShader(const std::string &ShaderName, GLenum ShaderType)
    {
        // Check if the shader already exists
        if (ShaderObjects.find(ShaderName) != ShaderObjects.end())
        {
            return ShaderObjects[ShaderName];
        }

        // Create a new ShaderObject
        auto shader = std::make_shared<ShaderObject>(ShaderName, ShaderType);

        // Store and return the shader
        ShaderObjects[ShaderName] = shader;
        return ShaderObjects[ShaderName];
    }

    // Load or retrieve a ShaderProgram
    std::shared_ptr<ShaderProgram> ShaderManager::LoadShaderProgram(const std::string& ProgramName, const std::string& VertexShaderName, const std::string& FragmentShaderName)
    {
        // Check if the program already exists
        if (ShaderPrograms.find(ProgramName) != ShaderPrograms.end())
        {
            return ShaderPrograms[ProgramName];
        }

        std::string ExecutablePath = myc::GetExecutableDir();
        // Retrieve or create the vertex and fragment shaders
        auto vertexShader = LoadShader(ExecutablePath + VertexShaderName, GL_VERTEX_SHADER);
        auto fragmentShader = LoadShader(ExecutablePath + FragmentShaderName, GL_FRAGMENT_SHADER);

        if (!vertexShader || !fragmentShader)
        {
            std::cerr << "Failed to load shaders for program: " << ProgramName << std::endl;
            return nullptr;
        }

        // Create a new ShaderProgram
        auto program = std::make_shared<ShaderProgram>(ProgramName);
        program->AttachShaderObject(vertexShader);
        program->AttachShaderObject(fragmentShader);

        if (!program->LinkShaderProgram())
        {
            std::cerr << "Failed to link shader program: " << ProgramName << std::endl;
            return nullptr;
        }

        // Store and return the program
        ShaderPrograms[ProgramName] = program;
        return ShaderPrograms[ProgramName];
    }
}
