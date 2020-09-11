cmake_minimum_required(VERSION 2.8)

file(GLOB CMAKE "*.cmake" )

file(GLOB SOURCES Sample.cpp  )
file(GLOB HEADERS "*.h" )
 
set(SAMPLE_API SAMPLE_USE_D3D12)

add_static_executable(Sample_MT WIN32 SOURCES ${SOURCES} ${HEADERS} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
set_target_properties(Sample_MT PROPERTIES FOLDER Static/Applications)
#target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
target_compile_definitions(Sample_MT PRIVATE DO_PROFILING=0 )
set_target_properties(Sample_MT PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries(Sample_MT SimulCrossPlatform_MT SimulMath_MT Core_MT directxtex_MT )
if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D11")
	target_include_directories(Sample_MT PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
	target_link_libraries(Sample_MT SimulDirectX11_MT effects11_MT)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Sample_MT DirectX11CoreShaders )
	endif()
endif()

if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D12")
	target_link_libraries(Sample_MT SimulDirectX12_MT)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Sample_MT DirectX12CoreShaders )
	endif()
endif()


add_dynamic_executable(Sample_MD WIN32 SOURCES ${SOURCES} ${HEADERS} ${CMAKE} DEFINITIONS ${SAMPLE_API} FOLDER ${SIMUL_PLATFORM_FOLDER_PREFIX})
target_compile_definitions(Sample_MD PRIVATE SIMUL_DYNAMIC_LINK=1 DO_PROFILING=0)

target_include_directories(Sample_MD PRIVATE "${CMAKE_SOURCE_DIR}/Platform/DirectX11")
#target_include_directories(Sample_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/Effects11_Win8SDK/inc" )
target_include_directories(Sample_MD PRIVATE "${CMAKE_SOURCE_DIR}/External/DirectX/DirectXTex/DirectXTex" )
set_target_properties(Sample_MD PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
target_link_libraries(Sample_MD SimulCrossPlatform_MD SimulMath_MD Core_MD)

if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D11")
	target_link_libraries(Sample_MD SimulDirectX11_MD)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Sample_MD DirectX11CoreShaders )
	endif()
endif()

if(${SAMPLE_API} MATCHES "SAMPLE_USE_D3D12")
	target_link_libraries(Sample_MD SimulDirectX12_MD)
	if(SIMUL_BUILD_SHADERS)
		add_dependencies( Sample_MD DirectX12CoreShaders )
	endif()
endif()
