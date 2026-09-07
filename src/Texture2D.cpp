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

    // Maps an empty-allocation internal format to the glTexImage2D upload format/type pair GL
    // requires for that internal format. Analogous to GLFormatForChannelCount above, but for
    // AllocateEmpty's internalformat-driven path rather than channel-count-driven pixel upload.
    struct FUploadFormatAndType
    {
        GLenum Format;
        GLenum Type;
    };

    FUploadFormatAndType UploadFormatForInternalFormat(GLenum InternalFormat)
    {
        switch(InternalFormat)
        {
            case GL_DEPTH24_STENCIL8: return {GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
            default: return {GL_RGBA, GL_UNSIGNED_BYTE};
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

    Texture2D::Texture2D(int InWidth, int InHeight, GLenum InternalFormat, GLenum WrapMode)
        : Width(InWidth), Height(InHeight)
    {
        AllocateEmpty(InternalFormat, WrapMode);
    }

    Texture2D::Texture2D(int InWidth, int InHeight, GLenum InternalFormat, GLsizei InSamples)
        : Width(InWidth), Height(InHeight)
    {
        AllocateEmptyMultisample(InternalFormat, InSamples);
    }

    void Texture2D::Resize(int NewWidth, int NewHeight, GLenum NewInternalFormat)
    {
        Width = NewWidth;
        Height = NewHeight;
        InternalFormat = NewInternalFormat;

        glBindTexture(Target, TextureID);

        if(Target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Samples, InternalFormat, Width, Height, GL_TRUE);
        }
        else
        {
            const FUploadFormatAndType UploadFormat = UploadFormatForInternalFormat(InternalFormat);
            glTexImage2D(GL_TEXTURE_2D, 0, (GLint)InternalFormat, Width, Height, 0, UploadFormat.Format, UploadFormat.Type, nullptr);
        }

        glBindTexture(Target, 0);
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
        : TextureID(Other.TextureID), Width(Other.Width), Height(Other.Height), Channels(Other.Channels),
          InternalFormat(Other.InternalFormat), Samples(Other.Samples), Target(Other.Target)
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
            InternalFormat = Other.InternalFormat;
            Samples = Other.Samples;
            Target = Other.Target;
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

    void Texture2D::AllocateEmpty(GLenum InInternalFormat, GLenum WrapMode)
    {
        InternalFormat = InInternalFormat;

        glGenTextures(1, &TextureID);
        glBindTexture(GL_TEXTURE_2D, TextureID);

        const FUploadFormatAndType UploadFormat = UploadFormatForInternalFormat(InternalFormat);
        glTexImage2D(GL_TEXTURE_2D, 0, (GLint)InternalFormat, Width, Height, 0, UploadFormat.Format, UploadFormat.Type, nullptr);

        //no mipmap filtering/generation - this is a render target that gets rewritten every frame,
        //so a mip chain would go stale immediately and building one would be wasted work
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint)WrapMode);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint)WrapMode);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Texture2D::AllocateEmptyMultisample(GLenum InInternalFormat, GLsizei InSamples)
    {
        InternalFormat = InInternalFormat;
        Samples = InSamples;
        Target = GL_TEXTURE_2D_MULTISAMPLE;

        glGenTextures(1, &TextureID);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, TextureID);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, Samples, InternalFormat, Width, Height, GL_TRUE);
        //deliberately no glTexParameteri calls here - filter/wrap/mipmap params are invalid on a
        //multisample texture (there's nothing to filter or wrap between - every sample is its own texel)
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
    }

    void Texture2D::Bind(unsigned int TextureUnit) const
    {
        glActiveTexture(GL_TEXTURE0 + TextureUnit);
        glBindTexture(Target, TextureID);
    }

    void Texture2D::Unbind() const
    {
        glBindTexture(Target, 0);
    }

    void Texture2D::SetMinifyingFunction(GLenum FilterFunction) const
    {
        //GL_TEXTURE_MIN_FILTER is invalid on a multisample texture - see AllocateEmptyMultisample
        if(Target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)FilterFunction);
    }

    void Texture2D::SetMagnifyingFunction(GLenum FilterFunction) const
    {
        if(Target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)FilterFunction);
    }

    void Texture2D::SetWrapMode(GLenum Axis, GLenum WrapMode) const
    {
        if(Target == GL_TEXTURE_2D_MULTISAMPLE)
        {
            return;
        }

        glBindTexture(GL_TEXTURE_2D, TextureID);
        glTexParameteri(GL_TEXTURE_2D, Axis, (GLint)WrapMode);
    }
}
