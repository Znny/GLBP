//
// Created by Ryan on 5/22/2024.
//
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// header file includes

///opengl extension loader and glfw
#include <glad/glad.h>
#include <GLFW/glfw3.h>

//glm
#include <glm/glm.hpp>

///std
#include <stdlib.h>
#include <stdio.h>
#include "ShaderProgram.h"
//#include "matrix_clip_space.hpp"
#include <glm/gtc/matrix_transform.hpp>

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Init(int argc, char** argv, char** envp);
bool InitGraphics();
bool InitInput();

void Run();
void UpdateTiming(GLFWwindow* window);
void Tick(double DeltaTime);
void Render(double DeltaTime);

void ProcessInput();
void KeyboardEventCallback(GLFWwindow* Window, int KeyCode, int ScanCode, int Action, int Modifiers);
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

//projection matrix, representing how objects in space are projected onto the screen
glm::mat4x4 ProjectionMatrix;

//view matrix, representing the viewers transform in space
glm::mat4x4 ViewMatrix;

//vp matrix, needs to be updated when the projection matrix or view matrix changes
glm::mat4x4 ViewProjectionMatrix;


//view distance from the center
double ViewDistance = 10.0;

glm::vec3 EyeLocation;
glm::vec3 UpDirection(0.0, 1.0, 0.0);

//rotation speed in radians/s
double RotationSpeed = 10.0f;

//timing
static double LastFrameTime = 0;
static double ThisFrameTime = 0;
static double LastTimingUpdateTime = 0;
static double DeltaTime = 0.0;
static unsigned int FrameCount = 0;
static unsigned int LastTimingUpdateFrame = 0;

//exit flag
static bool bRequestedExit = false;

//initialization flags
static bool bGLFWInitialized = false;

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

//shader objects
ShaderProgram* PassthroughShaderProgram;
ShaderObject* PassthroughVertexShader;
ShaderObject* PassthroughFragmentShader;


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
    fprintf(stdout, "initializing...\n");

    if(!InitGraphics())
    {
        return false;
    }

    if(!InitInput())
    {
        return false;
    }

    fprintf(stdout, "initialization successful.\n");
    return true;
}

bool InitGraphics()
{
    //attempt initializing GLFW
    if(!(bGLFWInitialized = glfwInit()))
    {
        fprintf(stdout, "GLFW initialization failure.\n");
        return false;
    }

    //set error callback
    glfwSetErrorCallback(ErrorCallback);

    //try to set context version
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);

    //enable anti-aliasing?
    //glfwWindowHint(GLFW_SAMPLES, 4);

    const char* Title = "GLBP";

    //attempt to create the window
    MainWindow = glfwCreateWindow( Width, Height, Title, NULL, NULL);
    if(MainWindow == nullptr)
    {
        fprintf(stdout, "Window creation failed.\n");
        return false;
    }

    //make the newly created opengl context current
    glfwMakeContextCurrent(MainWindow);

    //load gl extensions
    if(!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        fprintf(stdout, "Couldn't load openGL extensions\n");
        return false;
    }
    else
    {
        fprintf(stdout, "GLAD loaded GL extensions\n");
    }

    // get version info
    fprintf(stdout,"Renderer: %s\n", glGetString(GL_RENDERER));
    fprintf(stdout, "OpenGL %s\n", glGetString(GL_VERSION));

    //enable vertical sync
    glfwSwapInterval(1);

    // enable depth testing, making so occluded pixels won't be rendered
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"

    //create shader objects
    PassthroughShaderProgram = new ShaderProgram();
    PassthroughVertexShader = new ShaderObject("resource/passthrough.vs", GL_VERTEX_SHADER);
    PassthroughFragmentShader = new ShaderObject("resource/passthrough.fs", GL_FRAGMENT_SHADER);

    //compile vert and frag shaders
    PassthroughVertexShader->Compile();
    PassthroughFragmentShader->Compile();

    //attach shaders to the shader program
    PassthroughShaderProgram->Attach(PassthroughVertexShader);
    PassthroughShaderProgram->Attach(PassthroughFragmentShader);

    //link the program
    PassthroughShaderProgram->Link();


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

    //setup projection matrix
    constexpr double FoV_y = glm::radians(50.0);
    const double AspectRatio = Width / Height;
    constexpr double zNear = 0.1;
    constexpr double zFar = 1000.0;

    ProjectionMatrix = glm::perspective(FoV_y, AspectRatio, zNear, zFar);

    //set view location, will update every frame for now
    ViewMatrix = glm::lookAt(EyeLocation, glm::vec3(0.0), UpDirection);
    ViewProjectionMatrix = ViewMatrix * ProjectionMatrix;

    glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->ProgramID, "ViewProjectionMatrix"), 1, GL_FALSE, (GLfloat*)&ViewProjectionMatrix);



    return true;
}

bool InitInput()
{
    //set keyboard callback
    glfwSetKeyCallback(MainWindow, KeyboardEventCallback);

    //set resize callback
    glfwSetFramebufferSizeCallback(MainWindow, WindowResizeEventCallback);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// main loop

void Run()
{
    fprintf(stdout, "run started at time %lfs, running...\n", glfwGetTime());

    while(!bRequestedExit)
    {
        UpdateTiming(MainWindow);
        Tick(DeltaTime);
        Render(DeltaTime);
        ProcessInput();
    }

    fprintf(stdout, "running complete.\n");
}

void Tick(double dt)
{
    EyeLocation = glm::vec3(cos(ThisFrameTime) * ViewDistance, 0.0, sin(ThisFrameTime) * ViewDistance);
    ViewMatrix = glm::lookAt(EyeLocation, glm::vec3(0.0, 0.5, 0.0), UpDirection);
    ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;
    fprintf(stdout, "EyeLocation = (%f, %f, %f)\n", EyeLocation.x, EyeLocation.y, EyeLocation.z);
    glUniformMatrix4fv(glGetUniformLocation(PassthroughShaderProgram->ProgramID, "ViewProjectionMatrix"), 1, GL_FALSE, (GLfloat*)&ViewProjectionMatrix);
}

void Render(double dt)
{
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
        glUseProgram(PassthroughShaderProgram->ProgramID);
        glBindVertexArray(VertexArrayObject);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

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
    fprintf(stdout, "cleaning up...\n");

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

    fprintf(stdout, "cleanup complete.\n");
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// event callbacks
void ErrorCallback(int error, const char *description)
{
    ///todo: switch to use bespoke logging once available
    fprintf(stderr, "Error %X: %s", error, description);
}

void KeyboardEventCallback(GLFWwindow *Window, int KeyCode, int ScanCode, int Action, int Modifiers)
{
    if(Action != GLFW_PRESS)
    {
        return;
    }

    if(KeyCode == GLFW_KEY_ESCAPE)
    {
        bRequestedExit = true;
    }
    else if(KeyCode == GLFW_KEY_R)
    {
        PassthroughShaderProgram->Reload();
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
    fprintf(stdout, "Window resized to %dx%d\n", NewWidth, NewHeight);
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

