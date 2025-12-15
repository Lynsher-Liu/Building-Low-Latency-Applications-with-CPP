#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "log4cplus::log4cplus" for configuration ""
set_property(TARGET log4cplus::log4cplus APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(log4cplus::log4cplus PROPERTIES
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/liblog4cplus.so.3"
  IMPORTED_SONAME_NOCONFIG "liblog4cplus.so.3"
  )

list(APPEND _cmake_import_check_targets log4cplus::log4cplus )
list(APPEND _cmake_import_check_files_for_log4cplus::log4cplus "${_IMPORT_PREFIX}/lib/liblog4cplus.so.3" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
