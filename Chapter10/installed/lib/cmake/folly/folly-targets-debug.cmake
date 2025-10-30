#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Folly::folly" for configuration "Debug"
set_property(TARGET Folly::folly APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Folly::folly PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libfolly.so.0.58.0-dev"
  IMPORTED_SONAME_DEBUG "libfolly.so.0.58.0-dev"
  )

list(APPEND _IMPORT_CHECK_TARGETS Folly::folly )
list(APPEND _IMPORT_CHECK_FILES_FOR_Folly::folly "${_IMPORT_PREFIX}/lib/libfolly.so.0.58.0-dev" )

# Import target "Folly::folly_test_util" for configuration "Debug"
set_property(TARGET Folly::folly_test_util APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Folly::folly_test_util PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libfolly_test_util.so.0.58.0-dev"
  IMPORTED_SONAME_DEBUG "libfolly_test_util.so.0.58.0-dev"
  )

list(APPEND _IMPORT_CHECK_TARGETS Folly::folly_test_util )
list(APPEND _IMPORT_CHECK_FILES_FOR_Folly::folly_test_util "${_IMPORT_PREFIX}/lib/libfolly_test_util.so.0.58.0-dev" )

# Import target "Folly::follybenchmark" for configuration "Debug"
set_property(TARGET Folly::follybenchmark APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(Folly::follybenchmark PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libfollybenchmark.so.0.58.0-dev"
  IMPORTED_SONAME_DEBUG "libfollybenchmark.so.0.58.0-dev"
  )

list(APPEND _IMPORT_CHECK_TARGETS Folly::follybenchmark )
list(APPEND _IMPORT_CHECK_FILES_FOR_Folly::follybenchmark "${_IMPORT_PREFIX}/lib/libfollybenchmark.so.0.58.0-dev" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
