cmake_minimum_required(VERSION 3.10)

file(GLOB SOURCES "*.cpp"  )
file(GLOB HEADERS "*.h" )
file(GLOB shaders "Shaders/*.sfx" "Shaders/*.sl")

source_group("Shaders" src FILES ${shaders} )

add_static_executable(00_HelloWorld_MT WIN32 SOURCES ${SOURCES} ${HEADERS} ${shaders} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})