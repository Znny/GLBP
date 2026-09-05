//
// Created by Ryan on 8/30/2026.
//
#include "glad/glad.h"
#include "FramebufferAttachment.h"
namespace Rendering
{

    const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Color
            {
                    Rendering::EFramebufferAttachmentType::Renderbuffer,
                    GL_COLOR_ATTACHMENT0,
                    GL_RGB8,
                    1
            };
    const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_MultisampleColor
            {
                    Rendering::EFramebufferAttachmentType::Renderbuffer,
                    GL_COLOR_ATTACHMENT0,
                    GL_RGB8,
                    4
            };
    const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Depth
            {
                    Rendering::EFramebufferAttachmentType::Renderbuffer,
                    GL_DEPTH_ATTACHMENT,
                    GL_DEPTH_COMPONENT,
                    1
            };
    const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_Stencil
            {
                    Rendering::EFramebufferAttachmentType::Renderbuffer,
                    GL_STENCIL_ATTACHMENT,
                    GL_STENCIL_INDEX8,
                    1
            };
    const FFramebufferAttachmentSpec DefaultRenderBufferFramebufferAttachment_DepthStencil
            {
                    Rendering::EFramebufferAttachmentType::Renderbuffer,
                    GL_DEPTH_STENCIL_ATTACHMENT,
                    GL_DEPTH24_STENCIL8,
                    1
            };
    const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Color
            {
                    Rendering::EFramebufferAttachmentType::Texture,
                    GL_COLOR_ATTACHMENT0,
                    GL_RGB,
                    1
            };
    const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_MultisampleColor
            {
                    Rendering::EFramebufferAttachmentType::Texture,
                    GL_COLOR_ATTACHMENT0,
                    GL_RGB,
                    4
            };
    const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Depth
            {
                    Rendering::EFramebufferAttachmentType::Texture,
                    GL_DEPTH_ATTACHMENT,
                    GL_DEPTH_COMPONENT,
                    1
            };
    const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_Stencil
            {
                    Rendering::EFramebufferAttachmentType::Texture,
                    GL_STENCIL_ATTACHMENT,
                    GL_STENCIL_INDEX8,
                    1
            };
    const FFramebufferAttachmentSpec DefaultTexturedFramebufferAttachment_DepthStencil
            {
                    Rendering::EFramebufferAttachmentType::Texture,
                    GL_DEPTH_STENCIL_ATTACHMENT,
                    GL_DEPTH24_STENCIL8,
            };
}