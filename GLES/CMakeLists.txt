cmake_minimum_required(VERSION 2.9)

file(GLOB CMAKE
    "*.cmake"
)

file(GLOB SOURCES
	"*.cpp"
)

if(NOT PLATFORM_COMPILE_DEVICE_MANAGER)
list(FILTER SOURCES EXCLUDE REGEX ".*DeviceManager.cpp$")
list(FILTER SOURCES EXCLUDE REGEX ".*DisplaySurface.cpp$")
endif()

list(REMOVE_ITEM lib_srcs "IlmImf/b44ExpLogTable.cpp")
file(GLOB HEADERS
    "*.h"
)

file(GLOB SHADERS
	"${SIMUL_PLATFORM_DIR}/Shaders/Sfx/*.sfx" 
)

file(GLOB SHADER_INCLUDES
	"${SIMUL_PLATFORM_DIR}/Shaders/SL/*.sl"
	"${SIMUL_PLATFORM_DIR}/GLES/GLSL/*.sl"
)
file(GLOB SHADER_DEFS
	"${SIMUL_PLATFORM_DIR}/GLES/Sfx/*.*" 
	)

if(SIMUL_SOURCE_BUILD)
	add_static_library(SimulGLES SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} DEFINITIONS GL_GLEXT_PROTOTYPES INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/../External/stb" "${PLATFORM_ANGLE_DIR}" "${PLATFORM_ANGLE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include" LINK_DIRS ${PLATFORM_ANGLE_LIB_DIR}${PLATFORM_ANGLE_LIB_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/build_mt/lib/${CMAKE_BUILD_TYPE})

	set( JSON_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Sfx/GLES.json" )
	add_dynamic_library(SimulGLES SOURCES ${SOURCES} ${HEADERS} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} LINK_DIRS ${PLATFORM_ANGLE_LIB_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/build_md/lib/${CMAKE_BUILD_TYPE} INCLUDES "${PLATFORM_ANGLE_DIR}" "${PLATFORM_ANGLE_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include" LINK SimulGLES_MD SimulCrossPlatform_MD SimulMath_MD Core_MD libGLESv2.dll.lib libEGL.dll.lib DEFINITIONS SIMUL_DYNAMIC_LINK=1 SIMUL_GLES_DLL=1 GL_GLEXT_PROTOTYPES)
	add_sfx_shader_project( GLESCoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Sfx" "${CMAKE_CURRENT_SOURCE_DIR}/../Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${SHADER_DEFS} ${in_rs} OPTIONS -w -l OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../GLES/shaderbin" INTERMEDIATE "${CMAKE_CURRENT_BINARY_DIR}/../GLES/sfx_intermediate"  FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders)

	add_dynamic_library(SimulGLES SOURCES ${SOURCES} ${HEADERS} ${JSON_FILE} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})

	set_target_properties(SimulGLES_MT PROPERTIES LINK_FLAGS "-s USE_WEBGL2=1 -s FULL_ES3=1 -s USE_GLFW=3 -s WASM=1")

	if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
		add_sfx_shader_project( GLESCoreShaders ${JSON_FILE} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/GLSL" "${CMAKE_CURRENT_SOURCE_DIR}/../Shaders/SL" SOURCES ${SHADERS} ${SHADER_INCLUDES} ${SHADER_DEFS} ${in_rs} OPTIONS -w -l OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/../GLES/shaderbin" INTERMEDIATE "${CMAKE_CURRENT_BINARY_DIR}/../GLES/sfx_intermediate"  FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX}/Shaders)
	 
		target_link_directories( SimulGLES_MD PUBLIC ${PLATFORM_ANGLE_LIB_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/build_md/lib/${CMAKE_BUILD_TYPE})
		target_compile_definitions(SimulGLES_MD PRIVATE SIMUL_DYNAMIC_LINK=1 SIMUL_GLES_DLL=1)
		target_include_directories(SimulGLES_MD PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../External/stb")
		target_include_directories(SimulGLES_MD PUBLIC "${PLATFORM_ANGLE_DIR}" "${PLATFORM_ANGLE_DIR}/include")
		target_include_directories(SimulGLES_MD PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../External/glfw/include")

		target_link_libraries(SimulGLES_MD SimulCrossPlatform_MD SimulMath_MD Core_MD glfw3 libGLESv2.dll)
	endif()
endif()