cmake_minimum_required(VERSION 2.8)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_library(SimulOpenGL_MT ${SOURCES} ${HEADERS})
target_include_directories(SimulOpenGL_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")

add_definitions(-DSIMUL_DYNAMIC_LINK -DSIMUL_OPENGL_DLL)
add_library(SimulOpenGL_MD SHARED ${SOURCES} ${HEADERS})
target_include_directories(SimulOpenGL_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")


target_link_libraries(SimulOpenGL_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD)

