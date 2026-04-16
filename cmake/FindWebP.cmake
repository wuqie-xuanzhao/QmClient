if(NOT PREFER_BUNDLED_LIBS)
  set(CMAKE_MODULE_PATH ${ORIGINAL_CMAKE_MODULE_PATH})
  find_package(WebP)
  set(CMAKE_MODULE_PATH ${OWN_CMAKE_MODULE_PATH})
  if(WebP_FOUND)
    set(WebP_BUNDLED OFF)
    set(WebP_DEP)
  endif()
endif()

if(NOT WebP_FOUND)
  set_extra_dirs_lib(WebP webp)
  find_library(WebP_LIBRARY
    NAMES webp libwebp
    HINTS ${HINTS_WebP_LIBDIR} ${PC_WebP_LIBDIR} ${PC_WebP_LIBRARY_DIRS}
    PATHS ${PATHS_WebP_LIBDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  set_extra_dirs_include(WebP webp "${WebP_LIBRARY}")
  find_path(WebP_INCLUDEDIR
    NAMES webp/decode.h
    HINTS ${HINTS_WebP_INCLUDEDIR} ${PC_WebP_INCLUDEDIR} ${PC_WebP_INCLUDE_DIRS}
    PATHS ${PATHS_WebP_INCLUDEDIR}
    ${CROSSCOMPILING_NO_CMAKE_SYSTEM_PATH}
  )

  mark_as_advanced(WebP_LIBRARY WebP_INCLUDEDIR)

  if(WebP_LIBRARY AND WebP_INCLUDEDIR)
    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(WebP DEFAULT_MSG WebP_LIBRARY WebP_INCLUDEDIR)

    set(WebP_LIBRARIES ${WebP_LIBRARY})
    set(WebP_INCLUDE_DIRS ${WebP_INCLUDEDIR})
  endif()
endif()

set(WebP_COPY_FILES)
# WebP on Windows uses static library, no DLL to copy
