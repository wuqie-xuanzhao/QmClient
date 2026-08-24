if(NOT METAL_COMPILER OR NOT METALLIB_COMPILER)
  message(FATAL_ERROR "Metal shader tools were not discovered before including BuildMetalShaders.cmake")
endif()

set(METAL_SHADER_SOURCE "${PROJECT_SOURCE_DIR}/data/shader/metal/qmclient.metal")
set(METAL_SHADER_OUTPUT_DIR "${PROJECT_BINARY_DIR}/data/shader/metal")
set(METAL_SHADER_AIR "${METAL_SHADER_OUTPUT_DIR}/qmclient.air")
set(METAL_SHADER_LIBRARY "${METAL_SHADER_OUTPUT_DIR}/qmclient.metallib")

add_custom_command(
  OUTPUT "${METAL_SHADER_AIR}"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${METAL_SHADER_OUTPUT_DIR}"
  COMMAND "${XCRUN_PROGRAM}" --sdk macosx metal -c "${METAL_SHADER_SOURCE}" -I "${PROJECT_SOURCE_DIR}/src/engine/client/backend/metal" -o "${METAL_SHADER_AIR}"
  DEPENDS
    "${METAL_SHADER_SOURCE}"
    "${PROJECT_SOURCE_DIR}/src/engine/client/backend/metal/metal_types.h"
  COMMENT "Compiling Metal shader qmclient.metal"
  VERBATIM
)

add_custom_command(
  OUTPUT "${METAL_SHADER_LIBRARY}"
  COMMAND "${XCRUN_PROGRAM}" --sdk macosx metallib "${METAL_SHADER_AIR}" -o "${METAL_SHADER_LIBRARY}"
  DEPENDS "${METAL_SHADER_AIR}"
  COMMENT "Linking Metal library qmclient.metallib"
  VERBATIM
)

set(METAL_SHADER_FILE_LIST "data/shader/metal/qmclient.metallib" CACHE STRING "Metal shader file list" FORCE)
add_custom_target(build_metal_shaders DEPENDS "${METAL_SHADER_LIBRARY}")
