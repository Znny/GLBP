cmake_minimum_required(VERSION 3.20)
project(GLBP)

add_subdirectory(extern/myc)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)
include_directories(include/glad)
include_directories(include/glm)
include_directories(include/glm/detail)
include_directories(include/glm/ext)
include_directories(include/glm/gtc)
include_directories(include/glm/gtx)
include_directories(include/glm/simd)
include_directories(include/KHR)
include_directories(extern/myc)

# GLFW headers must match whichever GLFW is actually linked below, or ABI-versioned
# functions (e.g. glfwGetPlatform(), added in 3.4, used by ImGui's GLFW backend) can
# get compiled in against headers newer than the linked library, causing "undefined
# reference" link errors. The vendored GLFW headers live alongside the vendored
# mingw-w64 static lib they pair with (lib/lib-mingw-w64/include), not under the
# generic include/ root, so they can't leak onto the include path on other platforms.
# Elsewhere, find_package(glfw3) supplies headers and lib together so they can never
# drift apart from each other.
if(WIN32)
    include_directories(lib/lib-mingw-w64/include)
else()
    find_package(glfw3 REQUIRED)
endif()

add_library(imgui STATIC
        extern/imgui/imgui.cpp
        extern/imgui/imgui_draw.cpp
        extern/imgui/imgui_tables.cpp
        extern/imgui/imgui_widgets.cpp
        extern/imgui/imgui_demo.cpp
        extern/imgui/backends/imgui_impl_glfw.cpp
        extern/imgui/backends/imgui_impl_opengl3.cpp)
target_include_directories(imgui PUBLIC extern/imgui extern/imgui/backends)
if(NOT WIN32)
    # imgui_impl_glfw.cpp needs <GLFW/glfw3.h>; on WIN32 that comes from the
    # lib/lib-mingw-w64/include include_directories() above, but elsewhere it only
    # comes from the glfw target's own usage requirements
    target_link_libraries(imgui PUBLIC glfw)
endif()

add_executable(GLBP
        src/glad.c
        src/main.cpp
        src/ConfigManager.cpp
        src/VertexBuffer.cpp
        src/IndexBuffer.cpp
        src/VertexArray.cpp
        src/ShaderObject.cpp
        src/ShaderProgram.cpp
        src/ShaderManager.cpp
        src/Transform.cpp
        src/Camera.cpp
        src/Gizmo.cpp
        src/SSTextRenderer.cpp
        src/stb_truetype_impl.cpp)

set(GLFW_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/lib-mingw-w64")

target_link_libraries(GLBP PRIVATE myc_paths)
target_link_libraries(GLBP PRIVATE myc_logging)
target_link_libraries(GLBP PRIVATE imgui)

if(WIN32)
    target_link_libraries(GLBP PRIVATE
            "${CMAKE_SOURCE_DIR}/lib/lib-mingw-w64/libglfw3.a"
            -static)
else()
    target_link_libraries(GLBP PRIVATE glfw GL dl)
endif()

# Add post-build command which copies resource files into the build directory the target was placed in
add_custom_command(TARGET GLBP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resource
        $<TARGET_FILE_DIR:GLBP>/resource)

# Add post-build command which copies config files into the build directory the target was placed in
add_custom_command(TARGET GLBP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/config
        $<TARGET_FILE_DIR:GLBP>/config)