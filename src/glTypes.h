//
// Created by Ryan on 8/30/2026.
//
#pragma once

typedef unsigned int GLenum;
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;

// Raw GL format values needed by more than one header (Texture2D.h and Framebuffer.h both want
// these without including each other's full definition) - named descriptively rather than after
// the real GL macros (GL_RGBA8, GL_DEPTH24_STENCIL8) to avoid colliding with glad.h's #define'd
// versions of the same names in any .cpp that includes both.
constexpr GLenum DefaultColorAttachmentFormat = 0x8058;    // GL_RGBA8
constexpr GLenum DefaultDepthStencilFormat = 0x88F0;       // GL_DEPTH24_STENCIL8

