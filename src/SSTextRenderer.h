#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <stb/stb_truetype.h>

namespace Rendering { class ShaderProgram; }

// Basic screen-space text renderer. Bakes a TTF into a single-channel bitmap font atlas up
// front, then draws strings as textured quads in pixel coordinates (origin top-left, Y down).
// Single-line only - no wrapping or newline handling.
class SSTextRenderer
{
public:
    SSTextRenderer() = default;
    ~SSTextRenderer();

    // Loads TTFFilename (resolved relative to the executable, like ShaderManager's shader paths)
    // and bakes ASCII 32..127 into a bitmap atlas at FontPixelHeight. Must be called after the GL
    // context exists. Returns false if the font file couldn't be read or the shader failed to link.
    bool Initialize(const std::string& TTFFilename, float FontPixelHeight = 24.0f);

    // Keeps the screen-space projection matching the window; call from a resize callback.
    void SetScreenSize(int Width, int Height);

    // Draws Text with (X, Y) as the baseline-left origin, in pixel coordinates.
    void DrawText(const std::string& Text, float X, float Y, const glm::vec3& Color = glm::vec3(1.0f), float Scale = 1.0f) const;

    // Width in pixels Text would occupy if drawn, for right-aligning/centering callers.
    float MeasureTextWidth(const std::string& Text, float Scale = 1.0f) const;

private:
    static constexpr int FirstChar = 32;
    static constexpr int NumChars = 96; // ASCII 32..127
    static constexpr int AtlasWidth = 512;
    static constexpr int AtlasHeight = 512;

    stbtt_bakedchar CharData[NumChars] = {};

    unsigned int AtlasTexture = 0;
    unsigned int VAO = 0;
    unsigned int VertexBuffer = 0;

    std::shared_ptr<Rendering::ShaderProgram> Shader;

    int ScreenWidth = 800;
    int ScreenHeight = 600;
    bool bInitialized = false;
};
