#pragma once

#include <string>

typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace Rendering
{
    // GL_REPEAT's value from the GL spec (0x2901) - stable across all conformant GL implementations.
    // Used as this header's default WrapMode so callers can omit it without this header needing to
    // include glad.h just for one enum constant (same reasoning as the local GLuint/GLenum typedefs above).
    constexpr GLenum DefaultTextureWrapMode = 0x2901;

    class Texture2D
    {
    public:
        // Loads Filename (resolved relative to the executable, like ShaderManager's shader paths)
        // via stb_image, preserving its native channel count (1/2/3/4 -> grey/grey-alpha/RGB/RGBA)
        // rather than forcing everything to RGBA. WrapMode is applied to both the S and T axes.
        explicit Texture2D(const std::string& Filename, GLenum WrapMode = DefaultTextureWrapMode);

        // Builds a texture directly from an in-memory pixel buffer (Width * Height * Channels bytes,
        // row-major, no padding). No file involved - useful for procedural textures/UI atlases.
        Texture2D(const unsigned char* PixelData, int Width, int Height, int Channels = 4, GLenum WrapMode = DefaultTextureWrapMode);

        ~Texture2D();

        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        Texture2D(Texture2D&& Other) noexcept;
        Texture2D& operator=(Texture2D&& Other) noexcept;

        void Bind(unsigned int TextureUnit = 0) const;
        void Unbind() const;

        GLuint GetTextureID() const { return TextureID; }
        int GetWidth() const { return Width; }
        int GetHeight() const { return Height; }
        int GetChannelCount() const { return Channels; }

    private:
        void Upload(const unsigned char* PixelData, GLenum WrapMode);

        GLuint TextureID = 0;
        int Width = 0;
        int Height = 0;
        int Channels = 0;
    };
}

#undef GLuint
#undef GLenum
