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
#include <optional>
#include <vector>

#include "ShaderProgram.h"
#include "ShaderObject.h"
#include "ShaderManager.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "Texture2D.h"
#include "Camera.h"
#include "Gizmo.h"
#include "SSTextRenderer.h"
#include "ConfigManager.h"
#include "myc/logging/logging.h"
#include "myc/paths/paths.h"
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

static int Width = DefaultWidth;
static int Height = DefaultHeight;

//camera
Camera MainCamera;

//gizmo, and the transform it's currently visualizing
TransformGizmo Gizmo;
Transform GizmoTargetTransform;

//screen-space text
SSTextRenderer TextRenderer;

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

//vertices of a single triangle
static float TriangleVerts[] =
{
   0.0f,  0.5f,  0.0f,
   0.5f, -0.5f,  0.0f,
  -0.5f, -0.5f,  0.0f
};

//colors for a single triangle
static float TriangleColors[] =
{
  1.0f, 0.0f, 0.0f,
  0.0f, 1.0f, 0.0f,
  0.0f, 0.0f, 1.0f
};

//vertex buffer object
GLuint VertexBufferObject_Positions;
GLuint VertexBufferObject_Colors;

//vertex array object
GLuint VertexArrayObject;

//hexagonal ring ("O") drawn around the triangle above, built with the new VertexBuffer/IndexBuffer/
//VertexArray classes instead of raw GL calls. std::optional because VertexArray's constructor calls
//glGenVertexArrays(), which needs a live GL context - this can't be constructed at global-init time,
//only once InitGraphics() has created the window/context (mirrors how Gizmo/TextRenderer are default-
//constructed globally but only actually touch GL from an Initialize() called after that point).
std::optional<Rendering::VertexArray> HexStrip;

//textured quad demonstrating Texture2D, off to the side of the triangle/hex-ring so they don't overlap.
//same std::optional-for-deferred-construction reasoning as HexStrip above applies to both members here.
std::optional<Rendering::VertexArray> TexturedQuad;
std::optional<Rendering::Texture2D> CheckerTexture;

//shader objects
Rendering::ShaderManager* shaderManager;
std::shared_ptr<Rendering::ShaderProgram> PassthroughShaderProgram;
std::shared_ptr<Rendering::ShaderProgram> TexturedShaderProgram;
Rendering::ShaderObject* PassthroughVertexShader;
Rendering::ShaderObject* PassthroughFragmentShader;


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

    //bake the screen-space text renderer's font atlas
    if(!TextRenderer.Initialize("/resource/font/Roboto-Medium.ttf", 24.0f))
    {
        LogError("Failed to initialize SSTextRenderer\n");
    }
    TextRenderer.SetScreenSize(Width, Height);

    ///////////////////////
    /// initialize rendering objects

    for(int i = 0; i < 9; i++)
    {
        TriangleVerts[i] *= 5.0;
    }

    //create vertex buffer for storing per-vertex data
    glGenBuffers(1, &VertexBufferObject_Positions);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Positions);
    glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), TriangleVerts, GL_STATIC_DRAW);

    glGenBuffers(1, &VertexBufferObject_Colors);
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Colors);
    glBufferData(GL_ARRAY_BUFFER, 9 * sizeof(float), TriangleColors, GL_STATIC_DRAW);

    //create vertax array object for storing info about bound objects and what to render
    glGenVertexArrays(1, &VertexArrayObject);
    glBindVertexArray(VertexArrayObject);

    //specify vertex attribute 0 and specify format
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Positions);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    //specify color layout
    glBindBuffer(GL_ARRAY_BUFFER, VertexBufferObject_Colors);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    //hexagonal ring ("O") surrounding the triangle above, built with VertexBuffer/IndexBuffer/VertexArray
    //instead of raw GL calls - proves out the new abstraction alongside the old hand-rolled style right next to it
    {
        constexpr float InnerRadius = 4.0f;
        constexpr float OuterRadius = 5.0f;
        constexpr int SideCount = 6;

        //interleaved {x,y,z, r,g,b} per vertex, alternating outer/inner rim points so the strip
        //tiles the ring; the first outer/inner pair is repeated at the end to close the loop
        std::vector<float> HexVerts;
        for(int Side = 0; Side <= SideCount; Side++)
        {
            const float Angle = glm::radians(360.0f * (float)Side / (float)SideCount);
            const float CosA = cosf(Angle);
            const float SinA = sinf(Angle);

            //outer rim vertex
            HexVerts.insert(HexVerts.end(), {OuterRadius * CosA, OuterRadius * SinA, 0.0f, 1.0f, 0.6f, 0.0f});
            //inner rim vertex
            HexVerts.insert(HexVerts.end(), {InnerRadius * CosA, InnerRadius * SinA, 0.0f, 1.0f, 0.6f, 0.0f});
        }

        std::vector<GLuint> HexIndices(HexVerts.size() / 6);
        for(size_t i = 0; i < HexIndices.size(); i++)
        {
            HexIndices[i] = (GLuint)i;
        }

        HexStrip.emplace();
        HexStrip->AddVertexBuffer(
            Rendering::VertexBuffer(HexVerts.data(), HexVerts.size() * sizeof(float), GL_STATIC_DRAW),
            {
                Rendering::FVertexAttribute{0, 3, GL_FLOAT, false},
                Rendering::FVertexAttribute{1, 3, GL_FLOAT, false}
            },
            6 * sizeof(float));
        HexStrip->SetIndexBuffer(Rendering::IndexBuffer(HexIndices.data(), (unsigned int)HexIndices.size(), GL_STATIC_DRAW));
    }

    //textured quad off to the side of the triangle/hex-ring, proving out Texture2D. The checkerboard
    //is generated as 3-channel RGB (not RGBA) specifically so this exercises Texture2D's non-4-byte-
    //aligned upload path (GL_UNPACK_ALIGNMENT=1), not just the trivial RGBA case.
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
        CheckerTexture.emplace(CheckerPixels.data(), CheckerSize, CheckerSize, 3, GL_REPEAT);

        //interleaved {x,y,z, u,v} per vertex; UVs go to 2.0 (not 1.0) so GL_REPEAT tiles the
        //checkerboard twice across the quad instead of just stretching one copy over it.
        //Offset/size are deliberately tight: at z=0 this camera (50deg vertical FOV, 1920x1080,
        //8 units back) only has ~6.6 units of horizontal half-width in view, and the hex ring's
        //outer radius already reaches 5 - this box keeps the quad clear of the ring on one side
        //and inside the frustum on the other, with a small margin either way.
        constexpr float QuadHalfSize = 0.6f;
        constexpr float QuadOffsetX = 5.7f;
        const float QuadVerts[] =
        {
            QuadOffsetX - QuadHalfSize, -QuadHalfSize, 0.0f,  0.0f, 0.0f,
            QuadOffsetX + QuadHalfSize, -QuadHalfSize, 0.0f,  3.0f, 0.0f,
            QuadOffsetX + QuadHalfSize,  QuadHalfSize, 0.0f,  3.0f, 3.0f,
            QuadOffsetX - QuadHalfSize,  QuadHalfSize, 0.0f,  0.0f, 3.0f,
        };
        const GLuint QuadIndices[] = {0, 1, 2, 2, 3, 0};

        TexturedQuad.emplace();
        TexturedQuad->AddVertexBuffer(
            Rendering::VertexBuffer(QuadVerts, sizeof(QuadVerts), GL_STATIC_DRAW),
            {
                Rendering::FVertexAttribute{0, 3, GL_FLOAT, false},
                Rendering::FVertexAttribute{1, 2, GL_FLOAT, false}
            },
            5 * sizeof(float));
        TexturedQuad->SetIndexBuffer(Rendering::IndexBuffer(QuadIndices, 6, GL_STATIC_DRAW));
    }

    //setup the camera: perspective projection matching the window, positioned back from the origin and looking at it
    MainCamera = Camera((double)Width, (double)Height, 0.1, 1000.0, ECameraProjectionMode::Perspective, 50.0);
    MainCamera.SetLocation(glm::vec3(0.0f, 0.0f, 8.0f));

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

    const glm::mat4 ViewProjectionMatrix = MainCamera.GetViewProjectionMatrix();
    glUseProgram(PassthroughShaderProgram->GetProgramID());
    glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->GetProgramID(), "ViewProjectionMatrix"), 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

    glUseProgram(TexturedShaderProgram->GetProgramID());
    glUniformMatrix4fv(glGetUniformLocation(TexturedShaderProgram->GetProgramID(), "ViewProjectionMatrix"), 1, GL_FALSE, &ViewProjectionMatrix[0][0]);
}

void Render(double dt)
{
    //start a new Dear ImGui frame - nothing is actually drawn until ImGui::Render()/RenderDrawData() below
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //smoke test for the ImGui integration - gets replaced by real tool panels (shader playground, etc.) later
    ImGui::ShowDemoWindow();

    //update uniform variables
    //camera variables
    //timing variables
    //resolution
    //mouse info

    const double Red = cos(ThisFrameTime);
    const double Green = cos(ThisFrameTime);
    const double Blue = cos(ThisFrameTime);

    //set clear color
    glClearColor(Red, Green, Blue, 1.0);

    //clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //if(PassthroughShaderProgram != nullptr && glIsProgram(PassthroughShaderProgram->ProgramID))
    {
        //render here
        glUseProgram(PassthroughShaderProgram->GetProgramID());
        glBindVertexArray(VertexArrayObject);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    //draw the hex ring around the triangle - same shader/uniforms, geometry built via the new
    //VertexBuffer/IndexBuffer/VertexArray classes instead of raw GL calls like the triangle above
    if(HexStrip.has_value())
    {
        HexStrip->Draw(GL_TRIANGLE_STRIP);
    }

    //draw the checkerboard-textured quad next to the triangle/hex-ring, proving out Texture2D
    if(TexturedQuad.has_value() && CheckerTexture.has_value())
    {
        glUseProgram(TexturedShaderProgram->GetProgramID());
        glUniform1i(glGetUniformLocation(TexturedShaderProgram->GetProgramID(), "TexSampler"), 0);
        CheckerTexture->Bind(0);
        TexturedQuad->Draw(GL_TRIANGLES);
    }

    //draw the transform gizmo on top of the scene
    Gizmo.Draw(GizmoTargetTransform, MainCamera.GetLocation(), MainCamera.GetViewProjectionMatrix());

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
    TextRenderer.DrawText("RMB + WASDQE to fly, F5 to reload shaders", 12.0f, 52.0f, glm::vec3(0.7f, 0.7f, 0.7f));
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
    MainCamera.SetClipDimensions((double)NewWidth, (double)NewHeight, 0.1, 1000.0);
    TextRenderer.SetScreenSize(NewWidth, NewHeight);

    LogInfo("Window resized to %dx%d\n", NewWidth, NewHeight);
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

