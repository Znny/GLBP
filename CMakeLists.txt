cmake_minimum_required(VERSION 3.28)
project(GLBP)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)
include_directories(include/glad)
include_directories(include/GLFW)
include_directories(include/glm)
include_directories(include/glm/detail)
include_directories(include/glm/ext)
include_directories(include/glm/gtc)
include_directories(include/glm/gtx)
include_directories(include/glm/simd)
include_directories(include/KHR)

add_executable(GLBP
        src/glad.c
        src/main.cpp
        src/ShaderObject.h)

set(GLFW_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/lib-mingw-w64")

target_link_libraries(GLBP PRIVATE "${GLFW_LIB_DIR}/libglfw3.a")