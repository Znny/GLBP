//
// Created by Ryan on 8/30/2026.
//

#include "FramebufferAttachment.h"
#include "glad/glad.h"
#include "Texture2D.h"

namespace Rendering
{
    FramebufferAttachment::FramebufferAttachment(const FFramebufferAttachmentSpec& inAttachmentSpec, int inWidth, int inHeight)
        : attachmentSpec(inAttachmentSpec), width(inWidth), height(inHeight)
    {
        CreateGLObject();
    }

    FramebufferAttachment::~FramebufferAttachment()
    {
        DestroyGLObject();
    }

    FFramebufferAttachmentSpec& FramebufferAttachment::GetSpecReference()
    {
        return attachmentSpec;
    }

    GLuint FramebufferAttachment::GetAttachmentID() const
    {
        return attachmentID;
    }

    Texture2D* FramebufferAttachment::GetBackingTexture() const
    {
        return backingTexture;
    }

    FOnAttachmentIdChanged& FramebufferAttachment::GetOnAttachmentIdChangedDelegate()
    {
        return OnAttachmentIdChangedDelegate;
    }

    void FramebufferAttachment::Resize(int newWidth, int newHeight)
    {
        if(newWidth == width && newHeight == height)
        {
            return;
        }

        width = newWidth;
        height = newHeight;
        RecreateStorage();
    }

    void FramebufferAttachment::SetSpec(const FFramebufferAttachmentSpec& NewSpec)
    {
        const bool bTypeChanged = NewSpec.AttachmentType != attachmentSpec.AttachmentType;
        const bool bAttachmentPointChanged = NewSpec.AttachmentPoint != attachmentSpec.AttachmentPoint;

        if(bTypeChanged)
        {
            //different kind of GL object entirely (Renderbuffer <-> Texture) - the old id is
            //meaningless to the new type, so destroy under the old spec, then create fresh under
            //the new one
            DestroyGLObject();
            attachmentSpec = NewSpec;
            CreateGLObject();
        }
        else
        {
            attachmentSpec = NewSpec;
            RecreateStorage();
        }

        if(bTypeChanged || bAttachmentPointChanged)
        {
            UpdateAttachment();
        }
    }

    void FramebufferAttachment::CreateGLObject()
    {
        if(attachmentSpec.AttachmentType == EFramebufferAttachmentType::Renderbuffer)
        {
            glGenRenderbuffers(1, &attachmentID);
            glBindRenderbuffer(GL_RENDERBUFFER, attachmentID);

            if(attachmentSpec.samples > 1)
            {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, attachmentSpec.samples, attachmentSpec.internalFormat, width, height);
            }
            else
            {
                glRenderbufferStorage(GL_RENDERBUFFER, attachmentSpec.internalFormat, width, height);
            }

            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        else
        {
            backingTexture = new Texture2D(width, height, attachmentSpec.internalFormat);
            attachmentID = backingTexture->GetTextureID();

            //raw depth values must not be interpolated when sampled - GL_LINEAR/mipmaps would
            //produce real sampling artifacts on a depth/stencil texture attachment
            const bool bIsDepthOrStencil = attachmentSpec.AttachmentPoint == GL_DEPTH_ATTACHMENT
                || attachmentSpec.AttachmentPoint == GL_STENCIL_ATTACHMENT
                || attachmentSpec.AttachmentPoint == GL_DEPTH_STENCIL_ATTACHMENT;
            if(bIsDepthOrStencil)
            {
                backingTexture->SetMinifyingFunction(GL_NEAREST);
                backingTexture->SetMagnifyingFunction(GL_NEAREST);
            }
        }
    }

    void FramebufferAttachment::DestroyGLObject()
    {
        if(attachmentSpec.AttachmentType == EFramebufferAttachmentType::Renderbuffer)
        {
            if(attachmentID != 0)
            {
                glDeleteRenderbuffers(1, &attachmentID);
            }
        }
        else
        {
            delete backingTexture;
            backingTexture = nullptr;
        }

        attachmentID = 0;
    }

    void FramebufferAttachment::RecreateStorage()
    {
        //same kind of GL object, new dimensions/format/samples - reuse the existing id so the
        //owning Framebuffer doesn't need to redo glFramebufferRenderbuffer/glFramebufferTexture2D
        if(attachmentSpec.AttachmentType == EFramebufferAttachmentType::Renderbuffer)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, attachmentID);

            if(attachmentSpec.samples > 1)
            {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, attachmentSpec.samples, attachmentSpec.internalFormat, width, height);
            }
            else
            {
                glRenderbufferStorage(GL_RENDERBUFFER, attachmentSpec.internalFormat, width, height);
            }

            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
        else
        {
            backingTexture->Resize(width, height, attachmentSpec.internalFormat);
        }
    }

    void FramebufferAttachment::UpdateAttachment()
    {
        if(OnAttachmentIdChangedDelegate.IsBound())
        {
            OnAttachmentIdChangedDelegate.Execute(this);
        }
    }
}
