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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Init(int argc, char** argv, char** envp);
bool InitGraphics();
bool InitInput();

void Run();
void Tick(double DeltaTime);
void Render(double DeltaTime);

void ProcessInput();
void KeyboardEventCallback(GLFWwindow* Window, int KeyCode, int ScanCode, int Action, int Modifiers);
void WindowResizeEventCallback(GLFWwindow* Window, int NewWidth, int NewHeight);

void ErrorCallback(int error, const char* description);

void Cleanup();

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//main window for the sim
static GLFWwindow* MainWindow;

//timing
static double LastFrameTime = 0;
static double ThisFrameTime = 0;

//exit flag
static bool bRequestedExit = false;

//initialization flags
static bool bGLFWInitialized = false;

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

    //spit out environment variables, cuz why not lol
    for(int i = 0; envp[i] != NULL; i++)
    {
        fprintf(stdout, "envp[%d]:%s\n", i, envp[i]);
    }

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

    constexpr int Width = 1920;
    constexpr int Height = 1080;
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
        fprintf(stdout, "Couldn't load openGL extensions");
        return false;
    }

    // get version info
    fprintf(stdout,"Renderer: %s\n", glGetString(GL_RENDERER));
    fprintf(stdout, "OpenGL %s\n", glGetString(GL_VERSION));

    //enable vertical sync
    glfwSwapInterval(1);

    return true;
}

bool InitInput()
{
    //set keyboard callback
    glfwSetKeyCallback(MainWindow, KeyboardEventCallback);

    glfwSetFramebufferSizeCallback(MainWindow, WindowResizeEventCallback);

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// main loop

void Run()
{
    fprintf(stdout, "run started at time %lfs, running...\n", glfwGetTime());

    double DeltaTime = 0.0;
    while(!bRequestedExit)
    {
        ThisFrameTime = glfwGetTime();
        DeltaTime = (ThisFrameTime = glfwGetTime()) - LastFrameTime;

        Tick(DeltaTime);
        Render(DeltaTime);
        ProcessInput();
    }
    fprintf(stdout, "running complete.\n");
}

void Tick(double DeltaTime)
{

}

void Render(double DeltaTime)
{
    const double Red = cos(ThisFrameTime);
    const double Green = cos(ThisFrameTime);
    const double Blue = cos(ThisFrameTime);

    //set clear color
    glClearColor(Red, Green, Blue, 1.0);

    //clear the color and depth buffers
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    //render here
    //
    //

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

}

void WindowResizeEventCallback(GLFWwindow *Window, int NewWidth, int NewHeight)
{
    if(NewWidth <= 0) NewWidth = 1;
    if(NewHeight <= 0) NewHeight = 1;

    glViewport(0, 0, NewWidth, NewHeight);
}

