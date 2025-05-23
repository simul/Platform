cmake_minimum_required(VERSION 3.12)

file(GLOB SOURCES "*.cpp" )

file(GLOB HEADERS "*.h" )
file(GLOB SHADERS  "Shaders/*.sfx") 
file(GLOB SHADER_INCLUDES "Shaders/*.sl") 

if (PLATFORM_SWITCH)
	set(EXCLUDE_ITEM "${CMAKE_CURRENT_SOURCE_DIR}/Shaders/texture_compression.sfx")
	list(REMOVE_ITEM SHADERS ${EXCLUDE_ITEM})
endif()

if(PLATFORM_LOAD_RENDERDOC)
	add_definitions(-DPLATFORM_LOAD_RENDERDOC=1)
endif()
if(PLATFORM_LOAD_WINPIXGPUCAPTURER)
	add_definitions(-DPLATFORM_LOAD_WINPIXGPUCAPTURER=1)
endif()

if(PLATFORM_USE_ASSIMP)
	set(INCLUDE_DIRS "${SIMUL_PLATFORM_DIR}/External/assimp/include" "${CMAKE_BINARY_DIR}/External/assimp/include")
	set(ADDITIONAL_LIBS "")
else()
	set(INCLUDE_DIRS "")
	set(ADDITIONAL_LIBS "")
endif()

if(PLATFORM_WINDOWS OR PLATFORM_LINUX)
	#set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} Sfx)
endif()
set(INCLUDE_DIRS ${INCLUDE_DIRS} "${SIMUL_PLATFORM_DIR}/External/stb")
 
set(PLATFORM_STD_CHARCONV ON)
if(PLATFORM_LINUX)
	set(PLATFORM_STD_CHARCONV OFF)
elseif(PLATFORM_SWITCH)
	set(PLATFORM_STD_CHARCONV OFF)
elseif(PLATFORM_EMSCRIPTEN)
	set(PLATFORM_STD_CHARCONV OFF)
endif()

if(PLATFORM_PS4)
	set(PLATFORM_STD_FILESYSTEM 0)
elseif(PLATFORM_SWITCH)
	set(PLATFORM_STD_FILESYSTEM 0)
endif()

# whether to do the (slow) debugging of profiling levels - for finding mismatched Begin()/End() pairs.
option(PLATFORM_DEBUG_PROFILING_LEVELS "Whether to do the (slow) debugging of profiling levels - for finding mismatched Begin()/End() pairs." OFF)
option(PLATFORM_IMPLEMENT_STB_IMAGE "Whether to include stb_image and stb_image_write code in CrossPlatform/RenderPlatform.cpp. Disable if included elsewhere to avoid linker errors." ON)

#set(SIMUL_TRUESKY TRUE)
#if (NOT CMAKE_PROJECT_NAME STREQUAL "Simul")
	set(SIMUL_TRUESKY FALSE)
#endif()

# Setup and build the static library.
add_static_library( SimulCrossPlatform SOURCES ${SOURCES} ${HEADERS} ${SHADERS} ${SHADER_INCLUDES}
	DEFINITIONS PLATFORM_LOAD_RENDERDOC=$<BOOL:${PLATFORM_LOAD_RENDERDOC}> PLATFORM_LOAD_WINPIXGPUCAPTURER=$<BOOL:${PLATFORM_LOAD_WINPIXGPUCAPTURER}> PLATFORM_USE_ASSIMP=$<BOOL:${PLATFORM_USE_ASSIMP}> PLATFORM_STD_CHARCONV=$<BOOL:${PLATFORM_STD_CHARCONV}> PLATFORM_DEBUG_PROFILING_LEVELS=$<BOOL:${PLATFORM_DEBUG_PROFILING_LEVELS}> PLATFORM_IMPLEMENT_STB_IMAGE=$<BOOL:${PLATFORM_IMPLEMENT_STB_IMAGE}>
	FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX} DEPENDENCIES ${ADDITIONAL_LIBS} INCLUDES ${INCLUDE_DIRS} )

add_multiplatform_sfx_shader_project( SimulCrossPlatform${STATIC_LINK_SUFFIX} INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/Shaders"
								"${SIMUL_PLATFORM_DIR}/.."
							SOURCES ${SHADERS} ${SHADER_INCLUDES} DEFINES VULKAN_SDK=${VULKAN_SDK_DIR} CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} 
							OPTIONS -GSIMUL_TRUESKY=$<BOOL:${SIMUL_TRUESKY}>
					 		OUTPUT "${CMAKE_BINARY_DIR}/shaderbin/$PLATFORM_NAME" )

# For parallel_hashmap
if(SIMUL_SOURCE_BUILD)
	target_include_directories(SimulCrossPlatform${STATIC_LINK_SUFFIX} PUBLIC "${SIMUL_PLATFORM_DIR}/External")
else()
	target_include_directories(SimulCrossPlatform${STATIC_LINK_SUFFIX} INTERFACE "${SIMUL_PLATFORM_DIR}/External")
endif()

if (SIMUL_DLL_SDK_LIBRARY)
	AddDLLCompileDefinitions( SimulCrossPlatform${STATIC_LINK_SUFFIX} )
endif()

if(PLATFORM_USE_ASSIMP)
	add_compile_definitions(SimulCrossPlatform${STATIC_LINK_SUFFIX} PRIVATE ASSIMP_LIBNAME="${ASSIMP_LIBNAME}.dll")
endif()
