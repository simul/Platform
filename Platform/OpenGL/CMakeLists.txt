cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
	"*.cpp" "glad.c"
)

file(GLOB HEADERS
    "*.h"
)
add_static_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${CMAKE})
set_target_properties(SimulOpenGL_MT PROPERTIES FOLDER Static)
target_include_directories(SimulOpenGL_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")
target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_SOURCE_DIR}/External/glfw/include")

add_dynamic_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${CMAKE})
target_compile_definitions(SimulOpenGL_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_OPENGL_DLL=1)
set_target_properties(SimulOpenGL_MD PROPERTIES FOLDER Dynamic)
target_include_directories(SimulOpenGL_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/stb")
target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")
target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_SOURCE_DIR}/External/glfw/include")

target_link_libraries(SimulOpenGL_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD glfw)

