if(PLATFORM_SUPPORT_ANGLE)

set(ANGLE_BUILD_OUTPUT_DIR "C:/Simul/Angle/out/Debug" CACHE STRING "Set the location of the Angle directory.")
set(src_dir "${ANGLE_BUILD_OUTPUT_DIR}")
set(dst_dir "${SIMUL_PLATFORM_DIR}/Angle/bin")

#message(STATUS "${src_dir}")
#message(STATUS "${dst_dir}")

file(GLOB ANGLR_LIB_FILES
  "${src_dir}/*.dll"
  "${src_dir}/*.lib"
  "${src_dir}/*.pdb"
  "${src_dir}/*.so"
  "${src_dir}/*.a"
)

file(COPY ${ANGLR_LIB_FILES} DESTINATION ${dst_dir})

endif()