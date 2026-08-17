cmake_minimum_required(VERSION 3.20)
project(GLBP)

add_subdirectory(extern/myc)

set(CMAKE_CXX_STANDARD 17)

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
include_directories(extern/myc)

add_executable(GLBP
        src/glad.c
        src/main.cpp
        src/ShaderObject.cpp
        src/ShaderProgram.cpp
        src/ShaderManager.cpp)

set(GLFW_LIB_DIR "${CMAKE_SOURCE_DIR}/lib/lib-mingw-w64")

target_link_libraries(GLBP PRIVATE myc_paths)
target_link_libraries(GLBP PRIVATE myc_logging)

if(WIN32)
    target_link_libraries(GLBP PRIVATE
            "${CMAKE_SOURCE_DIR}/lib/lib-mingw-w64/libglfw3.a"
            -static)
else()
    find_package(glfw3 REQUIRED)
    target_link_libraries(GLBP PRIVATE glfw GL dl)
endif()

# Add post-build command which copies resource files into the build directory the target was placed in
add_custom_command(TARGET GLBP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resource
        $<TARGET_FILE_DIR:GLBP>/resource)