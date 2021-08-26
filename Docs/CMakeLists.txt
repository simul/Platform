find_package(Doxygen REQUIRED)

# Find all the public headers
get_target_property(CORE_HEADER_DIR Core_MT INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(MATH_HEADER_DIR SimulMath_MT INTERFACE_INCLUDE_DIRECTORIES)
get_target_property(CROSSPLATFORM_HEADER_DIR SimulCrossPlatform_MT INTERFACE_INCLUDE_DIRECTORIES)
file(GLOB_RECURSE PUBLIC_HEADERS ${SIMUL_PLATFORM_DIR}/*.h)
set(DOXYGEN_INPUT_DIR "\"${MATH_HEADER_DIR}\"")
set(DOXYGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/doxygen)
set(DOXYGEN_INDEX_FILE ${DOXYGEN_OUTPUT_DIR}/xml/index.xml)
set(DOXYFILE_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
set(DOXYFILE_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
set(DOXYGEN_SEARCH_PATH ${SIMUL_PLATFORM_DIR}/..)

#Replace variables inside @@ with the current values
configure_file(${DOXYFILE_IN} ${DOXYFILE_OUT} @ONLY)

file(MAKE_DIRECTORY ${DOXYGEN_OUTPUT_DIR}) #Doxygen won't create this for us
add_custom_command(OUTPUT ${DOXYGEN_INDEX_FILE}
					DEPENDS ${PUBLIC_HEADERS} Doxyfile.in
					COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYFILE_OUT}
					MAIN_DEPENDENCY ${DOXYFILE_OUT} ${DOXYFILE_IN}
					COMMENT "Generating docs")

add_custom_target(Doxygen ALL DEPENDS ${DOXYGEN_INDEX_FILE} SOURCES Doxyfile.in)
set_target_properties( Doxygen PROPERTIES FOLDER Docs)

set(CMAKE_MODULE_PATH "${SIMUL_PLATFORM_DIR}/CMake" ${CMAKE_MODULE_PATH})
find_package(Sphinx REQUIRED)

set(SPHINX_SOURCE ${SIMUL_PLATFORM_DIR}/Docs)
set(SPHINX_BUILD ${CMAKE_CURRENT_BINARY_DIR}/sphinx)

file(GLOB RST "*.rst" )
add_custom_target(Sphinx ALL
					COMMAND ${SPHINX_EXECUTABLE} -c ${SIMUL_PLATFORM_DIR}/Docs -b html
					# Tell Breathe where to find the ygen output
					-Dbreathe_projects.Platform=${DOXYGEN_OUTPUT_DIR}/xml
					${SPHINX_SOURCE} ${SPHINX_BUILD}
					WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
					DEPENDS
					# Other docs files you want to track should go here (or in some variable)
					${SIMUL_PLATFORM_DIR}/Docs/index.rst
					${DOXYGEN_INDEX_FILE}
					MAIN_DEPENDENCY ${SPHINX_SOURCE}/conf.py
					COMMENT "Generating documentation with Sphinx"
					SOURCES ${RST})
set_target_properties( Sphinx PROPERTIES FOLDER Docs)
