#include "Framebuffer.h"

#include <algorithm>

#include "glad/glad.h"

#include "gear/logging/logging.h"
#include "FramebufferAttachment.h"
#include "Texture2D.h"

namespace Rendering
{
    Framebuffer::Framebuffer(const FFramebufferSpec& Spec)
        : Width(Spec.Width), Height(Spec.Height)
    {
        glGenFramebuffers(1, &FramebufferID);

        for(const FFramebufferAttachmentSpec& AttachmentSpec : Spec.Attachments)
        {
            AddAttachment(AttachmentSpec);
        }

        UpdateResolveTarget();
    }

    Framebuffer::~Framebuffer()
    {
        if(glIsFramebuffer(FramebufferID))
        {
            glDeleteFramebuffers(1, &FramebufferID);
            FramebufferID = 0;
        }
    }

    void Framebuffer::Bind() const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);
        glViewport(0, 0, Width, Height);
    }

    void Framebuffer::Unbind() const
    {
        //only unbinds the FBO target - does NOT restore any previous viewport, that's the
        //caller's responsibility (see the class comment in Framebuffer.h)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::Resize(int NewWidth, int NewHeight)
    {
        if(NewWidth == Width && NewHeight == Height)
        {
            return;
        }

        Width = NewWidth;
        Height = NewHeight;

        for(auto& Attachment : Attachments)
        {
            Attachment->Resize(Width, Height);
        }

        UpdateResolveTarget();
    }

    void Framebuffer::AddAttachment(const FFramebufferAttachmentSpec& AttachmentSpec)
    {
        for(auto& ExistingAttachment : Attachments)
        {
            if(ExistingAttachment->GetSpecReference().AttachmentPoint == AttachmentSpec.AttachmentPoint)
            {
                ExistingAttachment->SetSpec(AttachmentSpec);
                return;
            }
        }

        auto NewAttachment = std::make_unique<FramebufferAttachment>(AttachmentSpec, Width, Height);
        NewAttachment->GetOnAttachmentIdChangedDelegate().BindRaw<Framebuffer, &Framebuffer::OnAttachmentIdChanged>(this);
        AttachToFramebuffer(*NewAttachment);
        Attachments.push_back(std::move(NewAttachment));
    }

    void Framebuffer::SetSpec(const FFramebufferSpec& NewSpec)
    {
        Resize(NewSpec.Width, NewSpec.Height);

        for(const FFramebufferAttachmentSpec& AttachmentSpec : NewSpec.Attachments)
        {
            AddAttachment(AttachmentSpec);
        }

        // drop any attachment NewSpec no longer lists, so the result matches a fresh construction
        Attachments.erase(std::remove_if(Attachments.begin(), Attachments.end(),
            [&NewSpec](const std::unique_ptr<FramebufferAttachment>& Existing)
            {
                const GLenum AttachmentPoint = Existing->GetSpecReference().AttachmentPoint;
                return std::none_of(NewSpec.Attachments.begin(), NewSpec.Attachments.end(),
                    [AttachmentPoint](const FFramebufferAttachmentSpec& Spec)
                    {
                        return Spec.AttachmentPoint == AttachmentPoint;
                    });
            }), Attachments.end());

        UpdateResolveTarget();
    }

    void Framebuffer::AttachToFramebuffer(FramebufferAttachment& Attachment) const
    {
        glBindFramebuffer(GL_FRAMEBUFFER, FramebufferID);

        const FFramebufferAttachmentSpec& Spec = Attachment.GetSpecReference();
        if(Spec.AttachmentType == EFramebufferAttachmentType::Renderbuffer)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, Spec.AttachmentPoint, GL_RENDERBUFFER, Attachment.GetAttachmentID());
        }
        else
        {
            glFramebufferTexture2D(GL_FRAMEBUFFER, Spec.AttachmentPoint, Attachment.GetGLTextureTarget(), Attachment.GetAttachmentID(), 0);
        }

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            LogError("Framebuffer %dx%d incomplete after attaching point 0x%X\n", Width, Height, Spec.AttachmentPoint);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Framebuffer::OnAttachmentIdChanged(FramebufferAttachment* Attachment)
    {
        AttachToFramebuffer(*Attachment);
    }

    void Framebuffer::BindColorAttachment(unsigned int TextureUnit) const
    {
        //a multisampled color attachment can't be sampled directly - bind the resolved copy
        //instead (see ResolveMultisampledColor/UpdateResolveTarget)
        if(ResolveTarget)
        {
            ResolveTarget->BindColorAttachment(TextureUnit);
            return;
        }

        for(const auto& Attachment : Attachments)
        {
            if(Attachment->GetSpecReference().AttachmentPoint == GL_COLOR_ATTACHMENT0)
            {
                glActiveTexture(GL_TEXTURE0 + TextureUnit);
                glBindTexture(GL_TEXTURE_2D, Attachment->GetBackingTexture()->GetTextureID());;
                return;
            }
        }
    }

    void Framebuffer::ResolveMultisampledColor() const
    {
        if(!ResolveTarget)
        {
            return;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, FramebufferID);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, ResolveTarget->FramebufferID);
        glBlitFramebuffer(0, 0, Width, Height, 0, 0, Width, Height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    const FFramebufferAttachmentSpec* Framebuffer::FindColorAttachmentSpec() const
    {
        for(const auto& Attachment : Attachments)
        {
            if(Attachment->GetSpecReference().AttachmentPoint == GL_COLOR_ATTACHMENT0)
            {
                return &Attachment->GetSpecReference();
            }
        }

        return nullptr;
    }

    void Framebuffer::UpdateResolveTarget()
    {
        const FFramebufferAttachmentSpec* ColorSpec = FindColorAttachmentSpec();
        const bool bNeedsResolve = ColorSpec != nullptr && ColorSpec->samples > 1;

        if(!bNeedsResolve)
        {
            ResolveTarget.reset();
            return;
        }

        if(!ResolveTarget)
        {
            const FFramebufferSpec ResolveSpec
            {
                Width, Height,
                {FFramebufferAttachmentSpec{EFramebufferAttachmentType::Texture, GL_COLOR_ATTACHMENT0, ColorSpec->internalFormat, 1}}
            };
            ResolveTarget = std::make_unique<Framebuffer>(ResolveSpec);
        }
        else
        {
            ResolveTarget->Resize(Width, Height);
        }
    }

}
