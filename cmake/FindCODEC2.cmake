# - Try to find CODEC2
# Once done this will define

#Seems to work on Cygwin as well now -- more testing for may be required
# Not sure what causes the 'Package 'codec2', required by 'virtual:world', not found' message, but seems to work anyways
# -- Checking for module 'codec2'
# --   Package 'codec2', required by 'virtual:world', not found
# -- Found CODEC2: /usr/local/lib/libcodec2.dll.a

if (NOT CODEC2_FOUND)

  find_package(PkgConfig)
  pkg_check_modules(CODEC2_PKG codec2)
  set(CODEC2_DEFINITIONS ${PC_CODEC2_CFLAGS_OTHER})

  find_path(CODEC2_INCLUDE_DIR
    NAMES codec2/codec2.h codec2.h
    HINTS ${CODEC2_PKG_INCLUDE_DIRS})

  find_library(CODEC2_LIBRARY
    NAMES codec2/codec2 codec2
    HINTS ${CODEC2_PKG_LIBRARY_DIRS})

  set(CODEC2_LIBRARIES ${CODEC2_LIBRARY})
  set(CODEC2_INCLUDE_DIRS ${CODEC2_INCLUDE_DIR})

  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(CODEC2 DEFAULT_MSG CODEC2_LIBRARY CODEC2_INCLUDE_DIR)
  mark_as_advanced(CODEC2_INCLUDE_DIR CODEC2_LIBRARY)

endif (NOT CODEC2_FOUND)