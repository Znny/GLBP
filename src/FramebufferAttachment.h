//
// Created by Ryan on 8/30/2026.
//

#pragma once

#include "Delegates.h"
#include "glTypes.h"

#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_RGB8 0x8051

namespace Rendering
{
    class Texture2D;

    // Selects the backing GL object for a Framebuffer's depth/stencil attachment.
    enum class EFramebufferAttachmentType
    {
        Renderbuffer, // GL_RENDERBUFFER - not sampleable, cheaper, correct default when nothing reads depth/stencil
        Texture       // sampleable Texture2D - for post-processing effects that need to read depth/stencil
    };
    const static EFramebufferAttachmentType DefaultFramebufferAttachmentType = EFramebufferAttachmentType::Renderbuffer;

    struct FFramebufferAttachmentSpec
    {
        EFramebufferAttachmentType AttachmentType = DefaultFramebufferAttachmentType;
        GLenum AttachmentPoint = GL_COLOR_ATTACHMENT0; //COLOR, DEPTH, STENCIL, DEPTH_STENCIL
        GLenum internalFormat = GL_RGB8;  //appropriate format for the given attachment point
        GLsizei samples = 1; //1 by default, >1 for multisampling render buffer
    };

    //default framebuffers, for easy use
    extern const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Color;
    extern const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_MultisampleColor;
    extern const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Depth;
    extern const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Stencil;
    extern const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_DepthStencil;
    extern const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Color;
    extern const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_MultisampleColor;
    extern const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Depth;
    extern const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Stencil;
    extern const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_DepthStencil;


    class FramebufferAttachment;

    // Fired only when this attachment's underlying GL object id has changed (a Renderbuffer<->Texture
    // type change) or its FBO attachment point changed - either way, the owning Framebuffer must
    // re-run glFramebufferRenderbuffer/glFramebufferTexture2D to point at it again. NOT fired for a
    // plain resize or format/sample-count change, since those reuse the existing id in place.
    DECLARE_DELEGATE_1Param(FOnAttachmentIdChanged, FramebufferAttachment*)

    class FramebufferAttachment
    {
    public:
        FramebufferAttachment(const FFramebufferAttachmentSpec& AttachmentSpec, int width, int height);

        ~FramebufferAttachment();

        // Resizes this attachment's storage, reusing the existing GL object id where possible
        // (see FOnAttachmentIdChanged above for when that's not possible).
        void Resize(int newWidth, int newHeight);

        FFramebufferAttachmentSpec& GetSpecReference();
        void SetSpec(const Rendering::FFramebufferAttachmentSpec &NewSpec);

        GLuint GetAttachmentID() const;

        Rendering::Texture2D* GetBackingTexture() const;

        FOnAttachmentIdChanged& GetOnAttachmentIdChangedDelegate();

    private:
        void CreateGLObject();
        void DestroyGLObject();
        void RecreateStorage();
        void UpdateAttachment();

        FOnAttachmentIdChanged OnAttachmentIdChangedDelegate;

        FFramebufferAttachmentSpec attachmentSpec = DefaultRenderBufferFramebufferAttachment_Color;
        int width = 0;
        int height = 0;
        GLuint attachmentID = 0;
        Texture2D* backingTexture = nullptr;

    };

}
