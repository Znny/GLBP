# GLBP TODO

## Done
- Dear ImGui integration (docking, not viewports - see WSLg cursor-warp notes below)
- Vertex/index buffer + VAO abstraction (`VertexBuffer`/`IndexBuffer`/`VertexArray`)
- Texture2D support (`stb_image`, native channel counts, configurable wrap mode)
- Per-frame UBO for common shader inputs (`FrameConstants`: ViewProjectionMatrix,
  Resolution, CursorPosition, Time)
- Framebuffer/render-target abstraction (`Framebuffer`: FBO with a sampleable Texture2D
  color attachment, plus a depth/stencil attachment that's either a non-sampled renderbuffer
  (default) or a sampleable Texture2D, in-place `Resize()`). Proven out by the multi-viewport
  system below.
- Multi-viewport rendering: perspective (free-fly) + 3 stationary orthographic cameras
  (Top/Front/Right), 2x2 grid. Space toggles perspective-fullscreen vs. multi-view. Each
  viewport renders straight into the default framebuffer by default (`glViewport`+`glScissor`
  confines it to its quadrant - cheap, no offscreen target needed); any viewport can optionally
  be routed through its own `Framebuffer` instead (e.g. for future per-viewport post-processing)
  and gets composited via a blit shader - not required, and unused by default.

## Next up
- Shader playground tool (file-watch auto-reload + ImGui panel for live-tweaking
  uniforms and viewing compile errors)

## Ideas / not started
- GLSL `#include` support: GLSL has no native `#include` (the `ARB_shading_language_include`
  extension exists but is spotty/driver-dependent, not reliable at this project's GL 3.3
  floor). Would need to be faked at the application level in `ShaderObject::Load()`
  (`src/ShaderObject.cpp`), which already reads shader source into a `std::string` before
  compiling - scan for `#include "file.glsl"` lines and splice the target file's contents
  in before handing the combined source to `glShaderSource`. Two things worth doing
  alongside it:
  - Include guard (skip re-including a file already pulled in for this shader object) -
    GLSL has no `#pragma once`, so a diamond dependency would otherwise cause duplicate-
    declaration compile errors.
  - `#line N "filename"` directives at each splice point, so compiler error messages point
    at the right original file/line instead of the flattened combined source.
  Motivating use case: a `common.glsl` shared by multiple `.vs`/`.fs` files (e.g. so the
  `FrameConstants` uniform block declaration in `resource/textured.vs`/`.fs` doesn't need
  to be hand-copied into every shader that wants it).

## Known environment quirks (not action items, just context)
- Running natively under WSL/WSLg (not the mingw Windows build) hits real GLFW/XWayland
  cursor-warp bugs (see upstream glfw/glfw#2271) - `GLFW_CURSOR_DISABLED` behavior during
  RMB camera-fly can misbehave. `bUsingWSL` detection + on-screen warning already exists
  in `src/main.cpp`; building/running the Windows `.exe` (mingw target) sidesteps it
  entirely.
- ImGui's platform multi-viewport (`ImGuiConfigFlags_ViewportsEnable`) is deliberately left
  off for the same WSLg reason - only docking is enabled.
