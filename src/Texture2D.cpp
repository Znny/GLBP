#include "Texture2D.h"

#include "glad/glad.h"

#include <stb/stb_image.h>

#include "myc/logging/logging.h"
#include "myc/paths/paths.h"

namespace
{
    // Used as both the internalformat and format arguments to glTexImage2D - the base (unsized)
    // format enums are valid for both, and keeping them in lockstep like this avoids the internal/
    // upload format ever silently drifting apart from the channel count actually uploaded.
    GLenum GLFormatForChannelCount(int Channels)
    {
        switch(Channels)
        {
            case 1: return GL_RED;
            case 2: return GL_RG;
            case 3: return GL_RGB;
            case 4: return GL_RGBA;
            default: return GL_RGBA;
        }
    }
}

namespace Rendering
{
    Texture2D::Texture2D(const std::string& Filename, GLenum WrapMode)
    {
        stbi_set_flip_vertically_on_load(true);

        const std::string FullPath = myc::GetExecutableDir() + Filename;

        unsigned char* PixelData = stbi_load(FullPath.c_str(), &Width, &Height, &Channels, 0);
        if(!PixelData)
        {
            LogError("Failed to load texture \"%s\": %s\n", FullPath.c_str(), stbi_failure_reason());
            return;
        }

        Upload(PixelData, WrapMode);
        stbi_image_free(PixelData);
    }

    Texture2D::Texture2D(const unsigned char* PixelData, int InWidth, int InHeight, int InChannels, GLenum WrapMode)
        : Width(InWidth), Height(InHeight), Channels(InChannels)
    {
        Upload(PixelData, WrapMode);
    }

    Texture2D::~Texture2D()
    {
        if(glIsTexture(TextureID))
        {
            glDeleteTextures(1, &TextureID);
            TextureID = 0;
        }
    }

    Texture2D::Texture2D(Texture2D&& Other) noexcept
        : TextureID(Other.TextureID), Width(Other.Width), Height(Other.Height), Channels(Other.Channels)
    {
        Other.TextureID = 0;
    }

    Texture2D& Texture2D::operator=(Texture2D&& Other) noexcept
    {
        if(this != &Other)
        {
            if(glIsTexture(TextureID))
            {
                glDeleteTextures(1, &TextureID);
            }

            TextureID = Other.TextureID;
            Width = Other.Width;
            Height = Other.Height;
            Channels = Other.Channels;
            Other.TextureID = 0;
        }

        return *this;
    }

    void Texture2D::Upload(const unsigned char* PixelData, GLenum WrapMode)
    {
        const GLenum Format = GLFormatForChannelCount(Channels);

        glGenTextures(1, &TextureID);
        glBindTexture(GL_TEXTURE_2D, TextureID);

        //rows aren't guaranteed to be a multiple of GL's default 4-byte unpack alignment for
        //channel counts other than 4 (or widths that aren't multiples of 4) - without this, those
        //uploads come out sheared
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)Format, Width, Height, 0, Format, GL_UNSIGNED_BYTE, PixelData);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)WrapMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)WrapMode);
        glGenerateMipmap(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture2D::Bind(unsigned int TextureUnit) const
    {
        glActiveTexture(GL_TEXTURE0 + TextureUnit);
        glBindTexture(GL_TEXTURE_2D, TextureID);
    }

    void Texture2D::Unbind() const
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
