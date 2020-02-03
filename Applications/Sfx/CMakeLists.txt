cmake_minimum_required(VERSION 2.8)
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
add_dynamic_executable(Sfx ${SOURCES} ${HEADERS} ${CMAKE} ${FLEX_BISON} ${GeneratedFiles} )
target_include_directories(Sfx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/GeneratedFiles" )

if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set_target_properties(Sfx PROPERTIES FOLDER "Dynamic/Applications" )
	deploy_to_directory(Sfx ${CMAKE_SOURCE_DIR}\\Tools\\bin)
	set_target_properties(Sfx PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
											   VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}")
endif()