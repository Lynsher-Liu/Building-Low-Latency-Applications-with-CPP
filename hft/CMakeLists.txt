cmake_minimum_required(VERSION 3.5)

# Enable testing globally
enable_testing()

# We use the GoogleTest module if it is available (only in CMake 3.9+)
# It requires CMP0054 and CMP0057 to be enabled.
if (POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()
if (POLICY CMP0057)
  cmake_policy(SET CMP0057 NEW)
endif()

if (NOT DEFINED PACKAGE_VERSION)
  set(PACKAGE_VERSION "1.0.0")
endif()

if (NOT DEFINED SO_VERSION)
  set(SO_VERSION 1)
endif()

project(LowLatencyApp)


if (NOT DEFINED CPACK_GENERATOR)
  set(CPACK_GENERATOR "DEB")
endif()
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "ASTRI")
include(CPack)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS "-std=c++2a -ldl -Wall -Wextra  -Wpedantic")

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-reorder")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O2 ") #-fsanitize=address

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/"
  # for in-fbsource builds
  "${CMAKE_CURRENT_SOURCE_DIR}/../opensource/fbcode_builder/CMake"
  # For shipit-transformed builds
  "${CMAKE_CURRENT_SOURCE_DIR}/build/fbcode_builder/CMake"
  ${CMAKE_MODULE_PATH})

# include(FBBuildOptions)
# fb_activate_static_library_option()

# When installing Folly & Wangle in a non-default prefix, this will let
# projects linking against libwangle.so to find libfolly.so automatically.
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(INCLUDE_INSTALL_DIR include CACHE STRING
    "The subdirectory where header files should be installed")
set(LIB_INSTALL_DIR lib CACHE STRING
    "The subdirectory where libraries should be installed")
set(CMAKE_INSTALL_DIR lib/cmake/ngc CACHE STRING
    "The subdirectory where CMake package config files should be installed")
set(CONFIG_INSTALL_DIR config CACHE STRING
    "The subdirectory where the configurations should be installed")

set_property(GLOBAL PROPERTY GLOBAL_DEPENDS_DEBUG_MODE 1)

# -------------------- Optional / local third-party deps --------------------
# Folly is only required for the websocket market-data path. In restricted environments
# we build without it and compile only the non-websocket components.
find_package(folly CONFIG QUIET)
if (folly_FOUND)
  add_compile_definitions(HFT_ENABLE_WEBSOCKET=1)
else()
  add_compile_definitions(HFT_ENABLE_WEBSOCKET=0)
endif()

# fmt: use header-only mode from the vendored headers to avoid needing an installed fmt package.
add_library(fmt_header_only INTERFACE)
add_library(fmt::fmt ALIAS fmt_header_only)
target_compile_definitions(fmt_header_only INTERFACE FMT_HEADER_ONLY=1)
target_include_directories(fmt_header_only INTERFACE ${CMAKE_SOURCE_DIR}/third-party/fmt/include)

find_package(OpenSSL REQUIRED)
#find_package(Glog REQUIRED)
find_package(gflags CONFIG QUIET)
# log4cplus: prefer a CMake package if present, otherwise fall back to pkg-config.
find_package(log4cplus CONFIG QUIET)
if (NOT log4cplus_FOUND)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LOG4CPLUS REQUIRED log4cplus)
  find_library(LOG4CPLUS_LIB log4cplus HINTS ${LOG4CPLUS_LIBRARY_DIRS})
  if (NOT LOG4CPLUS_LIB)
    message(FATAL_ERROR "log4cplus library not found")
  endif()
  add_library(log4cplus::log4cplus UNKNOWN IMPORTED)
  set_target_properties(log4cplus::log4cplus PROPERTIES IMPORTED_LOCATION "${LOG4CPLUS_LIB}")
  target_include_directories(log4cplus::log4cplus INTERFACE ${LOG4CPLUS_INCLUDE_DIRS})
endif()
#find_library(LOG4CPLUS_LIB NAMES log4cplus PATHS "${CMAKE_PREFIX_PATH}/lib" REQUIRED)
if (gflags_FOUND)
  message(STATUS "Found gflags from package config")
  message(STATUS "gflags_CONFIG=${gflags_CONFIG}")
else()
  find_package(Gflags REQUIRED)
endif()
#find_package(LibEvent MODULE REQUIRED)
#find_package(DoubleConversion REQUIRED)
find_package(Threads REQUIRED)
if (UNIX AND NOT APPLE)
  find_package(Librt)
endif()
find_package(Boost REQUIRED COMPONENTS iostreams system context)

#include(CheckAtomic)

# set(TRADING_HEADER_DIRS
#   http
# )

# foreach(dir ${TRADING_HEADER_DIRS})
#  file(GLOB_RECURSE headers ${dir}/*.h)
#  set(TRADING_HEADERS
#    ${TRADING_HEADERS}
#    ${headers})
#  message(STATUS "TRADING_HEADERS=${TRADING_HEADERS}")
# endforeach()

include_directories(
  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${INCLUDE_INSTALL_DIR}>
  ${CMAKE_SOURCE_DIR}/third-party
  ${LIBGFLAGS_INCLUDE_DIR}
  #${GLOG_INCLUDE_DIRS}
  ${OPENSSL_INCLUDE_DIR}
  ${ZLIB_INCLUDE_DIRS}
  ${ZSTD_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${FIZZ_INCLUDE_DIR}
  ${FOLLY_INCLUDE_DIR}
  ${GFLAGS_INCLUDE_DIRS}
  ${LIBEVENT_INCLUDE_DIR}
  ${DOUBLE_CONVERSION_INCLUDE_DIRS}
)

#add_subdirectory(common)
#add_subdirectory(exchange)
add_subdirectory(trading)

#list(APPEND LIBS libexchange)
list(APPEND LIBS libtrading)
#list(APPEND LIBS libcommon)
list(APPEND LIBS pthread)

include_directories(${PROJECT_SOURCE_DIR})
#include_directories(${PROJECT_SOURCE_DIR}/exchange)
include_directories(${PROJECT_SOURCE_DIR}/trading)

# add_executable(exchange_main exchange/exchange_main.cpp)
# target_link_libraries(exchange_main PUBLIC ${LIBS} 
# #     ${GTEST_BOTH_LIBRARIES}
# #   ${GMOCK_BOTH_LIBRARIES}
#    ${3rd_LIBS}
# #   pthread
# #   numa
#   )

# install(
#   TARGETS exchange_main
#   DESTINATION bin/
# )

add_executable(trading_main trading/trading_main.cpp)
target_link_libraries(trading_main PUBLIC ${LIBS}
#     ${GTEST_BOTH_LIBRARIES}
#   ${GMOCK_BOTH_LIBRARIES}
#   ${3rd_LIBS}
   numa 
  )

install(
  TARGETS trading_main
  DESTINATION bin/
)

install(
   FILES 
     config/config.json
     config/trading.log.properties
     config/.env
   DESTINATION ${CONFIG_INSTALL_DIR}
)

# Install CMake package configuration files for wangle
include(CMakePackageConfigHelpers)