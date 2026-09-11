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
#include <cassert>
#include <string>
#include <memory>
#include <vector>
#include <array>
#include <algorithm>

#include "ShaderProgram.h"
#include "ShaderObject.h"
#include "ShaderManager.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "MeshData.h"
#include "Texture2D.h"
#include "Framebuffer.h"
#include "UniformBuffer.h"
#include "FrameConstants.h"
#include "Camera.h"
#include "SceneGraph.h"
#include "Gizmo.h"
#include "Grid.h"
#include "SSTextRenderer.h"
#include "ConfigManager.h"
#include "gear/logging/logging.h"
#include "gear/paths/paths.h"
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
void DrawViewportBorders();
void UpdateCameraMovement(GLFWwindow* Window, double DeltaTime);
void KeyboardEventCallback(GLFWwindow* Window, int KeyCode, int ScanCode, int Action, int Modifiers);
void MouseButtonEventCallback(GLFWwindow* Window, int Button, int Action, int Modifiers);
void CursorPositionEventCallback(GLFWwindow* Window, double XPos, double YPos);
void ScrollEventCallback(GLFWwindow* Window, double XOffset, double YOffset);
int GetViewportIndexAtCursor(double CursorX, double CursorY);
void GetPerspectiveViewportRect(int& OutX, int& OutY, int& OutW, int& OutH);
void ScreenPointToWorldRay(double CursorX, double CursorY, Camera& Cam, int RectX, int RectY, int RectW, int RectH, glm::vec3& OutRayOrigin, glm::vec3& OutRayDirection);
bool ProjectWorldToScreen(const glm::vec3& WorldPoint, const glm::mat4& ViewProjectionMatrix, int RectX, int RectY, int RectW, int RectH, glm::vec2& OutPixel);
void WindowResizeEventCallback(GLFWwindow* Window, int NewWidth, int NewHeight);

void ErrorCallback(int error, const char* description);

void Cleanup();

Rendering::FMeshData GenerateTetrahedronMeshData();
Rendering::FMeshData GenerateHexTorusMeshData(float InnerRadius, float OuterRadius, int SideCount, float HalfDepth);
Rendering::FMeshData GenerateTexturedCubeMeshData(float HalfSize);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//main window for the sim
GLFWwindow* MainWindow = nullptr;

constexpr int DefaultWidth = 1920;
constexpr int DefaultHeight = 1080;

//world-space frustum size (see Camera::SetClipDimensions) for the 3 stationary orthographic
//viewport cameras - see RecomputeViewportQuadrants
constexpr double OrthoClipSize = 16.0;

//pixel gap left between quadrants in multi-view mode, filled in by DrawViewportBorders() - see
//RecomputeViewportQuadrants
constexpr int ViewportBorderThickness = 2;

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

    // Orthonormal basis spanning this viewport's reference grid plane (its normal is their cross
    // product) - see Grid::Draw. Defaults to the XZ/ground plane, shared by Perspective and Top;
    // Front/Right override this at setup below to match the plane they actually look across.
    glm::vec3 GridTangent = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 GridBitangent = glm::vec3(0.0f, 0.0f, 1.0f);

    // World-space frustum height for this viewport's orthographic camera - starts at OrthoClipSize
    // (matching every ortho viewport's initial zoom) and is adjusted per-viewport by mouse-wheel
    // zoom (see ScrollEventCallback). Unused by the Perspective viewport, which sizes its frustum
    // from the window's pixel dimensions instead (see RecomputeViewportQuadrants).
    double OrthoWorldHeight = OrthoClipSize;
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

//reference grid, shared across all 4 viewports (see Grid.h) - each Draw() call is parameterized
//by that viewport's FViewport::GridTangent/GridBitangent
Grid ViewportGrid;

//screen-space texts
SSTextRenderer TextRenderer;

//when true, all 4 Viewports render simultaneously into a 2x2 grid instead of just the
//perspective camera rendering fullscreen. Toggled by Space (KeyboardEventCallback).
static bool bMultiViewMode = false;

//vsync state, toggled by F7 (KeyboardEventCallback) - matches the glfwSwapInterval(1) set at
//startup in InitGraphics()
static bool bVsyncEnabled = true;

//camera fly controls (active only while the right mouse button is held, mirroring most editors)
static bool bRightMouseHeld = false;
static bool bFirstCursorSample = true;
static double LastCursorX = 0.0;
static double LastCursorY = 0.0;

//index (into Viewports) of whichever viewport the current RMB drag is targeting, latched at press
//time from the cursor's quadrant - Viewport_Perspective flies the camera as before, Top/Front/Right
//pan instead (see CursorPositionEventCallback), -1 means no drag is active (or it started in the
//border gap between quadrants, which is a no-op)
static int ActiveMouseViewport = -1;

//true only while LMB/MB3 is held AND the press landed in the perspective viewport (see
//MouseButtonEventCallback) - unlike RMB, these two are perspective-only (see GetViewportIndexAtCursor),
//so there's no per-viewport routing to latch, just a single "is this drag live" flag each.
//bGlideActive: LMB - yaw with horizontal mouse movement, glide forward/back along the ground
//(XZ) plane with vertical movement. bYZPanActive: MB3 - pan along the camera's local Up/Forward
//(YZ) plane, no strafing.
static bool bGlideActive = false;
static bool bYZPanActive = false;

//gizmo drag state - LMB press checks for a gizmo-axis hit before falling back to bGlideActive
//above, so a drag on a handle takes priority over camera glide (perspective-viewport only, same
//scoping as bGlideActive/bYZPanActive)
static bool bGizmoDragActive = false;
static EGizmoAxis GizmoDragAxis = EGizmoAxis::None;

//timing
static double LastFrameTime = 0;
static double ThisFrameTime = 0;
static double LastTimingUpdateTime = 0;
static double DeltaTime = 0.0;
static unsigned int FrameCount = 0;
static unsigned int LastTimingUpdateFrame = 0;

//rolling frametime average - updated every frame (unlike the once-a-second FPS counter above),
//via a fixed-size circular buffer + running sum so the per-frame cost stays O(1)
constexpr int FrametimeWindowSize = 120; //~2s at 60fps - tune via this one constant
static double FrametimeWindow[FrametimeWindowSize] = {};
static int FrametimeWindowIndex = 0;
static int FrametimeWindowCount = 0;
static double FrametimeWindowSum = 0.0;

static bool bUsingWSL = false;

//exit flag
static bool bRequestedExit = false;

//initialization flags
static bool bGLFWInitialized = false;

//GL version actually negotiated with the platform in CreateBestWindow (0 until a window exists)
static int NegotiatedGLVersionMajor = 0;
static int NegotiatedGLVersionMinor = 0;

//colored tetrahedron centered at the origin - see GenerateTetrahedronMeshData() for the geometry
//itself (plain CPU-side FMeshData) and UploadMesh() (MeshData.h) for the GPU upload step that
//builds this. unique_ptr for the same deferred-construction reason as HexTorus/TexturedCube below
//(VertexArray's constructor needs a live GL context, so this can't be built at global-init time,
//only once InitGraphics() has created the window/context).
std::unique_ptr<Rendering::VertexArray> Tetrahedron;

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

//scene hierarchy for the render objects below - see SceneGraph.h for the handle/depth-bucket design
SceneGraph MainSceneGraph;
NodeHandle TetrahedronNode;
NodeHandle HexTorusNode;
NodeHandle TexturedCubeNode;

//which scene node (if any) the gizmo currently targets - invalid handle means nothing selected
NodeHandle SelectedNode;

//the one place SelectedNode actually changes, called directly from the "Scene" ImGui panel
void SelectNode(NodeHandle Handle)
{
    SelectedNode = Handle;
}

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
//TEMPORARY smoke test for SceneGraph - exercises handle validity, depth tracking, world-matrix
//composition, reparenting, and stale-handle detection after slot reuse, against a throwaway
//graph (not MainSceneGraph). No GL context needed, so it runs before InitGraphics(). Delete once
//eyeballed - this stands in for a real test framework, which the project doesn't have yet.
void SceneGraphSmokeTest()
{
    SceneGraph TestGraph;

    const NodeHandle A = TestGraph.CreateNode();
    const NodeHandle B = TestGraph.CreateNode(A);
    const NodeHandle C = TestGraph.CreateNode(B);
    assert(TestGraph.GetNode(A)->GetDepth() == 0);
    assert(TestGraph.GetNode(B)->GetDepth() == 1);
    assert(TestGraph.GetNode(C)->GetDepth() == 2);

    TestGraph.GetLocalTransform(A).SetTranslation(glm::vec3(1.0f, 0.0f, 0.0f));
    TestGraph.UpdateWorldTransforms();
    assert(glm::vec3(TestGraph.GetWorldMatrix(B)[3]) == glm::vec3(1.0f, 0.0f, 0.0f));
    assert(glm::vec3(TestGraph.GetWorldMatrix(C)[3]) == glm::vec3(1.0f, 0.0f, 0.0f));

    TestGraph.Reparent(C, {});
    TestGraph.UpdateWorldTransforms();
    assert(TestGraph.GetNode(C)->GetDepth() == 0);
    assert(glm::vec3(TestGraph.GetWorldMatrix(C)[3]) == glm::vec3(0.0f, 0.0f, 0.0f));

    TestGraph.DestroyNode(B);
    const auto& RemainingChildrenOfA = TestGraph.GetNode(A)->GetChildren();
    assert(std::find(RemainingChildrenOfA.begin(), RemainingChildrenOfA.end(), B) == RemainingChildrenOfA.end());

    //D reuses B's freed slot index - the old handle to B must still read as invalid despite that,
    //since B's slot generation was bumped on destruction and D's handle carries the new generation
    const NodeHandle D = TestGraph.CreateNode();
    assert(!TestGraph.IsValid(B));
    assert(TestGraph.IsValid(D));

    LogInfo("SceneGraph smoke test passed.\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// initialization functions
bool Init(int argc, char** argv, char** envp)
{
    setvbuf(stdout, nullptr, _IOLBF, 0);   // line-buffered regardless of TTY detection
    // or _IONBF for fully unbuffered, like stderr
    LogInfo("initializing...\n");

    SceneGraphSmokeTest();

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

    ConfigManager::Get().LoadFromFile(gear::GetExecutableDir() + "/config/default.conf");

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

//builds a colored tetrahedron's geometry, centered at the origin, as plain CPU-side FMeshData - see
//UploadMesh() (MeshData.h) for the step that turns this into a GPU-ready VertexArray. One triangle
//(3 verts) per face rather than 4 shared corner verts, so each face can carry its own copy of the 3
//corner colors. Apex-up construction: V0 sits directly above the centroid on +Y, the other 3
//vertices form an equilateral triangle in a horizontal plane below it, spaced 120 degrees apart
//around the Y axis, with V1 positioned in the +Z direction from center. For a regular tetrahedron
//with circumradius R (center-to-vertex distance) = 3.0: apex at (0,R,0); base plane at y=-R/3 (so
//the 4 vertices' centroid lands exactly on the origin); base horizontal radius = R*2*sqrt(2)/3 (the
//value that makes apex-to-base and base-to-base edge lengths equal, i.e. makes it regular).
//Face winding is CCW as seen from outside each face (verified by hand against the tetrahedron's
//centroid at the origin), matching the codebase's winding convention elsewhere even though face
//culling isn't currently enabled.
Rendering::FMeshData GenerateTetrahedronMeshData()
{
    Rendering::FMeshData MeshData;
    MeshData.Positions =
    {
        //face opposite V0=(0, 3, 0): V1,V3,V2
        { 0.000000f, -1.0f,  2.828427f}, {-2.449490f, -1.0f, -1.414214f}, { 2.449490f, -1.0f, -1.414214f},
        //face opposite V1=(0, -1, 2.828427): V0,V2,V3
        { 0.0f,  3.0f,  0.0f}, { 2.449490f, -1.0f, -1.414214f}, {-2.449490f, -1.0f, -1.414214f},
        //face opposite V2=(2.449490, -1, -1.414214): V0,V3,V1
        { 0.0f,  3.0f,  0.0f}, {-2.449490f, -1.0f, -1.414214f}, { 0.000000f, -1.0f,  2.828427f},
        //face opposite V3=(-2.449490, -1, -1.414214): V0,V1,V2
        { 0.0f,  3.0f,  0.0f}, { 0.000000f, -1.0f,  2.828427f}, { 2.449490f, -1.0f, -1.414214f},
    };

    //per-corner colors (V0=red, V1=green, V2=blue, V3=yellow), repeated per-face in the same order
    //as Positions above so each corner keeps the same color everywhere it appears - Gouraud-blends
    //within each face, same visual language as the original triangle's red/green/blue gradient.
    MeshData.Colors =
    {
        {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, //V1,V3,V2
        {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, //V0,V2,V3
        {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, //V0,V3,V1
        {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, //V0,V1,V2
    };

    return MeshData;
}

//builds the hexagonal torus's ("hex nut") geometry - a hexagonal ring extruded along Z into a solid
//loop (front/back faces plus inner/outer walls) - as plain CPU-side FMeshData. 4 verts per side
//(outer/inner rim, front/back face), indexed - not a single continuous triangle strip like the old
//flat version, since front/back/inner-wall/outer-wall can't be expressed as one strip without
//degenerate triangles; GL_TRIANGLES is simpler and clearer here.
Rendering::FMeshData GenerateHexTorusMeshData(float InnerRadius, float OuterRadius, int SideCount, float HalfDepth)
{
    Rendering::FMeshData MeshData;

    for(int Side = 0; Side < SideCount; Side++)
    {
        const float Angle = glm::radians(360.0f * (float)Side / (float)SideCount);
        const float CosA = cosf(Angle);
        const float SinA = sinf(Angle);

        //vertex order per side: 0=OuterFront, 1=InnerFront, 2=OuterBack, 3=InnerBack
        MeshData.Positions.push_back({OuterRadius * CosA, OuterRadius * SinA,  HalfDepth});
        MeshData.Positions.push_back({InnerRadius * CosA, InnerRadius * SinA,  HalfDepth});
        MeshData.Positions.push_back({OuterRadius * CosA, OuterRadius * SinA, -HalfDepth});
        MeshData.Positions.push_back({InnerRadius * CosA, InnerRadius * SinA, -HalfDepth});
        for(int i = 0; i < 4; i++)
        {
            MeshData.Colors.push_back({1.0f, 0.6f, 0.0f});
        }
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
        MeshData.Indices.insert(MeshData.Indices.end(), {OuterFront, NextOuterFront, NextInnerFront, NextInnerFront, InnerFront, OuterFront});
        //back face (-Z outward, reversed relative to front): InnerBack, NextInnerBack, NextOuterBack, ...
        MeshData.Indices.insert(MeshData.Indices.end(), {InnerBack, NextInnerBack, NextOuterBack, NextOuterBack, OuterBack, InnerBack});
        //outer wall (radially outward): OuterFront, OuterBack, NextOuterBack, ...
        MeshData.Indices.insert(MeshData.Indices.end(), {OuterFront, OuterBack, NextOuterBack, NextOuterBack, NextOuterFront, OuterFront});
        //inner wall (radially inward, into the hole): InnerFront, NextInnerFront, NextInnerBack, ...
        MeshData.Indices.insert(MeshData.Indices.end(), {InnerFront, NextInnerFront, NextInnerBack, NextInnerBack, InnerBack, InnerFront});
    }

    return MeshData;
}

//builds a cube's geometry (positions + UVs) as plain CPU-side FMeshData, for the textured cube
//demonstrating Texture2D. 6 faces x 4 corners, each face's own 4 verts (not shared cube corners) so
//each face gets its own full 0..1 UV range - sharing corners across faces would need ambiguous
//per-vertex UVs, since a cube corner is part of 3 differently-UV'd faces. Corner order per face is
//CCW as seen from outside (verified by hand via cross product against each face's own normal).
Rendering::FMeshData GenerateTexturedCubeMeshData(float HalfSize)
{
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

    Rendering::FMeshData MeshData;
    for(int Face = 0; Face < 6; Face++)
    {
        for(int Corner = 0; Corner < 4; Corner++)
        {
            const glm::vec3& C = CubeFaces[Face].Corners[Corner];
            MeshData.Positions.push_back({C.x * HalfSize, C.y * HalfSize, C.z * HalfSize});
            MeshData.UVs.push_back(FaceUVs[Corner]);
        }
        const GLuint Base = (GLuint)(Face * 4);
        MeshData.Indices.insert(MeshData.Indices.end(), {Base, Base + 1, Base + 2, Base + 2, Base + 3, Base});
    }

    return MeshData;
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

    //create the reference grid's shader and shared NDC quad
    ViewportGrid.Initialize();

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

    Tetrahedron = Rendering::UploadMesh(GenerateTetrahedronMeshData());
    TetrahedronNode = MainSceneGraph.CreateNode();

    //hexagonal torus surrounding the tetrahedron above, built with VertexBuffer/IndexBuffer/VertexArray,
    //same as the tetrahedron. Extruded along Z into a solid hex-nut-shaped torus (front/back faces +
    //inner/outer walls) rather than the flat hexagonal washer this used to be - the passthrough
    //shader does no lighting/normal shading at all (flat vertex-color Gouraud interpolation only),
    //so no per-face vertex duplication is needed purely for shading correctness; only geometric
    //position differs between faces/walls.
    {
        constexpr float InnerRadius = 4.0f;
        constexpr float OuterRadius = 5.0f;
        constexpr int SideCount = 6;
        constexpr float HalfDepth = 0.5f; //full depth 1.0, matching the ring's radial width (Outer-Inner)

        HexTorus = Rendering::UploadMesh(GenerateHexTorusMeshData(InnerRadius, OuterRadius, SideCount, HalfDepth));

        //parented to the tetrahedron - the torus visually surrounds it, so this demonstrates the
        //hierarchy meaningfully: rotating the tetrahedron below carries the torus along with it
        HexTorusNode = MainSceneGraph.CreateNode(TetrahedronNode);
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

        TexturedCube = Rendering::UploadMesh(GenerateTexturedCubeMeshData(CubeHalfSize));

        //independent root node - cube's offset now lives on its local transform rather than
        //baked into its mesh data
        TexturedCubeNode = MainSceneGraph.CreateNode(TetrahedronNode);
        MainSceneGraph.GetLocalTransform(TexturedCubeNode).SetTranslation(glm::vec3(CubeOffsetX, 0.0f, 0.0f));
    }

    //setup the camera: perspective projection matching the window, positioned back from the origin and looking at it
    MainCamera = Camera((double)Width, (double)Height, 0.1, 1000.0, ECameraProjectionMode::Perspective, 50.0);
    MainCamera.SetLocation(glm::vec3(0.0f, 0.0f, 8.0f));

    //opts the perspective viewport into the offscreen-Framebuffer path (see FViewport's comment) -
    //Render() resizes this to the viewport's actual rect every frame, PerspectiveFramebufferSpec's
    //Width/Height here are just the initial allocation size
    Viewports[Viewport_Perspective].ViewportFramebuffer = std::make_unique<Rendering::Framebuffer>(PerspectiveFramebufferSpec);

    //three stationary orthographic cameras, one looking down each major world axis at the scene
    //origin - identity rotation always renders looking down -Z (that's fixed by the view/projection
    //convention in Camera.cpp, independent of Transform::WorldForward, which just defines what
    //GetForwardVector() points along - see Transform.cpp), confirmed against MainCamera's own setup below
    constexpr double OrthoDistance = 8.0;

    Viewports[Viewport_Top].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Top].ViewportCamera.SetLocation(glm::vec3(0.0f, (float)OrthoDistance, 0.0f));
    Viewports[Viewport_Top].ViewportCamera.SetRotation(glm::vec3(-90.0f, 0.0f, 0.0f)); //pitch down to look straight down -Y

    Viewports[Viewport_Front].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Front].ViewportCamera.SetLocation(glm::vec3(0.0f, 0.0f, (float)OrthoDistance));
    //no rotation needed - identity already looks down -Z toward the origin, same as MainCamera's default
    Viewports[Viewport_Front].GridTangent = glm::vec3(1.0f, 0.0f, 0.0f);   //X
    Viewports[Viewport_Front].GridBitangent = glm::vec3(0.0f, 1.0f, 0.0f); //Y - grid lies in the XY plane this camera looks across

    Viewports[Viewport_Right].ViewportCamera = Camera(OrthoClipSize, OrthoClipSize, 0.1, 1000.0, ECameraProjectionMode::Orthographic);
    Viewports[Viewport_Right].ViewportCamera.SetLocation(glm::vec3((float)OrthoDistance, 0.0f, 0.0f));
    Viewports[Viewport_Right].ViewportCamera.SetRotation(glm::vec3(0.0f, 90.0f, 0.0f)); //yaw to look down -X
    Viewports[Viewport_Right].GridTangent = glm::vec3(0.0f, 1.0f, 0.0f);   //Y
    Viewports[Viewport_Right].GridBitangent = glm::vec3(0.0f, 0.0f, 1.0f); //Z - grid lies in the YZ plane this camera looks across

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
    glfwSetScrollCallback(MainWindow, ScrollEventCallback);

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

    MainSceneGraph.UpdateWorldTransforms();
}

void Render(double dt)
{
    //start a new Dear ImGui frame - nothing is actually drawn until ImGui::Render()/RenderDrawData() below
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //smoke test for the ImGui integration - gets replaced by real tool panels (shader playground, etc.) later
    ImGui::ShowDemoWindow();

    //picks which scene node (if any) the gizmo targets - each button calls SelectNode directly,
    //so there's no intermediate index/state that has to stay in sync with SelectedNode
    ImGui::Begin("Scene");
    if(ImGui::Button("None")) SelectNode({});
    ImGui::SameLine(); if(ImGui::Button("Tetrahedron")) SelectNode(TetrahedronNode);
    ImGui::SameLine(); if(ImGui::Button("HexTorus")) SelectNode(HexTorusNode);
    ImGui::SameLine(); if(ImGui::Button("TexturedCube")) SelectNode(TexturedCubeNode);
    ImGui::End();

    const double Red = 0.0f;//cos(ThisFrameTime);
    const double Green = 0.0f;//cos(ThisFrameTime);
    const double Blue = 0.0f;//cos(ThisFrameTime);

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
        if(Tetrahedron)
        {
            const glm::mat4 Model = MainSceneGraph.GetWorldMatrix(TetrahedronNode);
            glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->GetProgramID(), "Model"), 1, GL_FALSE, &Model[0][0]);
            Tetrahedron->Draw(GL_TRIANGLES);
        }

        //draw the hex torus around the tetrahedron - same shader/uniforms, geometry built via the
        //same VertexBuffer/IndexBuffer/VertexArray classes as the tetrahedron above
        if(HexTorus)
        {
            const glm::mat4 Model = MainSceneGraph.GetWorldMatrix(HexTorusNode);
            glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->GetProgramID(), "Model"), 1, GL_FALSE, &Model[0][0]);
            HexTorus->Draw(GL_TRIANGLES);
        }

        //draw the checkerboard-textured cube next to the tetrahedron/hex-torus, proving out Texture2D
        if(TexturedCube && CheckerTexture)
        {
            glUseProgram(TexturedShaderProgram->GetProgramID());
            glUniform1i(glGetUniformLocation(TexturedShaderProgram->GetProgramID(), "TexSampler"), 0);
            const glm::mat4 Model = MainSceneGraph.GetWorldMatrix(TexturedCubeNode);
            glUniformMatrix4fv(glGetUniformLocation(TexturedShaderProgram->GetProgramID(), "Model"), 1, GL_FALSE, &Model[0][0]);
            CheckerTexture->Bind(0);
            TexturedCube->Draw(GL_TRIANGLES);
        }

        //reference grid, occluded by/occluding the scene geometry above via its analytic
        //gl_FragDepth write (see resource/grid.fs) - drawn before the gizmo so the gizmo (which
        //disables depth testing) still overlays on top of it
        ViewportGrid.Draw(VP.ViewportCamera, VP.GridTangent, VP.GridBitangent);

        //gizmo stays perspective-only - drawn here (not after the loop) so it's naturally
        //confined to the perspective viewport's own rect/scissor in multi-view mode too. Synced
        //each frame from SelectedNode's *resolved world* position/rotation (not its bare local
        //transform, which for a child like HexTorus wouldn't reflect where it actually is) -
        //rotation isn't consumed by Draw() yet but is threaded through for a future world-space-
        //vs-local-space gizmo toggle.
        if(SelectedNode.IsValid())
        {
            GizmoTargetTransform.SetTranslation(glm::vec3(MainSceneGraph.GetWorldMatrix(SelectedNode)[3]));
            GizmoTargetTransform.SetRotation(MainSceneGraph.GetWorldRotation(SelectedNode));
            Gizmo.Draw(GizmoTargetTransform, VP.ViewportCamera.GetLocation());
        }

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
    DrawViewportBorders();

    //label each active viewport (e.g. "Top (+Y)") so it's clear which is which in multi-view mode -
    //axis matches that viewport's camera position set up in InitGraphics() (Top sits at +Y looking
    //down, Front at +Z, Right at +X)
    for(int i = 0; i < ActiveViewportCount; i++)
    {
        const FViewport& VP = Viewports[i];

        const char* Label = "Perspective";
        if(i == Viewport_Top)    Label = "Top (+Y)";
        else if(i == Viewport_Front)  Label = "Front (+Z)";
        else if(i == Viewport_Right)  Label = "Right (+X)";

        const int RectX = bMultiViewMode ? VP.QuadrantX : 0;
        const int RectY = bMultiViewMode ? VP.QuadrantY : 0;
        const int RectW = bMultiViewMode ? VP.QuadrantWidth : Width;
        const int RectH = bMultiViewMode ? VP.QuadrantHeight : Height;

        //top-center Label within its viewport's rect. TextRenderer is top-left-origin/Y-down and
        //(X, Y) is the baseline-left origin; Quadrant rects are bottom-left-origin, so the
        //quadrant's top edge converts as Height - (RectY + RectH). FontPixelHeight offsets down
        //from that edge to the text's baseline, matching the HUD text's 12/28 top margin below.
        constexpr float FontPixelHeight = 24.0f; //matches TextRenderer.Initialize() in InitGraphics()
        const float TextWidth = TextRenderer.MeasureTextWidth(Label);
        const float LabelX = (float)RectX + ((float)RectW - TextWidth) * 0.5f;
        const float LabelY = (float)(Height - (RectY + RectH)) + FontPixelHeight;
        TextRenderer.DrawText(Label, LabelX, LabelY, glm::vec3(1.0f, 1.0f, 1.0f));
    }

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

void ToggleVsync()
{
    bVsyncEnabled = !bVsyncEnabled;
    glfwSwapInterval(bVsyncEnabled ? 1 : 0);
    LogInfo("vsync %s\n", bVsyncEnabled ? "enabled" : "disabled");
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

    if(KeyCode == GLFW_KEY_F7)
    {
        ToggleVsync();
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

//which viewport (if any) a screen-space cursor position falls within - Perspective always claims
//the whole window in single-view mode since it's the only thing rendering; in multi-view mode each
//of the 4 quadrants is hit-tested via its Quadrant* rect, returning -1 for the border gap between
//them. CursorX/Y are in GLFW's convention (top-left origin, Y down); Quadrant* is bottom-left
//origin (matches glViewport/glScissor), so Y is flipped before comparing.
int GetViewportIndexAtCursor(double CursorX, double CursorY)
{
    if(!bMultiViewMode)
    {
        return Viewport_Perspective;
    }

    const double FlippedY = (double)Height - CursorY;
    for(int i = 0; i < ViewportCount; i++)
    {
        const FViewport& VP = Viewports[i];
        if(CursorX >= VP.QuadrantX && CursorX < VP.QuadrantX + VP.QuadrantWidth &&
           FlippedY >= VP.QuadrantY && FlippedY < VP.QuadrantY + VP.QuadrantHeight)
        {
            return i;
        }
    }
    return -1;
}

//the perspective viewport's current on-screen rect, in the same bottom-left-origin convention as
//glViewport/glScissor - matches the RectX/Y/W/H computation already duplicated at each per-viewport
//draw site (e.g. in Render()), just for the two gizmo-picking call sites below, which sit outside
//that per-viewport loop
void GetPerspectiveViewportRect(int& OutX, int& OutY, int& OutW, int& OutH)
{
    const FViewport& VP = Viewports[Viewport_Perspective];
    OutX = bMultiViewMode ? VP.QuadrantX : 0;
    OutY = bMultiViewMode ? VP.QuadrantY : 0;
    OutW = bMultiViewMode ? VP.QuadrantWidth : Width;
    OutH = bMultiViewMode ? VP.QuadrantHeight : Height;
}

//unprojects a cursor pixel (GLFW convention: top-left origin, Y down) into a world-space ray,
//via the near/far NDC points through the inverse view-projection matrix - works uniformly for
//perspective and ortho cameras, though only the perspective viewport calls this today (gizmo
//interaction is scoped there, matching the existing camera controls)
void ScreenPointToWorldRay(double CursorX, double CursorY, Camera& Cam, int RectX, int RectY, int RectW, int RectH, glm::vec3& OutRayOrigin, glm::vec3& OutRayDirection)
{
    const double FlippedY = (double)Height - CursorY; //bottom-left-origin, matches RectY's convention
    const float NdcX = 2.0f * (float)(CursorX - RectX) / (float)RectW - 1.0f;
    const float NdcY = 2.0f * (float)(FlippedY - RectY) / (float)RectH - 1.0f;

    const glm::mat4 InvViewProjection = glm::inverse(Cam.GetViewProjectionMatrix());
    glm::vec4 NearPoint = InvViewProjection * glm::vec4(NdcX, NdcY, -1.0f, 1.0f);
    glm::vec4 FarPoint = InvViewProjection * glm::vec4(NdcX, NdcY, 1.0f, 1.0f);
    NearPoint /= NearPoint.w;
    FarPoint /= FarPoint.w;

    OutRayOrigin = glm::vec3(NearPoint);
    OutRayDirection = glm::normalize(glm::vec3(FarPoint - NearPoint));
}

//inverse of the above: projects a world point to a window pixel (GLFW convention), used to turn a
//world-space axis/ring into a 2D screen direction for interpreting mouse drag deltas. Returns
//false (leaving OutPixel untouched) if the point projects behind the camera.
bool ProjectWorldToScreen(const glm::vec3& WorldPoint, const glm::mat4& ViewProjectionMatrix, int RectX, int RectY, int RectW, int RectH, glm::vec2& OutPixel)
{
    const glm::vec4 Clip = ViewProjectionMatrix * glm::vec4(WorldPoint, 1.0f);
    if(Clip.w <= 1e-4f)
    {
        return false;
    }

    const glm::vec3 Ndc = glm::vec3(Clip) / Clip.w;
    const float PixelXInRect = (Ndc.x * 0.5f + 0.5f) * (float)RectW;
    const float PixelYBottomUp = (Ndc.y * 0.5f + 0.5f) * (float)RectH;
    OutPixel.x = (float)RectX + PixelXInRect;
    OutPixel.y = (float)Height - ((float)RectY + PixelYBottomUp); //flip to GLFW's top-left/Y-down convention
    return true;
}

void MouseButtonEventCallback(GLFWwindow *Window, int Button, int Action, int Modifiers)
{
    //don't start camera-fly/pan from a click ImGui already claimed (e.g. on a panel/widget)
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    if(Button == GLFW_MOUSE_BUTTON_RIGHT)
    {
        bRightMouseHeld = (Action == GLFW_PRESS);

        if(bRightMouseHeld)
        {
            double CursorX, CursorY;
            glfwGetCursorPos(Window, &CursorX, &CursorY);
            //latched for the duration of the drag - which viewport this targets doesn't change even if
            //the cursor wanders into another quadrant while still held
            ActiveMouseViewport = GetViewportIndexAtCursor(CursorX, CursorY);

            //only the perspective fly-cam needs the FPS-style hidden/locked cursor; ortho panning keeps
            //the cursor visible (standard pan-tool feel, and sidesteps the WSL cursor-warp bug for this path)
            if(!bUsingWSL && ActiveMouseViewport == Viewport_Perspective)
            {
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
            ActiveMouseViewport = -1;
        }
    }
    else if(Button == GLFW_MOUSE_BUTTON_LEFT)
    {
        //LMB "glide" is perspective-only (see GetViewportIndexAtCursor for the single-view-mode/
        //quadrant hit-test) - a press elsewhere (an ortho quadrant, or the border gap) is just ignored.
        //A press that hits a gizmo handle starts a gizmo drag instead of glide.
        if(Action == GLFW_PRESS)
        {
            double CursorX, CursorY;
            glfwGetCursorPos(Window, &CursorX, &CursorY);
            const bool bInPerspectiveViewport = (GetViewportIndexAtCursor(CursorX, CursorY) == Viewport_Perspective);

            bGizmoDragActive = false;
            if(bInPerspectiveViewport && SelectedNode.IsValid())
            {
                int RectX, RectY, RectW, RectH;
                GetPerspectiveViewportRect(RectX, RectY, RectW, RectH);

                glm::vec3 RayOrigin, RayDirection;
                ScreenPointToWorldRay(CursorX, CursorY, MainCamera, RectX, RectY, RectW, RectH, RayOrigin, RayDirection);

                const glm::vec3 TargetLocation = glm::vec3(MainSceneGraph.GetWorldMatrix(SelectedNode)[3]);
                const float GizmoScale = TransformGizmo::ComputeScale(MainCamera.GetLocation(), TargetLocation);
                const EGizmoAxis PickedAxis = Gizmo.PickAxis(RayOrigin, RayDirection, TargetLocation, GizmoScale);

                if(PickedAxis != EGizmoAxis::None)
                {
                    bGizmoDragActive = true;
                    GizmoDragAxis = PickedAxis;
                    bFirstCursorSample = true;
                }
            }

            if(!bGizmoDragActive)
            {
                bGlideActive = bInPerspectiveViewport;
                if(bGlideActive)
                {
                    //FPS-style hidden/locked cursor, same as RMB fly - glide's horizontal axis is a yaw look
                    if(!bUsingWSL)
                    {
                        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    }
                    bFirstCursorSample = true;
                }
            }
        }
        else if(bGizmoDragActive)
        {
            bGizmoDragActive = false;
            GizmoDragAxis = EGizmoAxis::None;
        }
        else if(bGlideActive)
        {
            if(!bUsingWSL)
            {
                glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            bGlideActive = false;
        }
    }
    else if(Button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        //MB3 pan is perspective-only, same hit-test as LMB glide above; also hides/locks the cursor
        //like RMB/LMB, so it doesn't wander off over ImGui or another quadrant mid-drag.
        if(Action == GLFW_PRESS)
        {
            double CursorX, CursorY;
            glfwGetCursorPos(Window, &CursorX, &CursorY);
            bYZPanActive = (GetViewportIndexAtCursor(CursorX, CursorY) == Viewport_Perspective);
            if(bYZPanActive)
            {
                if(!bUsingWSL)
                {
                    glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                bFirstCursorSample = true;
            }
        }
        else
        {
            if(bYZPanActive && !bUsingWSL)
            {
                glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            bYZPanActive = false;
        }
    }
}

void CursorPositionEventCallback(GLFWwindow *Window, double XPos, double YPos)
{
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    const bool bRightDragActive = bRightMouseHeld && ActiveMouseViewport >= 0;
    if(!bRightDragActive && !bGlideActive && !bYZPanActive && !bGizmoDragActive)
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
    const double OldCursorX = LastCursorX;
    const double OldCursorY = LastCursorY;
    LastCursorX = XPos;
    LastCursorY = YPos;

    if(bGizmoDragActive)
    {
        int RectX, RectY, RectW, RectH;
        GetPerspectiveViewportRect(RectX, RectY, RectW, RectH);

        const glm::mat4 ViewProjectionMatrix = MainCamera.GetViewProjectionMatrix();
        const glm::vec3 TargetLocation = glm::vec3(MainSceneGraph.GetWorldMatrix(SelectedNode)[3]);
        const glm::vec3 WorldAxisDirection = TransformGizmo::GetAxisDirection(GizmoDragAxis);

        float Delta = 0.0f;
        if(Gizmo.GetMode() == EGizmoMode::Rotate)
        {
            //angle-around-the-projected-center approach: the change in the cursor's angle around
            //TargetLocation's 2D screen position, in degrees, normalized to avoid an atan2 wraparound
            //jump. Note: whether a given drag direction reads as clockwise-positive or negative
            //depends on which side of the ring the camera is viewing from - an accepted
            //simplification of this approach.
            glm::vec2 ScreenCenter;
            if(ProjectWorldToScreen(TargetLocation, ViewProjectionMatrix, RectX, RectY, RectW, RectH, ScreenCenter))
            {
                const double PrevAngle = atan2(OldCursorY - ScreenCenter.y, OldCursorX - ScreenCenter.x);
                const double NewAngle = atan2(YPos - ScreenCenter.y, XPos - ScreenCenter.x);
                double DeltaAngleDegrees = -glm::degrees(NewAngle - PrevAngle);
                if(DeltaAngleDegrees > 180.0) DeltaAngleDegrees -= 360.0;
                if(DeltaAngleDegrees < -180.0) DeltaAngleDegrees += 360.0;

                Delta = (float)DeltaAngleDegrees;
            }
        }
        else
        {
            //project the axis to screen space and take how far the mouse moved along that 2D
            //direction, so dragging "along" the drawn handle (whichever way it points on screen)
            //moves/scales the object, regardless of camera angle
            glm::vec2 ScreenOrigin, ScreenAxisTip;
            if(ProjectWorldToScreen(TargetLocation, ViewProjectionMatrix, RectX, RectY, RectW, RectH, ScreenOrigin) &&
               ProjectWorldToScreen(TargetLocation + WorldAxisDirection, ViewProjectionMatrix, RectX, RectY, RectW, RectH, ScreenAxisTip) &&
               ScreenAxisTip != ScreenOrigin)
            {
                const glm::vec2 ScreenAxisDirection = glm::normalize(ScreenAxisTip - ScreenOrigin);
                const glm::vec2 PixelDelta((float)DeltaX, (float)DeltaY);
                const float PixelsAlongAxis = glm::dot(PixelDelta, ScreenAxisDirection);
                constexpr float TranslateSensitivity = 0.01f; //world units per pixel-along-axis
                constexpr float ScaleSensitivity = 0.01f;
                Delta = PixelsAlongAxis * (Gizmo.GetMode() == EGizmoMode::Translate ? TranslateSensitivity : ScaleSensitivity);
            }
        }

        //Translate/Rotate deltas are computed in world space above but must be applied in the
        //target's own local space - for a root node local==world so Direction passes through
        //unchanged, but for a parented node (e.g. HexTorus) it needs converting into the parent's
        //frame first. Scale is always expressed in the target's own local axes regardless of
        //parent, so it never needs this conversion.
        glm::vec3 Direction = WorldAxisDirection;
        const NodeHandle ParentHandle = MainSceneGraph.GetNode(SelectedNode)->GetParent();
        if(ParentHandle.IsValid())
        {
            if(Gizmo.GetMode() == EGizmoMode::Translate)
            {
                Direction = glm::vec3(glm::inverse(MainSceneGraph.GetWorldMatrix(ParentHandle)) * glm::vec4(WorldAxisDirection, 0.0f));
            }
            else if(Gizmo.GetMode() == EGizmoMode::Rotate)
            {
                Direction = glm::inverse(MainSceneGraph.GetWorldRotation(ParentHandle)) * WorldAxisDirection;
            }
        }

        Gizmo.ApplyTransformDeltaAlongDirection(MainSceneGraph.GetLocalTransform(SelectedNode), Direction, Delta);
        return;
    }

    if(bGlideActive)
    {
        //LMB glide: horizontal mouse yaws the camera (same feel as RMB fly's yaw), vertical mouse
        //glides forward/back along the ground (XZ) plane instead of pitching - GroundForward zeroes
        //out any Y component of the camera's current facing so height never changes from this,
        //regardless of whatever pitch the camera was left at by a previous RMB fly.
        constexpr float GlideYawSensitivity = 0.15f;
        constexpr float GlideMoveSensitivity = 0.03f; //world units per pixel of vertical mouse movement

        MainCamera.Yaw((float)-DeltaX * GlideYawSensitivity);
        MainCamera.GlideBackward((float)DeltaY * GlideMoveSensitivity);
        return;
    }

    if(bYZPanActive)
    {
        //MB3 pan: translate along the camera's local Right/Up plane (standard screen-space pan), no
        //dolly - both axes inverted relative to the ortho RMB-pan's "content follows cursor"
        //convention above, per feel testing.
        constexpr float YZPanSensitivity = 0.03f;
        MainCamera.Pan((float)DeltaX * YZPanSensitivity, (float)-DeltaY * YZPanSensitivity);
        return;
    }

    if(ActiveMouseViewport != Viewport_Perspective)
    {
        //ortho pan: translate along the viewport's own screen-space right/up (correct regardless of
        //that camera's fixed pitch/yaw) so the content under the cursor follows the drag. PanScale
        //converts screen pixels to world units - OrthoClipSize is the fixed world-space frustum
        //height every ortho camera uses (see RecomputeViewportQuadrants), so dividing by the
        //quadrant's pixel height gives world-units-per-pixel, same for both axes since the frustum
        //width is aspect-corrected to match.
        FViewport& VP = Viewports[ActiveMouseViewport];
        const float PanScale = (float)(OrthoClipSize / (double)VP.QuadrantHeight);
        VP.ViewportCamera.Pan((float)-DeltaX * PanScale, (float)DeltaY * PanScale);
        return;
    }

    constexpr float MouseSensitivity = 0.15f;

    //yaw rotates around the world up axis, independent of the camera's current tilt; pitch rotates
    //around the camera's own local right axis - Camera::Pitch clamps so it can't flip over
    MainCamera.Yaw((float)-DeltaX * MouseSensitivity);
    MainCamera.Pitch((float)-DeltaY * MouseSensitivity);
}

//mouse-wheel zoom for whichever orthographic viewport (Top/Front/Right) the cursor is currently
//over - GetViewportIndexAtCursor returns Viewport_Perspective unconditionally in single-view mode
//and whenever the cursor is directly over the perspective quadrant, both deliberately excluded
//here since scroll-to-zoom is an ortho-only interaction (the perspective camera uses WASDQE+RMB
//fly instead). Zooms toward the cursor (the world point under it stays under it) rather than
//toward the viewport's center, matching standard editor scroll-zoom behavior.
void ScrollEventCallback(GLFWwindow* Window, double XOffset, double YOffset)
{
    if(ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    double CursorX, CursorY;
    glfwGetCursorPos(Window, &CursorX, &CursorY);
    const int ViewportIndex = GetViewportIndexAtCursor(CursorX, CursorY);
    if(ViewportIndex <= Viewport_Perspective)
    {
        return; //not over an ortho viewport (Perspective, or the border gap, which returns -1)
    }

    FViewport& VP = Viewports[ViewportIndex];

    constexpr double ZoomStepFactor = 0.9; //multiplier per wheel notch; <1 shrinks the frustum (zooms in) on scroll-up
    constexpr double MinOrthoWorldHeight = 1.0;
    constexpr double MaxOrthoWorldHeight = 256.0;

    const double OldWorldHeight = VP.OrthoWorldHeight;
    const double NewWorldHeight = glm::clamp(OldWorldHeight * pow(ZoomStepFactor, YOffset), MinOrthoWorldHeight, MaxOrthoWorldHeight);
    if(NewWorldHeight == OldWorldHeight)
    {
        return; //already at the zoom limit
    }

    //world units per pixel before/after - used below to find how far to shift the camera so the
    //world point under the cursor stays under the cursor rather than the zoom recentering on the
    //viewport's middle
    const double OldWorldUnitsPerPixel = OldWorldHeight / (double)VP.QuadrantHeight;
    const double NewWorldUnitsPerPixel = NewWorldHeight / (double)VP.QuadrantHeight;

    //cursor offset from this viewport's quadrant center, in screen pixels. CursorX/Y are GLFW's
    //top-left/Y-down convention, matching directly for X; QuadrantY is bottom-left origin (see
    //FViewport) so its screen-space top edge is Height - (QuadrantY + QuadrantHeight)
    const double QuadrantCenterX = (double)VP.QuadrantX + (double)VP.QuadrantWidth * 0.5;
    const double QuadrantTopScreenY = (double)Height - (double)(VP.QuadrantY + VP.QuadrantHeight);
    const double QuadrantCenterScreenY = QuadrantTopScreenY + (double)VP.QuadrantHeight * 0.5;
    const double OffsetX = CursorX - QuadrantCenterX;
    const double OffsetYScreen = CursorY - QuadrantCenterScreenY;

    VP.OrthoWorldHeight = NewWorldHeight;
    const double AspectRatio = (double)VP.QuadrantWidth / (double)VP.QuadrantHeight;
    VP.ViewportCamera.SetClipDimensions(NewWorldHeight * AspectRatio, NewWorldHeight, 0.1, 1000.0);

    //pan by screen-right/screen-up (Y flipped since screen-down is world "down" on screen, i.e.
    //-Up), scaled by how much the world-units-per-pixel changed, so the same pixel still maps to
    //the same world point post-zoom
    const float PanScale = (float)(OldWorldUnitsPerPixel - NewWorldUnitsPerPixel);
    VP.ViewportCamera.Pan((float)OffsetX * PanScale, (float)-OffsetYScreen * PanScale);
}

void UpdateCameraMovement(GLFWwindow* Window, double DeltaTime)
{
    //WASDQE fly stays scoped to the perspective camera - not active while nothing is held
    //(ActiveMouseViewport == -1) or while panning an ortho viewport
    if(ActiveMouseViewport != Viewport_Perspective)
    {
        return;
    }

    //resolve which of WASDQE are held into a single local-space intent vector (+X = right, +Y = up,
    //+Z = forward) so simultaneous keys (e.g. strafing while moving forward) don't move the camera
    //faster than a single key would - the actual movement math lives on Camera itself
    //(MoveForward/Right/Up etc.), this just decides how much of each to ask for.
    glm::vec3 MoveIntent(0.0f);
    if(glfwGetKey(Window, GLFW_KEY_W) == GLFW_PRESS) MoveIntent.z += 1.0f;
    if(glfwGetKey(Window, GLFW_KEY_S) == GLFW_PRESS) MoveIntent.z -= 1.0f;
    if(glfwGetKey(Window, GLFW_KEY_D) == GLFW_PRESS) MoveIntent.x += 1.0f;
    if(glfwGetKey(Window, GLFW_KEY_A) == GLFW_PRESS) MoveIntent.x -= 1.0f;
    if(glfwGetKey(Window, GLFW_KEY_E) == GLFW_PRESS) MoveIntent.y += 1.0f;
    if(glfwGetKey(Window, GLFW_KEY_Q) == GLFW_PRESS) MoveIntent.y -= 1.0f;

    if(glm::length(MoveIntent) > 0.0001f)
    {
        constexpr float MoveSpeed = 6.0f;
        MoveIntent = glm::normalize(MoveIntent) * MoveSpeed * (float)DeltaTime;

        //MoveBackward/Left/Down are just negative MoveForward/Right/Up, so a signed distance covers both
        MainCamera.MoveForward(MoveIntent.z);
        MainCamera.MoveRight(MoveIntent.x);
        MainCamera.MoveUp(MoveIntent.y);
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

    //ViewportBorderThickness is carved out of the two quadrants on either side of each shared
    //boundary (not the outer window edges) - split so an odd thickness still tiles exactly, with
    //DrawViewportBorders() filling the resulting gap. NearBorder trims the edge touching the
    //boundary from below/left, FarBorder trims it from above/right; NearBorder+FarBorder == ViewportBorderThickness.
    const int NearBorder = ViewportBorderThickness / 2;
    const int FarBorder = ViewportBorderThickness - NearBorder;

    //2x2 grid: top-left Perspective, top-right Top, bottom-left Front, bottom-right Right
    const int QuadX[ViewportCount] = { 0, HalfWidth + FarBorder, 0, HalfWidth + FarBorder };
    const int QuadY[ViewportCount] = { HalfHeight + FarBorder, HalfHeight + FarBorder, 0, 0 };
    const int QuadW[ViewportCount] = { HalfWidth - NearBorder, RemWidth - FarBorder, HalfWidth - NearBorder, RemWidth - FarBorder };
    const int QuadH[ViewportCount] = { RemHeight - FarBorder, RemHeight - FarBorder, HalfHeight - NearBorder, HalfHeight - NearBorder };

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
            //by quadrant aspect ratio so a square in the scene stays square on screen. Uses this
            //viewport's own zoom level (OrthoWorldHeight), not the fixed OrthoClipSize, so a window
            //resize preserves whatever zoom the user has scrolled to (see ScrollEventCallback)
            const double AspectRatio = (double)VP.QuadrantWidth / (double)VP.QuadrantHeight;
            VP.ViewportCamera.SetClipDimensions(VP.OrthoWorldHeight * AspectRatio, VP.OrthoWorldHeight, 0.1, 1000.0);
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

    glViewport(0, 0, Width, Height); //restore full-window viewport - callers (border/HUD/label text) assume it
    glEnable(GL_DEPTH_TEST); //restore the app-wide invariant everything else relies on
}

// Fills the cross-shaped gap RecomputeViewportQuadrants() leaves between the 4 quadrants (see
// ViewportBorderThickness) with a solid border color, via glScissor+glClear - the same technique
// Render() already uses to confine each quadrant's own clear. No-op outside multi-view mode, since
// there's only one viewport (and no gap) then.
void DrawViewportBorders()
{
    if(!bMultiViewMode)
    {
        return;
    }

    const glm::vec3 BorderColor(0.5f, 0.5f, 0.5f);

    glEnable(GL_SCISSOR_TEST);
    glClearColor(BorderColor.r, BorderColor.g, BorderColor.b, 1.0f);

    //vertical gap: full window height, between the left and right columns of quadrants
    const FViewport& LeftColumn = Viewports[Viewport_Perspective];
    const FViewport& RightColumn = Viewports[Viewport_Top];
    const int VerticalGapX = LeftColumn.QuadrantX + LeftColumn.QuadrantWidth;
    const int VerticalGapWidth = RightColumn.QuadrantX - VerticalGapX;
    glScissor(VerticalGapX, 0, VerticalGapWidth, Height);
    glClear(GL_COLOR_BUFFER_BIT);

    //horizontal gap: full window width, between the top and bottom rows of quadrants
    const FViewport& BottomRow = Viewports[Viewport_Front];
    const FViewport& TopRow = Viewports[Viewport_Perspective];
    const int HorizontalGapY = BottomRow.QuadrantY + BottomRow.QuadrantHeight;
    const int HorizontalGapHeight = TopRow.QuadrantY - HorizontalGapY;
    glScissor(0, HorizontalGapY, Width, HorizontalGapHeight);
    glClear(GL_COLOR_BUFFER_BIT);

    glDisable(GL_SCISSOR_TEST);
}

void UpdateTiming(GLFWwindow* window)
{
    if(window == nullptr)
    {
        return;
    }

    DeltaTime = (ThisFrameTime = glfwGetTime()) - LastFrameTime;
    LastFrameTime = ThisFrameTime;

    //rolling frametime average - updated every frame regardless of the once-a-second window
    //below, via a running sum so this stays O(1) rather than re-summing the whole buffer
    FrametimeWindowSum -= FrametimeWindow[FrametimeWindowIndex];
    FrametimeWindow[FrametimeWindowIndex] = DeltaTime;
    FrametimeWindowSum += DeltaTime;
    FrametimeWindowIndex = (FrametimeWindowIndex + 1) % FrametimeWindowSize;
    if(FrametimeWindowCount < FrametimeWindowSize)
    {
        FrametimeWindowCount++;
    }

    //update timing counter in the window, 4 times a second
    double TimeSinceLastUpdate = ThisFrameTime - LastTimingUpdateTime;
    if (TimeSinceLastUpdate >= 1.0)
    {
        LastTimingUpdateTime = ThisFrameTime;
        LastTimingUpdateFrame = FrameCount;
        double fps = (double)FrameCount / TimeSinceLastUpdate;
        const double AvgFrametimeMs = (FrametimeWindowSum / FrametimeWindowCount) * 1000.0;
        char tmp[160];
        sprintf(tmp, "opengl @ fps: %.2f | avg frametime (last %d frames): %.3f ms", fps, FrametimeWindowCount, AvgFrametimeMs);
        glfwSetWindowTitle(window, tmp);
        FrameCount = 0;
    }
    FrameCount++;
}

