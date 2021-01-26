cmake_minimum_required(VERSION 2.9)

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
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
	"${SIMUL_PLATFORM_DIR}/OpenGL/GLSL/*.sl"
)
file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/Vulkan/GLSL/*.*" 
	)

add_static_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
if(SIMUL_SOURCE_BUILD)
	target_link_directories( SimulOpenGL_MT PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/build_mt/lib/${CMAKE_BUILD_TYPE})
	target_include_directories(SimulOpenGL_MT PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../External/stb")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../OpenGL")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include")

	set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/GLSL/GLSL.json" )
endif()

add_dynamic_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${JSON_FILE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

if(SIMUL_SOURCE_BUILD)
	add_sfx_shader_project( OpenGLCoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/GLSL" "${CMAKE_CURRENT_SOURCE_DIR}/../Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${SHADER_DEFS} ${in_rs} OPTIONS -w -l OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../OpenGL/shaderbin" INTERMEDIATE "${CMAKE_CURRENT_BINARY_DIR}/../OpenGL/sfx_intermediate"  FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders)
 
	target_link_directories( SimulOpenGL_MD PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/build_md/lib/${CMAKE_BUILD_TYPE})
	target_compile_definitions(SimulOpenGL_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_OPENGL_DLL=1)
	target_include_directories(SimulOpenGL_MD PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../External/stb")
	target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../OpenGL")
	target_include_directories(SimulOpenGL_MD PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include")

	target_link_libraries(SimulOpenGL_MD SimulCrossPlatform_MD SimulMath_MD Core_MD glfw3)
endif()