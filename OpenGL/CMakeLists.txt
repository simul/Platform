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

file(GLOB SHADERS
	"${CMAKE_SOURCE_DIR}/Platform/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${CMAKE_SOURCE_DIR}/Platform/Shaders/SL/*.sl"
)

add_static_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${CMAKE})
if(SIMUL_SOURCE_BUILD)
	target_link_directories( SimulOpenGL_MT PUBLIC ${CMAKE_SOURCE_DIR}/Platform/External/glfw/build_mt/lib/${CMAKE_BUILD_TYPE})
	set_target_properties(SimulOpenGL_MT PROPERTIES FOLDER Static)
	target_include_directories(SimulOpenGL_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/stb")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_SOURCE_DIR}/Platform/External/glfw/include")

	set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/GLSL/GLSL.json" )
endif()

add_dynamic_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${JSON_FILE})

if(SIMUL_SOURCE_BUILD)
	add_sfx_shader_project( OpenGLCoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/GLSL" "${CMAKE_SOURCE_DIR}/Platform/Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${JSON} ${CMAKE} ${in_rs} OPTIONS -w OUTPUT "${CMAKE_BINARY_DIR}/Platform/OpenGL/shaderbin" INTERMEDIATE "${CMAKE_BINARY_DIR}/Platform/OpenGL/sfx_intermediate" )
 
	target_link_directories( SimulOpenGL_MD PUBLIC ${CMAKE_SOURCE_DIR}/Platform/External/glfw/build_md/lib/${CMAKE_BUILD_TYPE})
	target_compile_definitions(SimulOpenGL_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_OPENGL_DLL=1)
	set_target_properties(SimulOpenGL_MD PROPERTIES FOLDER Dynamic)
	target_include_directories(SimulOpenGL_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/External/stb")
	target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/OpenGL")
	target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_SOURCE_DIR}/Platform/External/glfw/include")

	target_link_libraries(SimulOpenGL_MD SimulCrossPlatform_MD SimulGeometry_MD SimulMath_MD SimulBase_MD glfw3)
endif()