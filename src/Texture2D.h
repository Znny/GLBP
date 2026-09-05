#pragma once

#include <string>

#include "glTypes.h"

namespace Rendering
{
    // GL_REPEAT's value from the GL spec (0x2901) - stable across all conformant GL implementations.
    // Used as this header's default WrapMode so callers can omit it without this header needing to
    // include glad.h just for one enum constant (same reasoning as the local GLuint/GLenum typedefs
    // in glTypes.h). DefaultColorAttachmentFormat/DefaultDepthStencilFormat used to live here too,
    // but moved to glTypes.h since Framebuffer.h also needs them without including this header.
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

        // Allocates empty GPU storage (no pixel upload) at Width x Height, for use as a render-target
        // attachment (e.g. Framebuffer's color attachment). No mipmaps are generated - a render target
        // gets rewritten every frame, so a mip chain would go stale immediately and building one would
        // be wasted work.
        Texture2D(int Width, int Height, GLenum InternalFormat = DefaultColorAttachmentFormat, GLenum WrapMode = DefaultTextureWrapMode);

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

        // Sets the texture minification filter (GL_TEXTURE_MIN_FILTER).
        // Used when the texture is being minified, i.e. when a screen pixel
        // corresponds to more than approximately one texel in texture space.
        // Determines how multiple texels are filtered to produce a pixel.
        // May use mipmap filtering modes.
        // See https://registry.khronos.org/OpenGL-Refpages/gl4/html/glTexParameter.xhtml
        // for all valid minification filter modes
        void SetMinifyingFunction(GLenum FilterFunction) const;

        // Sets the texture magnification filter (GL_TEXTURE_MAG_FILTER).
        // Used when the texture is being magnified, i.e. when a screen pixel
        // corresponds to less than approximately one texel in texture space.
        // Determines how texel samples are filtered as the texture is enlarged.
        // Only GL_NEAREST and GL_LINEAR are valid magnification filters.
        void SetMagnifyingFunction(GLenum Function) const;

        //Set texture wrap mode for the given axis.
        // Valid axes are GL_TEXTURE_WRAP_S and GL_TEXTURE_WRAP_T
        // Valid wrap modes are GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_MIRRORED_REPEAT,
        // GL_REPEAT, GL_MIRROR_CLAMP_TO_EDGE
        void SetWrapMode(GLenum Axis, GLenum WrapMode) const;

        // Reallocates this texture's storage at NewWidth x NewHeight (and optionally a new
        // internal format) in place - re-runs glTexImage2D on the existing TextureID rather than
        // generating a new one. Only meaningful for a texture built via the empty-allocation
        // constructor (a render target); lets a resized FramebufferAttachment keep the same GL id
        // so its owning Framebuffer doesn't need to re-attach it.
        void Resize(int NewWidth, int NewHeight, GLenum NewInternalFormat);

    private:
        void Upload(const unsigned char* PixelData, GLenum WrapMode = DefaultTextureWrapMode);
        void AllocateEmpty(GLenum InInternalFormat, GLenum WrapMode);

        GLuint TextureID = 0;
        int Width = 0;
        int Height = 0;
        int Channels = 0;
        GLenum InternalFormat = 0; // set by AllocateEmpty; unused for the file/pixel-upload constructors
    };
}
