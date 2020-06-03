cmake_minimum_required(VERSION 2.8)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	file(GLOB CMAKE 	"*.cmake" )
	file(GLOB SOURCES 	Compiler.cpp
						FileLoader.cpp
						Main.cpp
						Sfx.cpp
						SfxEffect.cpp
						SfxProgram.cpp
						StringFunctions.cpp
						StringToWString.cpp )
	file(GLOB HEADERS 	"*.h" )
	file(GLOB FLEX_BISON "*.lpp" "*.ypp")

	find_package(FLEX REQUIRED)
	find_package(BISON REQUIRED)
	file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles")
	# If you want to disable line directives in Flex, use -L. For Bison, use -l.
	set(FLEX_FLAGS "")
	set(BISON_FLAGS "")
	flex_target(PreprocessorLexer Preprocessor.lpp ${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorLexer.cpp DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorLexer.h" COMPILE_FLAGS ${FLEX_FLAGS}  )
	bison_target(PreprocessorParser Preprocessor.ypp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorParser.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/PreprocessorParser.h" COMPILE_FLAGS ${BISON_FLAGS}  )

	flex_target(SfxScanner Sfx.lpp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxScanner.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxScanner.h"  COMPILE_FLAGS ${FLEX_FLAGS}  )
	bison_target(SfxLALRParser Sfx.ypp "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxLALRParser.cpp" DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles/SfxLALRParser.hpp" COMPILE_FLAGS ${BISON_FLAGS}  )

	ADD_FLEX_BISON_DEPENDENCY(PreprocessorLexer PreprocessorParser)
	ADD_FLEX_BISON_DEPENDENCY(SfxScanner SfxLALRParser)

	set( GeneratedFiles "${FLEX_PreprocessorLexer_OUTPUTS};${FLEX_SfxScanner_OUTPUTS};${BISON_PreprocessorParser_OUTPUTS};${BISON_SfxLALRParser_OUTPUTS}" )
	#message(STATUS " GeneratedFiles=${GeneratedFiles}" ) 
	source_group("Flex Bison" FILES ${FLEX_BISON} )
	source_group("Generated Files" FILES ${GeneratedFiles} )
	add_dynamic_executable(Sfx CONSOLE SOURCES ${SOURCES} ${HEADERS} ${CMAKE} ${FLEX_BISON} ${GeneratedFiles} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
	target_include_directories(Sfx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles" )

	#set_target_properties(Sfx PROPERTIES CMAKE_VS_PLATFORM_TOOLSET v142 )
	#set_target_properties(Sfx PROPERTIES CMAKE_GENERATOR_TOOLSET v142 )
	deploy_to_directory(Sfx ${CMAKE_SOURCE_DIR}\\Tools\\bin)
	set(SFX_TEST_PLATFORM PS4/Render)
	set_target_properties(Sfx PROPERTIES
	#VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../Platform/DirectX12/shaderbin"
	#VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/../../Platform/PS4/shaderbin"
		VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}"											  
		#VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_SOURCE_DIR}/Shaders/SFX/test.sfx -I\"${SIMUL_PLATFORM_DIR}/DirectX11/HLSL\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${SIMUL_PLATFORM_DIR}/Shaders/SL\" -O\"${CMAKE_CURRENT_BINARY_DIR}/../../DirectX11/shaderbin\" -P\"${SIMUL_PLATFORM_DIR}/DirectX11/HLSL/HLSL11.json\" -E\"PLATFORM=${SIMUL_PLATFORM_DIR}\" -M\"${CMAKE_CURRENT_BINARY_DIR}/../../DirectX11/sfx_intermediate\" -w -f -v"
		VS_DEBUGGER_COMMAND_ARGUMENTS "${SIMUL_DIR}/Shaders/SFX/test.sfx -I\"${SIMUL_PLATFORM_DIR}/Vulkan/GLSL\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${SIMUL_PLATFORM_DIR}/Shaders/SL\" -O\"${CMAKE_CURRENT_BINARY_DIR}/../../Vulkan/shaderbin\" -P\"${SIMUL_PLATFORM_DIR}/Vulkan/GLSL/GLSL.json\" -M\"${CMAKE_CURRENT_BINARY_DIR}/../../Vulkan/sfx_intermediate\" -w -f -v -d -l"

		#VS_DEBUGGER_COMMAND_ARGUMENTS "${CMAKE_SOURCE_DIR}/Shaders/SFX/test.sfx -I\"${SIMUL_PLATFORM_DIR}/DirectX12/HLSL\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${SIMUL_PLATFORM_DIR}/Shaders/SL\" -O\"${CMAKE_CURRENT_BINARY_DIR}/../../DirectX12/shaderbin\" -P\"${SIMUL_PLATFORM_DIR}/DirectX12/HLSL/HLSL12.json\" -E\"PLATFORM=${SIMUL_PLATFORM_DIR}\" -M\"${CMAKE_CURRENT_BINARY_DIR}/../../DirectX12/sfx_intermediate\" -w -f -v -d"
		
		#VS_DEBUGGER_COMMAND_ARGUMENTS "${SIMUL_DIR}/Shaders/SFX/test.sfx -I\"${SIMUL_PLATFORM_DIR}/OpenGL/GLSL\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${SIMUL_PLATFORM_DIR}/Shaders/SL\" -O\"${CMAKE_CURRENT_BINARY_DIR}/../../OpenGL/shaderbin\" -P\"${SIMUL_PLATFORM_DIR}/OpenGL/GLSL/GLSL.json\" -M\"${CMAKE_CURRENT_BINARY_DIR}/../../OpenGL/sfx_intermediate\" -w -f -v -d"
		
		#VS_DEBUGGER_COMMAND_ARGUMENTS "${SIMUL_PLATFORM_DIR}/Shaders/Sfx/test.sfx -I\"${CMAKE_CURRENT_SOURCE_DIR}/../../${SFX_TEST_PLATFORM}/PSSL\" -I\"${CMAKE_SOURCE_DIR}/Shaders/SL\" -I\"${CMAKE_CURRENT_SOURCE_DIR}/../../Shaders/SL\" -O\"${CMAKE_CURRENT_BINARY_DIR}/../../PS4/shaderbin\" -P\"${CMAKE_CURRENT_SOURCE_DIR}/../../${SFX_TEST_PLATFORM}/PSSL/PSSL.json\" -M\"${CMAKE_CURRENT_BINARY_DIR}/../../${SFX_TEST_PLATFORM}/sfx_intermediate\" -w -f -l -v -E\"SCE_ORBIS=C:\\\""
	)

endif()
