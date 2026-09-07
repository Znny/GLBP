#pragma once

#include <memory>
#include <vector>

#include "glTypes.h"

namespace Rendering
{
    class FramebufferAttachment;
    struct FFramebufferAttachmentSpec;

    // Describes the size and the list of attachments (color/depth/stencil, each independently
    // Renderbuffer- or Texture-backed per FFramebufferAttachmentSpec) a Framebuffer is built with.
    struct FFramebufferSpec
    {
        int Width;
        int Height;
        std::vector<FFramebufferAttachmentSpec> Attachments;
    };

    // Offscreen render target: an FBO owning a dynamic list of FramebufferAttachments (color,
    // depth, stencil, depth+stencil - whatever FFramebufferSpec::Attachments asks for), each
    // independently Renderbuffer- or Texture-backed. Bind() redirects rendering into it and sets
    // the GL viewport to its own size; Unbind() only rebinds the default framebuffer (id 0) - it
    // does NOT restore any previous viewport, that's the caller's responsibility (mirrors how
    // WindowResizeEventCallback already calls glViewport directly rather than this class tracking
    // window state it has no other reason to know about).
    //
    // Attachment lifecycle: AddAttachment() either updates an existing attachment at that
    // AttachmentPoint (via FramebufferAttachment::SetSpec) or constructs a new one and binds its
    // FOnAttachmentIdChanged delegate to OnAttachmentIdChanged() below - decoupled via TDelegate
    // (Delegates.h) rather than a hardcoded callback type, so neither header needs the other's
    // full definition. A plain resize/format change reuses the attachment's existing GL id and
    // never fires that delegate; only a Renderbuffer<->Texture type change (or attachment point
    // change) does, since only then does the FBO's glFramebufferRenderbuffer/glFramebufferTexture2D
    // binding actually need to be redone.
    class Framebuffer
    {
    public:
        explicit Framebuffer(const FFramebufferSpec& Spec);

        // Declared here but defined (not defaulted) in Framebuffer.cpp, not = default'd inline -
        // Attachments is a vector<unique_ptr<FramebufferAttachment>> with FramebufferAttachment
        // only forward-declared above, so the compiler can't generate these here (it would need
        // FramebufferAttachment's full definition to know how to destroy it). Defining them in the
        // .cpp, after #include "FramebufferAttachment.h", gives the compiler what it needs at the
        // point they're instantiated.
        ~Framebuffer();

        Framebuffer(const Framebuffer&) = delete;
        Framebuffer& operator=(const Framebuffer&) = delete;

        Framebuffer(Framebuffer&& Other) noexcept;
        Framebuffer& operator=(Framebuffer&& Other) noexcept;

        void Bind() const;
        void Unbind() const;

        // Resizes every attachment in place, keeping the FBO id (and each attachment's GL id)
        // stable. No-ops if NewWidth/NewHeight match the current size.
        void Resize(int NewWidth, int NewHeight);

        GLuint GetFramebufferID() const { return FramebufferID; }

        void BindColorAttachment(unsigned int AttachmentPoint) const;

        int GetWidth() const { return Width; }
        int GetHeight() const { return Height; }

        // Finds the existing attachment at AttachmentSpec.AttachmentPoint and updates its spec, or
        // constructs a new attachment if none exists yet at that point.
        void AddAttachment(const FFramebufferAttachmentSpec& AttachmentSpec);

        // Resyncs this Framebuffer to NewSpec: resizes to NewSpec.Width/Height, then adds or
        // updates (via AddAttachment) every attachment point NewSpec.Attachments lists, and
        // destroys any existing attachment at a point NewSpec no longer lists - the end result
        // matches what constructing fresh from NewSpec would have produced.
        void SetSpec(const FFramebufferSpec& NewSpec);

    private:
        // Runs glFramebufferRenderbuffer/glFramebufferTexture2D for Attachment against this FBO.
        // Called once when an attachment is first added, and again from OnAttachmentIdChanged
        // whenever an attachment's GL id (or attachment point) changes out from under it.
        void AttachToFramebuffer(FramebufferAttachment& Attachment) const;

        // Bound (via TDelegate::BindRaw) to each attachment's FOnAttachmentIdChanged delegate.
        void OnAttachmentIdChanged(FramebufferAttachment* Attachment);

        GLuint FramebufferID = 0;
        int Width = 0;
        int Height = 0;
        std::vector<std::unique_ptr<FramebufferAttachment>> Attachments;
    };
}
