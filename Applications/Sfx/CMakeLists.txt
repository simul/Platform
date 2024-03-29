cmake_minimum_required(VERSION 3.20)
if(PLATFORM_WINDOWS OR PLATFORM_LINUX)
	file(GLOB CMAKE 	"*.cmake" )
	file(GLOB SOURCES 	Compiler.cpp
						FileLoader.cpp
						Main.cpp
						Sfx.cpp
						SfxEffect.cpp
						SfxProgram.cpp
						ShaderInstance.cpp
						StringFunctions.cpp
						StringToWString.cpp )
	file(GLOB HEADERS 	"*.h" )
	file(GLOB FLEX_BISON "*.lpp" "*.ypp")

	find_package(FLEX REQUIRED)
	find_package(BISON REQUIRED)
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles")
	# If you want to disable line directives in Flex, use -L. For Bison, use -l.
	set(FLEX_FLAGS "")
	set(BISON_FLAGS "" )	# use  for Bison to debug shift-reduce conflicts.
	# specify FULL PATH to Preprocessor.lpp etc, so Flex and Bison will give us clickable warning/error messages.
	flex_target(PreprocessorLexer ${CMAKE_CURRENT_SOURCE_DIR}/Preprocessor.lpp ${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorLexer.cpp DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorLexer.h" COMPILE_FLAGS ${FLEX_FLAGS}  )
	bison_target(PreprocessorParser ${CMAKE_CURRENT_SOURCE_DIR}/Preprocessor.ypp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorParser.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorParser.h" COMPILE_FLAGS ${BISON_FLAGS} )

	flex_target(SfxScanner ${CMAKE_CURRENT_SOURCE_DIR}/Sfx.lpp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxScanner.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxScanner.h"  COMPILE_FLAGS ${FLEX_FLAGS}  )
	bison_target(SfxLALRParser ${CMAKE_CURRENT_SOURCE_DIR}/Sfx.ypp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxLALRParser.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxLALRParser.hpp" COMPILE_FLAGS ${BISON_FLAGS}  )

	ADD_FLEX_BISON_DEPENDENCY(PreprocessorLexer PreprocessorParser)
	ADD_FLEX_BISON_DEPENDENCY(SfxScanner SfxLALRParser)

	set( GeneratedFiles "${FLEX_PreprocessorLexer_OUTPUTS};${FLEX_SfxScanner_OUTPUTS};${BISON_PreprocessorParser_OUTPUTS};${BISON_SfxLALRParser_OUTPUTS}" )

	source_group("Flex Bison" FILES ${FLEX_BISON} )
	source_group("Generated Files" FILES ${GeneratedFiles} )
	add_static_executable(Sfx CONSOLE SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${FLEX_BISON} ${GeneratedFiles} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	target_include_directories(Sfx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles" )
	
	set( SFX_TEST_PLATFORMS "DirectX12;Vulkan" CACHE STRING "Which platform to use when debugging the Sfx executable" )
	#SET_PROPERTY(CACHE SFX_TEST_PLATFORM PROPERTY STRINGS DirectX11 DirectX12 Vulkan OpenGL PS4 GLES Commodore) 

	set(SFX_TEST_FILE "${CMAKE_SOURCE_DIR}/Shaders/Sfx/test.sfx" CACHE FILEPATH "Which file to use when debugging Sfx")
	set(SFX_TEST_DEFINES "-E\"EXAMPLE_DEFINE=example\"" CACHE STRING "")
	cmake_path(CONVERT "${CMAKE_CURRENT_SOURCE_DIR}/../../" TO_CMAKE_PATH_LIST PLATFORM_ROOT NORMALIZE)
	set(TEST_CONFIGS)
	foreach(GRAPHICS_API ${SFX_TEST_PLATFORMS})
		set(NEXT_CONFIG "-P\"${PLATFORM_ROOT}/${GRAPHICS_API}/Sfx/${GRAPHICS_API}.json\"")
		#message("NEXT_CONFIG ${NEXT_CONFIG}")
		set(TEST_CONFIGS "${TEST_CONFIGS} ${NEXT_CONFIG}")
	endforeach()
	#message("TEST_CONFIGS ${TEST_CONFIGS}")
	if(PLATFORM_WINDOWS)
		set_target_properties(Sfx PROPERTIES
			VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nPLATFORM_DIR=${SIMUL_PLATFORM_DIR}"
			VS_DEBUGGER_COMMAND_ARGUMENTS "${SFX_TEST_FILE} -I\"${CMAKE_SOURCE_DIR}/..\" -I\"${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders\" -o\"${CMAKE_BINARY_DIR}/Platform/shaderbin/$PLATFORM_NAME\" ${TEST_CONFIGS} -l -v ${SFX_TEST_DEFINES}"
			)
	endif()
	if(PLATFORM_LINUX)
		target_link_libraries(Sfx c++)
	endif()
endif()
