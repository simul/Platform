cmake_minimum_required(VERSION 2.9)
if(PLATFORM_WINDOWS)
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
	add_dynamic_executable(Sfx CONSOLE SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${FLEX_BISON} ${GeneratedFiles} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	target_include_directories(Sfx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles" )
	
	set( SFX_TEST_PLATFORM "DirectX11" CACHE STRING "Which platform to use when debugging the Sfx executable" )
	SET_PROPERTY(CACHE SFX_TEST_PLATFORM PROPERTY STRINGS DirectX11 DirectX12 Vulkan OpenGL PS4 GLES Commodore) 

	set(SFX_TEST_FILE "${CMAKE_SOURCE_DIR}/Shaders/Sfx/test.sfx" CACHE FILEPATH "Which file to use when debugging Sfx")
	set(SFX_TEST_DEFINES "-E\"EXAMPLE_DEFINE=example\"" CACHE STRING "")
	file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../" PLATFORM_ROOT )
	#message("PLATFORM_ROOT ${PLATFORM_ROOT}")
	if(PLATFORM_WINDOWS)
	set_target_properties(Sfx PROPERTIES
		VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}"
		VS_DEBUGGER_COMMAND_ARGUMENTS "${SFX_TEST_FILE} -b\"${CMAKE_BINARY_DIR}\" -I\"${PLATFORM_ROOT}/${SFX_TEST_PLATFORM}/Sfx\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${PLATFORM_ROOT}/Shaders/SL\" -O\"${PLATFORM_ROOT}/${SFX_TEST_PLATFORM}/shaderbin\" -P\"${PLATFORM_ROOT}/${SFX_TEST_PLATFORM}/Sfx/${SFX_TEST_PLATFORM}.json\" -M\"${PLATFORM_ROOT}/${SFX_TEST_PLATFORM}/sfx_intermediate\" -w -f -l -v ${SFX_TEST_DEFINES}"
	)
	endif()
	if(PLATFORM_LINUX)
		target_link_libraries(Sfx c++)
	endif()
endif()
