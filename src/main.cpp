//
// Created by Ryan on 5/22/2024.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// header file includes

///opengl extension loader and glfw
#include <glad/glad.h>
#include <GLFW/glfw3.h>

//dear imgui
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

//glm
#include <glm/glm.hpp>

///std
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <string>
#include <memory>
#include <vector>
#include <array>

#include "ShaderProgram.h"
#include "ShaderObject.h"
#include "ShaderManager.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Texture2D.h"
#include "Framebuffer.h"
#include "UniformBuffer.h"
#include "FrameConstants.h"
#include "Camera.h"
#include "Gizmo.h"
#include "SSTextRenderer.h"
#include "ConfigManager.h"
#include "myc/logging/logging.h"
#include "myc/paths/paths.h"
#include "FramebufferAttachment.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Init(int argc, char** argv, char** envp);
bool InitGraphics();
bool CreateBestWindow();
bool InitInput();

void Run();
void UpdateTiming(GLFWwindow* window);
void Tick(double DeltaTime);
void Render(double DeltaTime);

void ProcessInput();
void RecomputeViewportQuadrants(int WindowWidth, int WindowHeight);
void CompositeFramebufferBackedViewports();
void UpdateCameraMovement(GLFWwindow* Window, double DeltaTime);
void KeyboardEventCallback(GLFWwindow* Window, int KeyCode, int ScanCode, int Action, int Modifiers);
void MouseButtonEventCallback(GLFWwindow* Window, int Button, int Action, int Modifiers);
void CursorPositionEventCallback(GLFWwindow* Window, double XPos, double YPos);
void WindowResizeEventCallback(GLFWwindow* Window, int NewWidth, int NewHeight);

void ErrorCallback(int error, const char* description);

void Cleanup();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//main window for the sim
GLFWwindow* MainWindow = nullptr;

constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight = 1080;

//world-space frustum size (see Camera::SetClipDimensions) for the 3 stationary orthographic
//viewport cameras - see RecomputeViewportQuadrants
constexpr double OrthoClipSize = 16.0;

static int Width = DefaultWidth;
static int Height = DefaultHeight;

// One quadrant of the 2x2 multi-view layout: a camera and the screen-space quadrant it renders
// into when multi-view mode is active. ViewportFramebuffer is null by default - the viewport
// renders straight into the default framebuffer, confined to its quadrant via glViewport+glScissor
// (see Render()). Explicitly assign a Framebuffer on a given viewport to opt it into offscreen
// rendering instead (e.g. for future per-viewport post-processing) - the render loop already
// handles that path. The perspective viewport is assigned one (from PerspectiveFramebufferSpec,
// see below) to prove this out; the 3 orthographic viewports are left on the default path.
// NOTE: CompositeFramebufferBackedViewports()'s actual texture blit is still a stub (its
// GetColorTexture() bind call is commented out, and Framebuffer has no such accessor yet), so
// the perspective viewport's offscreen render currently has no way back onto the screen.
struct FViewport
{
    Camera ViewportCamera;
    std::unique_ptr<Rendering::Framebuffer> ViewportFramebuffer;
    int QuadrantX = 0, QuadrantY = 0, QuadrantWidth = 0, QuadrantHeight = 0; // pixel-space, GL bottom-left origin
};

//Rendering::FramebufferAttachmentSpec
const Rendering::FFramebufferSpec PerspectiveFramebufferSpec =
{
    DefaultWidth,
    DefaultHeight,
    {Rendering::DefaultRenderBufferFramebufferAttachment_MultisampleColor,
                Rendering::DefaultRenderBufferFramebufferAttachment_MultisampleDepth}
};

constexpr int Viewport_Perspective = 0;
constexpr int Viewport_Top = 1;
constexpr int Viewport_Front = 2;
constexpr int Viewport_Right = 3;
constexpr int ViewportCount = 4;

std::array<FViewport, ViewportCount> Viewports;

//alias so all of the existing free-fly/input code (UpdateCameraMovement, WASDQE, RMB mouse-look,
//Gizmo target, etc.) keeps working completely unchanged - it only ever needs the one perspective
//camera, never the array
Camera& MainCamera = Viewports[Viewport_Perspective].ViewportCamera;

//gizmo, and the transform it's currently visualizing
TransformGizmo Gizmo;
Transform GizmoTargetTransform;

//screen-space texts
SSTextRenderer TextRenderer;

//when true, all 4 Viewports render simultaneously into a 2x2 grid instead of just the
//perspective camera rendering fullscreen. Toggled by Space (KeyboardEventCallback).
static bool bMultiViewMode = false;

//camera fly controls (active only while the right mouse button is held, mirroring most editors)
static bool bRightMouseHeld = false;
static bool bFirstCursorSample = true;
static double LastCursorX = 0.0;
static double LastCursorY = 0.0;
static float CameraPitchDeg = 0.0f;

//timing
static double LastFrameTime = 0;
static double ThisFrameTime = 0;
static double LastTimingUpdateTime = 0;
static double DeltaTime = 0.0;
static unsigned int FrameCount = 0;
static unsigned int LastTimingUpdateFrame = 0;

static bool bUsingWSL = false;

//exit flag
static bool bRequestedExit = false;

//initialization flags
static bool bGLFWInitialized = false;

//GL version actually negotiated with the platform in CreateBestWindow (0 until a window exists)
static int NegotiatedGLVersionMajor = 0;
static int NegotiatedGLVersionMinor = 0;

//vertices of a colored tetrahedron centered at the origin, one triangle (3 verts) per face rather
//than 4 shared corner verts, so each face can carry its own copy of the 3 corner colors below -
//non-indexed glDrawArrays draw, same raw-GL style as the original triangle this replaces (deliberately
//not migrated to the VertexBuffer/IndexBuffer/VertexArray classes - see the hex torus/textured cube
//below for that style). Apex-up construction: V0 sits directly above the centroid on +Y, the other
//3 vertices form an equilateral triangle in a horizontal plane below it, spaced 120 degrees apart
//around the Y axis, with V1 positioned in the +Z direction from center. For a regular tetrahedron
//with circumradius R (center-to-vertex distance) = 3.0: apex at (0,R,0); base plane at y=-R/3 (so
//the 4 vertices' centroid lands exactly on the origin); base horizontal radius = R*2*sqrt(2)/3 (the
//value that makes apex-to-base and base-to-base edge lengths equal, i.e. makes it regular).
//Face winding is CCW as seen from outside each face (verified by hand against the tetrahedron's
//centroid at the origin), matching the codebase's winding convention elsewhere even though face
//culling isn't currently enabled.
static float TetrahedronVerts[] =
{
    //face opposite V0=(0, 3, 0): V1,V3,V2
     0.000000f, -1.0f,  2.828427f,
    -2.449490f, -1.0f, -1.414214f,
     2.449490f, -1.0f, -1.414214f,
    //face opposite V1=(0, -1, 2.828427): V0,V2,V3
     0.0f,  3.0f,  0.0f,
     2.449490f, -1.0f, -1.414214f,
    -2.449490f, -1.0f, -1.414214f,
    //face opposite V2=(2.449490, -1, -1.414214): V0,V3,V1
     0.0f,  3.0f,  0.0f,
    -2.449490f, -1.0f, -1.414214f,
     0.000000f, -1.0f,  2.828427f,
    //face opposite V3=(-2.449490, -1, -1.414214): V0,V1,V2
     0.0f,  3.0f,  0.0f,
     0.000000f, -1.0f,  2.828427f,
     2.449490f, -1.0f, -1.414214f,
};

//per-corner colors (V0=red, V1=green, V2=blue, V3=yellow), repeated per-face in the same order as
//TetrahedronVerts above so each corner keeps the same color everywhere it appears - Gouraud-blends
//within each face, same visual language as the original triangle's red/green/blue gradient.
static float TetrahedronColors[] =
{
    0.0f, 1.0f, 0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f, //V1,V3,V2
    1.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f, 0.0f, //V0,V2,V3
    1.0f, 0.0f, 0.0f,  1.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f, //V0,V3,V1
    1.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f,  0.0f, 0.0f, 1.0f, //V0,V1,V2
};

//vertex buffer object
GLuint VertexBufferObject_Positions;
GLuint VertexBufferObject_Colors;

//vertex array object
GLuint TetrahedronVAO;

//hexagonal torus ("hex nut") drawn around the tetrahedron above - a hexagonal ring extruded along Z
//into a solid loop (front/back faces plus inner/outer walls), built with the new VertexBuffer/
//IndexBuffer/VertexArray classes instead of raw GL calls. unique_ptr because VertexArray's
//constructor calls glGenVertexArrays(), which needs a live GL context - this can't be constructed at
//global-init time, only once InitGraphics() has created the window/context (mirrors how Gizmo/
//TextRenderer are default-constructed globally but only actually touch GL from an Initialize()
//called after that point).
std::unique_ptr<Rendering::VertexArray> HexTorus;

//textured cube demonstrating Texture2D (same texture on all 6 faces), off to the side of the
//tetrahedron/hex-torus so they don't overlap. Same unique_ptr-for-deferred-construction
//reasoning as HexTorus above applies to both members here.
std::unique_ptr<Rendering::VertexArray> TexturedCube;
std::unique_ptr<Rendering::Texture2D> CheckerTexture;

//minimal NDC-space textured-quad blit, used by CompositeFramebufferBackedViewports() to draw a
//viewport's offscreen Framebuffer color texture into its on-screen quadrant. Dormant/unused by
//default since no viewport has a Framebuffer assigned out of the box - see FViewport's comment.
std::shared_ptr<Rendering::ShaderProgram> BlitShaderProgram;
std::unique_ptr<Rendering::VertexArray> BlitQuad;

//shared per-frame data (time/resolution/cursor/view-projection) any shader can opt into via the
//FrameConstants uniform block - see resource/textured.vs/.fs for the consuming side
std::unique_ptr<Rendering::UniformBuffer> FrameConstantsUBO;
Rendering::FFrameConstants FrameConstants;

//shader objects
Rendering::ShaderManager* shaderManager;
std::shared_ptr<Rendering::ShaderProgram> PassthroughShaderProgram;
std::shared_ptr<Rendering::ShaderProgram> TexturedShaderProgram;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv, char** envp)
{
    //initialize the sim
    if(Init(argc, argv, envp))
    {
        //run the sim
        Run();
    }

    //cleanup the sim
    Cleanup();

    return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// initialization functions
bool Init(int argc, char** argv, char** envp)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);   // line-buffered regardless of TTY detection
    // or _IONBF for fully unbuffered, like stderr
    LogInfo("initializing...\n");

    const char* WSLIndicatorStrings[3] =
    {
    "WSL_DISTRO_NAME",
    "WSL2_GUI_APPS_ENABLED",
    "WSL_INTEROP"
    };

    for (int i = 0; i < 3; i++)
    {
        if (getenv(WSLIndicatorStrings[i]) != nullptr)
        {
            LogWarning("*** Detected WSL environment ***\n");
            LogWarning("* cursor hiding is not supported and will be disabled\n");
            bUsingWSL = true;
            break;
        }
    }

    ConfigManager::Get().LoadFromFile(myc::GetExecutableDir() + "/config/default.conf");

    if(!InitGraphics())
    {
        return false;
    }

    if(!InitInput())
    {
        return false;
    }

    LogInfo("initialization successful.\n");

    return true;
}

bool InitGraphics()
{
    //attempt initializing GLFW
    if(!(bGLFWInitialized = glfwInit()))
    {
        LogError("GLFW initialization failure.\n");
        return false;
    }

    //set error callback
    glfwSetErrorCallback(ErrorCallback);

    CreateBestWindow();

    //enable vertical sync
    glfwSwapInterval(1);

    // enable depth testing, making so occluded pixels won't be rendered
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

    //create shader objects
    shaderManager = Rendering::ShaderManager::Get();
    PassthroughShaderProgram = shaderManager->LoadShaderProgram("passthrough", "/resource/passthrough.vs", "/resource/passthrough.fs");
    TexturedShaderProgram = shaderManager->LoadShaderProgram("textured", "/resource/textured.vs", "/resource/textured.fs");

    //create the transform gizmo's shader and generated axis/ring/box meshes
    Gizmo.Initialize();

    FrameConstantsUBO = std::make_unique<Rendering::UniformBuffer>(sizeof(Rendering::FFrameConstants), GL_DYNAMIC_DRAW);
    FrameConstantsUBO->BindToPoint(Rendering::FrameConstantsBindingPoint);
    Rendering::BindFrameConstantsBlock(TexturedShaderProgram->GetProgramID());
    Rendering::BindFrameConstantsBlock(PassthroughShaderProgram->GetProgramID());
    Rendering::BindFrameConstantsBlock(Gizmo.GetShaderID());

    //bake the screen-space text renderer's font atlas
    if(!TextRenderer.Initialize("/resource/font/Roboto-Medium.ttf", 24.0f))
    {
        LogError("Failed to initialize SSTextRenderer\n");
    }
    TextRenderer.SetScreenSize(Width, Height);

    ///////////////////////
    /// initialize rendering objects

    //create vertex buffer for storing per-vertex data
    glGenBuffers(1, &VertexBufferObject_Positions);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Positions);
    glBufferData(GL_ARRAY_BUFFER, 36 * sizeof(float), TetrahedronVerts, GL_STATIC_DRAW);

    glGenBuffers(1, &VertexBufferObject_Colors);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Colors);
    glBufferData(GL_ARRAY_BUFFER, 36 * sizeof(float), TetrahedronColors, GL_STATIC_DRAW);

    //create vertax array object for storing info about bound objects and what to render
    glGenVertexArrays(1, &TetrahedronVAO);
    glBindVertexArray(TetrahedronVAO);

    //specify vertex attribute 0 and specify format
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Positions);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    //specify color layout
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Colors);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    //hexagonal torus surrounding the tetrahedron above, built with VertexBuffer/IndexBuffer/VertexArray
    //instead of raw GL calls - proves out the new abstraction alongside the old hand-rolled style right next to it.
    //Extruded along Z into a solid hex-nut-shaped torus (front/back faces + inner/outer walls) rather
    //than the flat hexagonal washer this used to be - the passthrough shader does no lighting/normal
    //shading at all (flat vertex-color Gouraud interpolation only), so no per-face vertex duplication
    //is needed purely for shading correctness; only geometric position differs between faces/walls.
    {
        constexpr float InnerRadius = 4.0f;
        constexpr float OuterRadius = 5.0f;
        constexpr int SideCount = 6;
        constexpr float HalfDepth = 0.5f; //full depth 1.0, matching the ring's radial width (Outer-Inner)

        //4 verts per side (outer/inner rim, front/back face), indexed - not a single continuous
        //triangle strip like the old flat version, since front/back/inner-wall/outer-wall can't be
        //expressed as one strip without degenerate triangles; GL_TRIANGLES is simpler and clearer here
        std::vector<float> HexVerts; //interleaved {x,y,z, r,g,b}
        std::vector<GLuint> HexIndices;

        for(int Side = 0; Side < SideCount; Side++)
        {
            const float Angle = glm::radians(360.0f * (float)Side / (float)SideCount);
            const float CosA = cosf(Angle);
            const float SinA = sinf(Angle);

            //vertex order per side: 0=OuterFront, 1=InnerFront, 2=OuterBack, 3=InnerBack
            HexVerts.insert(HexVerts.end(), {OuterRadius * CosA, OuterRadius * SinA,  HalfDepth, 1.0f, 0.6f, 0.0f});
            HexVerts.insert(HexVerts.end(), {InnerRadius * CosA, InnerRadius * SinA,  HalfDepth, 1.0f, 0.6f, 0.0f});
            HexVerts.insert(HexVerts.end(), {OuterRadius * CosA, OuterRadius * SinA, -HalfDepth, 1.0f, 0.6f, 0.0f});
            HexVerts.insert(HexVerts.end(), {InnerRadius * CosA, InnerRadius * SinA, -HalfDepth, 1.0f, 0.6f, 0.0f});
        }

        //winding verified by hand (CCW as seen from each face's outward direction) for all 4 parts
        for(int Side = 0; Side < SideCount; Side++)
        {
            const int Next = (Side + 1) % SideCount;
            const GLuint OuterFront = (GLuint)(Side * 4 + 0), InnerFront = (GLuint)(Side * 4 + 1);
            const GLuint OuterBack  = (GLuint)(Side * 4 + 2), InnerBack  = (GLuint)(Side * 4 + 3);
            const GLuint NextOuterFront = (GLuint)(Next * 4 + 0), NextInnerFront = (GLuint)(Next * 4 + 1);
            const GLuint NextOuterBack  = (GLuint)(Next * 4 + 2), NextInnerBack  = (GLuint)(Next * 4 + 3);

            //front face (+Z outward): OuterFront, NextOuterFront, NextInnerFront, InnerFront
            HexIndices.insert(HexIndices.end(), {OuterFront, NextOuterFront, NextInnerFront, NextInnerFront, InnerFront, OuterFront});
            //back face (-Z outward, reversed relative to front): InnerBack, NextInnerBack, NextOuterBack, ...
            HexIndices.insert(HexIndices.end(), {InnerBack, NextInnerBack, NextOuterBack, NextOuterBack, OuterBack, InnerBack});
            //outer wall (radially outward): OuterFront, OuterBack, NextOuterBack, ...
            HexIndices.insert(HexIndices.end(), {OuterFront, OuterBack, NextOuterBack, NextOuterBack, NextOuterFront, OuterFront});
            //inner wall (radially inward, into the hole): InnerFront, NextInnerFront, NextInnerBack, ...
            HexIndices.insert(HexIndices.end(), {InnerFront, NextInnerFront, NextInnerBack, NextInnerBack, InnerBack, InnerFront});
        }

        HexTorus = std::make_unique<Rendering::VertexArray>();
        HexTorus->AddVertexBuffer(
            Rendering::VertexBuffer(HexVerts.data(), HexVerts.size() * sizeof(float), GL_STATIC_DRAW),
            {
                Rendering::FVertexAttribute{0, 3, GL_FLOAT, false},
                Rendering::FVertexAttribute{1, 3, GL_FLOAT, false}
            },
            6 * sizeof(float));
        HexTorus->SetIndexBuffer(Rendering::IndexBuffer(HexIndices.data(), (unsigned int)HexIndices.size(), GL_STATIC_DRAW));
    }

    //textured cube off to the side of the tetrahedron/hex-torus, proving out Texture2D - same
    //checkerboard texture applied to all 6 faces. The checkerboard is generated as 3-channel RGB
    //(not RGBA) specifically so this exercises Texture2D's non-4-byte-aligned upload path
    //(GL_UNPACK_ALIGNMENT=1), not just the trivial RGBA case.
    {
        constexpr int CheckerSize = 64;
        constexpr int SquarePixels = 8;
        std::vector<unsigned char> CheckerPixels(CheckerSize * CheckerSize * 3);
        for(int y = 0; y < CheckerSize; y++)
        {
            for(int x = 0; x < CheckerSize; x++)
            {
                const bool bLight = ((x / SquarePixels) + (y / SquarePixels)) % 2 == 0;
                const unsigned char Value = bLight ? 220 : 40;
                const int PixelIndex = (y * CheckerSize + x) * 3;
                CheckerPixels[PixelIndex + 0] = Value;
                CheckerPixels[PixelIndex + 1] = Value;
                CheckerPixels[PixelIndex + 2] = Value;
            }
        }
        CheckerTexture = std::make_unique<Rendering::Texture2D>(CheckerPixels.data(), CheckerSize, CheckerSize, 3, GL_REPEAT);

        //Offset/size are deliberately tight: at z=0 this camera (50deg vertical FOV, 1920x1080,
        //8 units back) only has ~6.6 units of horizontal half-width in view, and the hex torus's
        //outer radius already reaches 5 - this box keeps the cube clear of the torus on one side
        //and inside the frustum on the other, with a small margin either way.
        constexpr float CubeHalfSize = 0.6f;
        constexpr float CubeOffsetX = 5.7f;

        //6 faces x 4 corners, each face's own 4 verts (not shared cube corners) so each face gets
        //its own full 0..1 UV range - sharing corners across faces would need ambiguous per-vertex
        //UVs, since a cube corner is part of 3 differently-UV'd faces. Corner order per face is CCW
        //as seen from outside (verified by hand via cross product against each face's own normal).
        struct FCubeFace { glm::vec3 Corners[4]; };
        const FCubeFace CubeFaces[6] =
        {
            {{ {-1,-1, 1}, { 1,-1, 1}, { 1, 1, 1}, {-1, 1, 1} }}, //+Z front
            {{ { 1,-1,-1}, {-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1} }}, //-Z back
            {{ { 1,-1, 1}, { 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1} }}, //+X right
            {{ {-1,-1,-1}, {-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1} }}, //-X left
            {{ {-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, {-1, 1,-1} }}, //+Y top
            {{ {-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1} }}, //-Y bottom
        };
        const glm::vec2 FaceUVs[4] = { {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f} };

        //interleaved {x,y,z, u,v} per vertex
        std::vector<float> CubeVerts;
        std::vector<GLuint> CubeIndices;
        for(int Face = 0; Face < 6; Face++)
        {
            for(int Corner = 0; Corner < 4; Corner++)
            {
                const glm::vec3& C = CubeFaces[Face].Corners[Corner];
                CubeVerts.insert(CubeVerts.end(),
                {
                    CubeOffsetX + C.x * CubeHalfSize, C.y * CubeHalfSize, C.z * CubeHalfSize,
                    FaceUVs[Corner].x, FaceUVs[Corner].y
                });
            }
            const GLuint Base = (GLuint)(Face * 4);
            CubeIndices.insert(CubeIndices.end(), {Base, Base + 1, Base + 2, Base + 2, Base + 3, Base});
        }

        TexturedCube = std::make_unique<Rendering::VertexArray>();
        TexturedCube->AddVertexBuffer(
            Rendering::VertexBuffer(CubeVerts.data(), CubeVerts.size() * sizeof(float), GL_STATIC_DRAW),
            {
                Rendering::FVertexAttribute{0, 3, GL_FLOAT, false},
                Rendering::FVertexAttribute{1, 2, GL_FLOAT, false}
            },
            5 * sizeof(float));
        TexturedCube->SetIndexBuffer(Rendering::IndexBuffer(CubeIndices.data(), (unsigned int)CubeIndices.size(), GL_STATIC_DRAW));
    }

    //setup the camera: perspective projection matching the window, positioned back from the origin and looking at it
    MainCamera = Camera((double)Width, (double)Height, 0.1, 1000.0, ECameraProjectionMode::Perspective, 50.0);
    MainCamera.SetLocation(glm::vec3(0.0f, 0.0f, 8.0f));

    //opts the perspective viewport into the offscreen-Framebuffer path (see FViewport's comment) -
    //Render() resizes this to the viewport's actual rect every frame, PerspectiveFramebufferSpec's
    //Width/Height here are just the initial allocation size
    Viewports[Viewport_Perspective].ViewportFramebuffer = std::make_unique<Rendering::Framebuffer>(PerspectiveFramebufferSpec);

    //three stationary orthographic cameras, one looking down each major world axis at the scene
    //origin - see Camera.cpp's ortho projection fix and the rotation math this relies on (Transform::
    //WorldForward = +Z, identity rotation looks down -Z, confirmed against MainCamera's own setup above)
    constexpr double OrthoDistance = 8.0;

    Viewports[Viewport_Top].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Top].ViewportCamera.SetLocation(glm::vec3(0.0f, (float)OrthoDistance, 0.0f));
    Viewports[Viewport_Top].ViewportCamera.SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f)); //pitch down to look straight down -Y

    Viewports[Viewport_Front].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Front].ViewportCamera.SetLocation(glm::vec3(0.0f, 0.0f, (float)OrthoDistance));
    //no rotation needed - identity already looks down -Z toward the origin, same as MainCamera's default

    Viewports[Viewport_Right].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Right].ViewportCamera.SetLocation(glm::vec3((float)OrthoDistance, 0.0f, 0.0f));
    Viewports[Viewport_Right].ViewportCamera.SetRotation(glm::vec3(0.0f, 90.0f, 0.0f)); //yaw to look down -X

    RecomputeViewportQuadrants(Width, Height);

    //dormant compositing shader/quad for the opt-in per-viewport Framebuffer path (see FViewport's
    //comment and CompositeFramebufferBackedViewports()) - built here regardless since the render
    //loop unconditionally checks for and uses them whenever a viewport does have a Framebuffer
    BlitShaderProgram = shaderManager->LoadShaderProgram("blit", "/resource/blit.vs", "/resource/blit.fs");
    const float BlitQuadVerts[] =
    {
        //x,     y,     u,    v (already NDC space, no transform needed - see resource/blit.vs)
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };
    const GLuint BlitQuadIndices[] = {0, 1, 2, 2, 3, 0};
    BlitQuad = std::make_unique<Rendering::VertexArray>();
    BlitQuad->AddVertexBuffer(
        Rendering::VertexBuffer(BlitQuadVerts, sizeof(BlitQuadVerts), GL_STATIC_DRAW),
        {
            Rendering::FVertexAttribute{0, 2, GL_FLOAT, false},
            Rendering::FVertexAttribute{1, 2, GL_FLOAT, false}
        },
        4 * sizeof(float));
    BlitQuad->SetIndexBuffer(Rendering::IndexBuffer(BlitQuadIndices, 6, GL_STATIC_DRAW));

    return true;
}

bool CreateBestWindow()
{
    struct GLVersion { int Major = 0; int Minor = 0;};

    //desired is attempted first; required is the minimum acceptable fallback. both come from config/default.conf
    ConfigManager& Config = ConfigManager::Get();
    const GLVersion DesiredVersion
    {
        Config.GetInt("gl_version_desired_major", 4),
        Config.GetInt("gl_version_desired_minor", 6)
    };
    const GLVersion RequiredVersion
    {
        Config.GetInt("gl_version_required_major", 3),
        Config.GetInt("gl_version_required_minor", 3)
    };

    const bool bDesiredDiffersFromRequired = DesiredVersion.Major != RequiredVersion.Major || DesiredVersion.Minor != RequiredVersion.Minor;
    const GLVersion VersionsToTry[2] = { DesiredVersion, RequiredVersion };
    const int NumVersionsToTry = bDesiredDiffersFromRequired ? 2 : 1;

    int CurrentVersionIndex = 0;
    for(; CurrentVersionIndex < NumVersionsToTry; CurrentVersionIndex++)
    {
        const int Major = VersionsToTry[CurrentVersionIndex].Major;
        const int Minor = VersionsToTry[CurrentVersionIndex].Minor;

        //try to set context version
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, Major);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, Minor);

        //enable anti-aliasing?
        //glfwWindowHint(GLFW_SAMPLES, 4);

        const char* Title = "GLBP";

        //attempt to create the window
        MainWindow = glfwCreateWindow( Width, Height, Title, NULL, NULL);
        if(MainWindow == nullptr)
        {
            LogInfo("GL version %d.%d Window creation failed.\n", Major, Minor);
        }
        else
        {
            LogInfo("GL version %d.%d Window creation success.\n", Major, Minor);
            NegotiatedGLVersionMajor = Major;
            NegotiatedGLVersionMinor = Minor;
            break;
        }
    }

    if(CurrentVersionIndex == NumVersionsToTry)
    {
        LogError("Failed to create window with desired GL %d.%d or required GL %d.%d, exiting...\n",
                 DesiredVersion.Major, DesiredVersion.Minor, RequiredVersion.Major, RequiredVersion.Minor);
        exit(EXIT_FAILURE);
    }

    //make the newly created opengl context current
    glfwMakeContextCurrent(MainWindow);

    //load gl extensions
    if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        LogError("Couldn't load openGL extensions\n");
        return false;
    }
    else
    {
        LogInfo("GLAD loaded GL extensions\n");
    }

    // get version info
    LogInfo("Renderer: %s\n", glGetString(GL_RENDERER));
    LogInfo("   OpenGL %s\n", glGetString(GL_VERSION));

    return true;
}

bool InitInput()
{
    //set keyboard callback
    glfwSetKeyCallback(MainWindow, KeyboardEventCallback);

    //set mouse callbacks, used to fly the camera while the right mouse button is held
    glfwSetMouseButtonCallback(MainWindow, MouseButtonEventCallback);
    glfwSetCursorPosCallback(MainWindow, CursorPositionEventCallback);

    //set resize callback
    glfwSetFramebufferSizeCallback(MainWindow, WindowResizeEventCallback);

    //set up Dear ImGui. must come after the callbacks above so its GLFW backend
    //(install_callbacks=true) can chain to them rather than silently replacing them
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //deliberately not enabling ImGuiConfigFlags_ViewportsEnable - platform multi-viewport
    //spawns real OS windows, which hits the same broken cursor-warp behavior WSLg already
    //has trouble with for GLFW_CURSOR_DISABLED
    ImGui_ImplGlfw_InitForOpenGL(MainWindow, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// main loop

void Run()
{
    LogInfo("run started at time %lfs, running...\n", glfwGetTime());

    while(!bRequestedExit)
    {
        UpdateTiming(MainWindow);
        Tick(DeltaTime);
        Render(DeltaTime);
        ProcessInput();
    }

   LogInfo("running complete.\n");
}

void Tick(double dt)
{
    UpdateCameraMovement(MainWindow, dt);

    //the passthrough-uniform/FrameConstants-UBO update that used to happen here now happens once
    //per viewport in Render() instead, immediately before that viewport's draws - each viewport
    //has its own camera/ViewProjectionMatrix, so a single once-per-frame update here is no longer
    //enough now that there can be more than one active viewport

    double CursorX = 0.0, CursorY = 0.0;
    glfwGetCursorPos(MainWindow, &CursorX, &CursorY);

    FrameConstants.CursorPosition = glm::vec2((float)CursorX, (float)CursorY);
    FrameConstants.Time = (float)ThisFrameTime;
}

void Render(double dt)
{
    //start a new Dear ImGui frame - nothing is actually drawn until ImGui::Render()/RenderDrawData() below
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //smoke test for the ImGui integration - gets replaced by real tool panels (shader playground, etc.) later
    ImGui::ShowDemoWindow();

    const double Red = cos(ThisFrameTime);
    const double Green = cos(ThisFrameTime);
    const double Blue = cos(ThisFrameTime);

    //in single-view mode only the perspective camera (index 0) renders, fullscreen; in multi-view
    //mode all 4 render, each confined to its own screen quadrant
    const int ActiveViewportCount = bMultiViewMode ? ViewportCount : 1;

    glEnable(GL_DEPTH_TEST);

    for(int i = 0; i < ActiveViewportCount; i++)
    {
        FViewport& VP = Viewports[i];

        //this frame's actual target rect: full window in single-view mode (only viewport 0 is
        //ever active then), or this viewport's quadrant in multi-view mode
        const int RectX = bMultiViewMode ? VP.QuadrantX : 0;
        const int RectY = bMultiViewMode ? VP.QuadrantY : 0;
        const int RectW = bMultiViewMode ? VP.QuadrantWidth : Width;
        const int RectH = bMultiViewMode ? VP.QuadrantHeight : Height;

        if(VP.ViewportFramebuffer)
        {
            //opt-in offscreen path - Resize() is a cheap no-op when already this size, so this
            //naturally keeps the FBO sized correctly whether this viewport is currently shown in
            //a quadrant or (if it's the perspective viewport) fullscreen in single-view mode
            VP.ViewportFramebuffer->Resize(RectW, RectH);
            VP.ViewportFramebuffer->Bind(); //sets glViewport to (0,0,RectW,RectH) internally
        }
        else
        {
            //default path: render straight into the default framebuffer, confined to this
            //viewport's rect. glViewport alone only changes the NDC-to-screen mapping - it does
            //NOT confine glClear/draws to that region, so glScissor is required too, otherwise
            //each viewport's clear would wipe the other three.
            glViewport(RectX, RectY, RectW, RectH);
            glEnable(GL_SCISSOR_TEST);
            glScissor(RectX, RectY, RectW, RectH);
        }

        glClearColor(Red, Green, Blue, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //per-viewport uniform/UBO update, moved out of Tick() since each viewport needs its own
        const glm::mat4 ViewProjectionMatrix = VP.ViewportCamera.GetViewProjectionMatrix();
        glUseProgram(PassthroughShaderProgram->GetProgramID());
        glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->GetProgramID(), "ViewProjectionMatrix"), 1, GL_FALSE, &ViewProjectionMatrix[0][0]);


        FrameConstants.ViewProjectionMatrix = ViewProjectionMatrix;
        //Resolution must match the surface actually being rendered to, not always the window -
        //textured.fs uses it against gl_FragCoord (relative to the currently active viewport/FBO)
        //for its cursor-glow effect; the wrong size breaks that math.
        FrameConstants.Resolution = glm::vec2((float)RectW, (float)RectH);
        FrameConstantsUBO->SetData(&FrameConstants, sizeof(FrameConstants));


        //render here
        glUseProgram(PassthroughShaderProgram->GetProgramID());
        glBindVertexArray(TetrahedronVAO);
        glDrawArrays(GL_TRIANGLES, 0, 12);

        //draw the hex torus around the tetrahedron - same shader/uniforms, geometry built via the
        //new VertexBuffer/IndexBuffer/VertexArray classes instead of raw GL calls like the tetrahedron above
        if(HexTorus)
        {
            HexTorus->Draw(GL_TRIANGLES);
        }

        //draw the checkerboard-textured cube next to the tetrahedron/hex-torus, proving out Texture2D
        if(TexturedCube && CheckerTexture)
        {
            glUseProgram(TexturedShaderProgram->GetProgramID());
            glUniform1i(glGetUniformLocation(TexturedShaderProgram->GetProgramID(), "TexSampler"), 0);
            CheckerTexture->Bind(0);
            TexturedCube->Draw(GL_TRIANGLES);
        }

        //gizmo stays perspective-only - drawn here (not after the loop) so it's naturally
        //confined to the perspective viewport's own rect/scissor in multi-view mode too
        Gizmo.Draw(GizmoTargetTransform, VP.ViewportCamera.GetLocation());

        if(VP.ViewportFramebuffer)
        {
            VP.ViewportFramebuffer->Unbind();
            //no-op unless this framebuffer's color attachment is multisampled - resolves it into
            //a sampleable single-sample texture before CompositeFramebufferBackedViewports() reads it
            VP.ViewportFramebuffer->ResolveMultisampledColor();
        }
        else
        {
            glDisable(GL_SCISSOR_TEST);
        }
    }

    glViewport(0, 0, Width, Height); //restore full-window viewport before compositing/HUD/ImGui
    CompositeFramebufferBackedViewports(); //perspective viewport has a Framebuffer now, but the blit is still a stub - see FViewport's comment

    //draw a small screen-space HUD showing the active gizmo mode and hotkeys
    const char* ModeName = "Translate (W)";
    if(Gizmo.GetMode() == EGizmoMode::Rotate)
    {
        ModeName = "Rotate (E)";
    }
    else if(Gizmo.GetMode() == EGizmoMode::Scale)
    {
        ModeName = "Scale (R)";
    }
    TextRenderer.DrawText(std::string("Gizmo mode: ") + ModeName, 12.0f, 28.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    TextRenderer.DrawText("RMB + WASDQE to fly, F5 to reload shaders, Space to toggle multi-view", 12.0f, 52.0f, glm::vec3(0.7f, 0.7f, 0.7f));
    if(bUsingWSL)
    {
        TextRenderer.DrawText("Warning: Cursor hiding is not supported on WSL, please consider running natively on windows or linux", 12.0f, 76.0f, glm::vec3(1.0f, 1.0f, 0.2f));
    }

    //finalize and draw the ImGui frame on top of the scene
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    //swap front and back buffers
    glfwSwapBuffers(MainWindow);
}

void ProcessInput()
{
    //poll queued events
    glfwPollEvents();

    //request exit if window x has been clicked
    if(glfwWindowShouldClose(MainWindow))
    {
        bRequestedExit = true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// cleanup functions
void Cleanup()
{
    LogInfo("cleaning up...\n");

    //shut down Dear ImGui before the GL context/window it depends on goes away
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    //destroy window if one exists
    if(MainWindow != nullptr)
    {
       glfwDestroyWindow(MainWindow);
    }

    //terminate GLFW
    if(bGLFWInitialized)
    {
       glfwTerminate();
    }

    LogInfo("cleanup complete.\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// event callbacks
void ErrorCallback(int error, const char *description)
{
    ///todo: switch to use bespoke logging once available
    LogError("glfwError %X: %s\n", error, description);
}

void KeyboardEventCallback(GLFWwindow *Window, int KeyCode, int ScanCode, int Action, int Modifiers)
{
    //don't let gizmo/camera hotkeys fire while an ImGui widget (e.g. a text field) wants the keyboard
    if(ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }

    if(Action != GLFW_PRESS)
    {
        return;
    }

    if(KeyCode == GLFW_KEY_ESCAPE)
    {
        bRequestedExit = true;
        return;
    }

    if(KeyCode == GLFW_KEY_F5)
    {
        PassthroughShaderProgram->ReloadShaderObjects();
        TexturedShaderProgram->ReloadShaderObjects();

        //relinking resets a program's uniform block bindings, so this needs reassigning after every reload
        Rendering::BindFrameConstantsBlock(TexturedShaderProgram->GetProgramID());
        return;
    }

    if(KeyCode == GLFW_KEY_SPACE)
    {
        bMultiViewMode = !bMultiViewMode;
        return;
    }

    //gizmo mode hotkeys double as WASDQE camera-fly keys while the right mouse button is held,
    //so only let them switch the gizmo mode when the camera isn't currently being flown
    if(bRightMouseHeld)
    {
        return;
    }

    if(KeyCode == GLFW_KEY_W)
    {
        Gizmo.SetMode(EGizmoMode::Translate);
    }
    else if(KeyCode == GLFW_KEY_E)
    {
        Gizmo.SetMode(EGizmoMode::Rotate);
    }
    else if(KeyCode == GLFW_KEY_R)
    {
        Gizmo.SetMode(EGizmoMode::Scale);
    }
}

void MouseButtonEventCallback(GLFWwindow *Window, int Button, int Action, int Modifiers)
{
    //don't start camera-fly from a click ImGui already claimed (e.g. on a panel/widget)
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    if(Button != GLFW_MOUSE_BUTTON_RIGHT)
    {
        return;
    }

    bRightMouseHeld = (Action == GLFW_PRESS);

    if(bRightMouseHeld)
    {
        if(!bUsingWSL)
        {
            //hide and lock the cursor for FPS-style mouse-look while flying the camera
            glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        bFirstCursorSample = true;
    }
    else
    {
        if(!bUsingWSL)
        {
            glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void CursorPositionEventCallback(GLFWwindow *Window, double XPos, double YPos)
{
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    if(!bRightMouseHeld)
    {
        return;
    }

    //the first sample after grabbing the cursor has no previous position to diff against
    if(bFirstCursorSample)
    {
        LastCursorX = XPos;
        LastCursorY = YPos;
        bFirstCursorSample = false;
        return;
    }

    const double DeltaX = XPos - LastCursorX;
    const double DeltaY = YPos - LastCursorY;
    LastCursorX = XPos;
    LastCursorY = YPos;

    constexpr float MouseSensitivity = 0.15f;
    constexpr float MaxPitchDeg = 89.0f;

    //yaw rotates around the world up axis, independent of the camera's current tilt
    //MainCamera.RotateWorld(Transform::WorldUp, (float)-DeltaX * MouseSensitivity);
    MainCamera.RotateLocal(Transform::WorldUp, (float)-DeltaX * MouseSensitivity);

    //pitch rotates around the camera's own local right axis, clamped so it can't flip over
    float PitchDelta = (float)-DeltaY * MouseSensitivity;
    PitchDelta = glm::clamp(CameraPitchDeg + PitchDelta, -MaxPitchDeg, MaxPitchDeg) - CameraPitchDeg;
    CameraPitchDeg += PitchDelta;
    MainCamera.RotateLocal(MainCamera.GetRightVector(), PitchDelta);
}

void UpdateCameraMovement(GLFWwindow* Window, double DeltaTime)
{
    if(!bRightMouseHeld)
    {
        return;
    }

    constexpr float MoveSpeed = 6.0f;
    glm::vec3 MoveDirection(0.0f);

    //forward/right movement is local to the camera's current orientation
    if(glfwGetKey(Window, GLFW_KEY_W) == GLFW_PRESS) MoveDirection -= MainCamera.GetForwardVector();
    if(glfwGetKey(Window, GLFW_KEY_S) == GLFW_PRESS) MoveDirection += MainCamera.GetForwardVector();
    if(glfwGetKey(Window, GLFW_KEY_D) == GLFW_PRESS) MoveDirection += MainCamera.GetRightVector();
    if(glfwGetKey(Window, GLFW_KEY_A) == GLFW_PRESS) MoveDirection -= MainCamera.GetRightVector();

    //up/down movement stays in world space regardless of camera pitch
    if(glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS) MoveDirection += Transform::WorldUp;
    if(glfwGetKey(Window, GLFW_KEY_Q) == GLFW_PRESS) MoveDirection -= Transform::WorldUp;

    if(glm::length(MoveDirection) > 0.0001f)
    {
        MainCamera.AddTranslation(glm::normalize(MoveDirection) * MoveSpeed * (float)DeltaTime);
    }
}

void WindowResizeEventCallback(GLFWwindow *Window, int NewWidth, int NewHeight)
{
    if(NewWidth <= 0)
    {
        NewWidth = 1;
    }
    if(NewHeight <= 0)
    {
        NewHeight = 1;
    }

    glViewport(0, 0, NewWidth, NewHeight);

    Width = NewWidth;
    Height = NewHeight;
    TextRenderer.SetScreenSize(NewWidth, NewHeight);
    RecomputeViewportQuadrants(NewWidth, NewHeight);

    LogInfo("Window resized to %dx%d\n", NewWidth, NewHeight);
}

// Pure layout math - computes each viewport's screen-quadrant rect and updates each camera's clip
// dimensions for the new aspect ratio. Does NOT touch any Framebuffer - FBO sizing for the opt-in
// path is handled per-frame in Render() instead, since whether a given viewport's FBO should be
// quadrant-sized or full-window-sized depends on the current single-view/multi-view mode, not
// just on window size.
void RecomputeViewportQuadrants(int WindowWidth, int WindowHeight)
{
    const int HalfWidth = WindowWidth / 2, HalfHeight = WindowHeight / 2;
    const int RemWidth = WindowWidth - HalfWidth, RemHeight = WindowHeight - HalfHeight; //odd leftover pixel goes to the right/top

    //2x2 grid: top-left Perspective, top-right Top, bottom-left Front, bottom-right Right
    const int QuadX[ViewportCount] = { 0, HalfWidth, 0, HalfWidth };
    const int QuadY[ViewportCount] = { HalfHeight, HalfHeight, 0, 0 };
    const int QuadW[ViewportCount] = { HalfWidth, RemWidth, HalfWidth, RemWidth };
    const int QuadH[ViewportCount] = { RemHeight, RemHeight, HalfHeight, HalfHeight };

    for(int i = 0; i < ViewportCount; i++)
    {
        FViewport& VP = Viewports[i];
        VP.QuadrantX = QuadX[i];
        VP.QuadrantY = QuadY[i];
        VP.QuadrantWidth = QuadW[i];
        VP.QuadrantHeight = QuadH[i];

        if(i == Viewport_Perspective)
        {
            VP.ViewportCamera.SetClipDimensions((double)WindowWidth, (double)WindowHeight, 0.1, 1000.0);
        }
        else
        {
            //ortho cameras: ClipWidth/Height are world-space frustum size (see Camera.cpp), scaled
            //by quadrant aspect ratio so a square in the scene stays square on screen
            const double AspectRatio = (double)VP.QuadrantWidth / (double)VP.QuadrantHeight;
            VP.ViewportCamera.SetClipDimensions(OrthoClipSize * AspectRatio, OrthoClipSize, 0.1, 1000.0);
        }
    }
}

// Blits any active viewport that has an offscreen Framebuffer assigned into its on-screen
// position. The actual texture bind below is still commented out (Framebuffer has no accessor
// for its attachment texture yet - see FViewport's comment), so this draws BlitQuad with
// whatever texture unit 0 was last bound to, not the viewport's own render.
// Deliberately does NOT glClear anything: viewports without a Framebuffer already rendered
// straight into the default framebuffer during Render()'s main loop, and a blanket clear here
// would erase that work.
void CompositeFramebufferBackedViewports()
{
    const int ActiveViewportCount = bMultiViewMode ? ViewportCount : 1;

    bool bAnyComposited = false;
    for(int i = 0; i < ActiveViewportCount; i++)
    {
        if(Viewports[i].ViewportFramebuffer)
        {
            bAnyComposited = true;
            break;
        }
    }
    if(!bAnyComposited)
    {
        return;
    }

    //full-screen blit quads have no meaningful depth - without disabling depth test here, stale
    //depth values left in the default framebuffer from a previous frame could reject this frame's
    //blit fragments and leave garbage/old content on screen intermittently
    glDisable(GL_DEPTH_TEST);
    glUseProgram(BlitShaderProgram->GetProgramID());
    glUniform1i(glGetUniformLocation(BlitShaderProgram->GetProgramID(), "TexSampler"), 0);

    for(int i = 0; i < ActiveViewportCount; i++)
    {
        const FViewport& VP = Viewports[i];
        if(!VP.ViewportFramebuffer)
        {
            continue;
        }

        const int RectX = bMultiViewMode ? VP.QuadrantX : 0;
        const int RectY = bMultiViewMode ? VP.QuadrantY : 0;
        const int RectW = bMultiViewMode ? VP.QuadrantWidth : Width;
        const int RectH = bMultiViewMode ? VP.QuadrantHeight : Height;

        glViewport(RectX, RectY, RectW, RectH);
        VP.ViewportFramebuffer->BindColorAttachment(0);
        BlitQuad->Draw(GL_TRIANGLES);
    }

    glEnable(GL_DEPTH_TEST); //restore the app-wide invariant everything else relies on
}

void UpdateTiming(GLFWwindow* window)
{
    if(window == nullptr)
    {
        return;
    }

    DeltaTime = (ThisFrameTime = glfwGetTime()) - LastFrameTime;
    LastFrameTime = ThisFrameTime;

    //update timing counter in the window, 4 times a second
    double TimeSinceLastUpdate = ThisFrameTime - LastTimingUpdateTime;
    if (TimeSinceLastUpdate >= 1.0)
    {
        LastTimingUpdateTime = ThisFrameTime;
        LastTimingUpdateFrame = FrameCount;
        double fps = (double)FrameCount / TimeSinceLastUpdate;
        char tmp[128];
        sprintf(tmp, "opengl @ fps: %.2f", fps);
        glfwSetWindowTitle(window, tmp);
        FrameCount = 0;
    }
    FrameCount++;
}

