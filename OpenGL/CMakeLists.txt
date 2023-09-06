cmake_minimum_required(VERSION 3.5)

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
	"${SIMUL_PLATFORM_DIR}/OpenGL/Sfx/*.sl"
)
file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/OpenGL/Sfx/*.json" 
	)

add_static_library(SimulOpenGL SOURCES ${SOURCES} ${HEADERS} ${SHADER_DEFS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
if(SIMUL_SOURCE_BUILD)
	add_dependencies( SimulOpenGL_MT SimulCrossPlatform_MT )
	target_include_directories(SimulOpenGL_MT PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../External/stb")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../OpenGL")
	target_include_directories(SimulOpenGL_MT PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include")

	set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/OpenGL.json" )
endif()
