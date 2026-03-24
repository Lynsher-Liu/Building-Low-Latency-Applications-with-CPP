#!/bin/bash

# TODO - point these to the correct binary locations on your system.
CMAKE=$(which cmake)
NINJA=$(which ninja)

# Useful constants
COLOR_RED="\033[0;31m"
COLOR_GREEN="\033[0;32m"
COLOR_OFF="\033[0m"
#runCountFile="/home/numOfRuns.txt"
#maxRuns=1

function detect_platform() {
  unameOut="$(uname -s)"
  case "${unameOut}" in
      Linux*)     PLATFORM=Linux;;
      Darwin*)    PLATFORM=Mac;;
      *)          PLATFORM="UNKNOWN:${unameOut}"
  esac
  echo -e "${COLOR_GREEN}Detected platform: $PLATFORM ${COLOR_OFF}"
}

function install_dependencies_linux() {
  apt-get update
  apt-get install -yq \
    git \
    cmake \
    m4 \
    g++ \
    flex \
    bison \
    libgflags-dev \
    libgoogle-glog-dev \
    libkrb5-dev \
    libsasl2-dev \
    libnuma-dev \
    pkg-config \
    libssl-dev \
    libcap-dev \
    gperf \
    libevent-dev \
    libtool \
    libjemalloc-dev \
    libsnappy-dev \
    wget \
    unzip \
    libiberty-dev \
    liblz4-dev \
    liblzma-dev \
    make \
    zlib1g-dev \
    binutils-dev \
    libsodium-dev \
    libdouble-conversion-dev
}

function install_dependencies() {
  echo -e "${COLOR_GREEN}[ INFO ] install dependencies ${COLOR_OFF}"
  if [ "$PLATFORM" = "Linux" ]; then
    install_dependencies_linux
  elif [ "$PLATFORM" = "Mac" ]; then
    install_dependencies_mac
  else
    echo -e "${COLOR_RED}[ ERROR ] Unknown platform: $PLATFORM ${COLOR_OFF}"
    exit 1
  fi
}

function synch_dependency_to_commit() {
  # Utility function to synch a dependency to a specific commit. Takes two arguments:
  #   - $1: folder of the dependency's git repository
  #   - $2: path to the text file containing the desired commit hash
  if [ "$FETCH_DEPENDENCIES" = false ] ; then
    return
  fi
  DEP_REV=$(sed 's/Subproject commit //' "$2")
  pushd "$1"
  git fetch
  # Disable git warning about detached head when checking out a specific commit.
  git -c advice.detachedHead=false checkout "$DEP_REV"
  popd
}

function setup_fmt() {
  FMT_DIR=$DEPS_DIR/fmt
  FMT_BUILD_DIR=$DEPS_DIR/fmt/build/

  if [ ! -d "$FMT_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning fmt repo ${COLOR_OFF}"
    git clone https://github.com/fmtlib/fmt.git  "$FMT_DIR"
  fi
  cd "$FMT_DIR"
  git fetch --tags
  git checkout 6.2.1
  echo -e "${COLOR_GREEN}Building fmt ${COLOR_OFF}"
  mkdir -p "$FMT_BUILD_DIR"
  cd "$FMT_BUILD_DIR" || exit

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
    -DBUILD_SHARED_LIBS=OFF                       \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    -DFMT_DOC=OFF                                 \
    -DFMT_TEST=OFF                                \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}fmt is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}

function setup_googletest() {
  GTEST_DIR=$DEPS_DIR/googletest
  GTEST_BUILD_DIR=$DEPS_DIR/googletest/build/

  if [ ! -d "$GTEST_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning googletest repo ${COLOR_OFF}"
    git clone https://github.com/google/googletest.git  "$GTEST_DIR"
  fi
  cd "$GTEST_DIR"
  git fetch --tags
  git checkout release-1.8.0
  echo -e "${COLOR_GREEN}Building googletest ${COLOR_OFF}"
  mkdir -p "$GTEST_BUILD_DIR"
  cd "$GTEST_BUILD_DIR" || exit

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}googletest is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}

function setup_zstd() {
  ZSTD_DIR=$DEPS_DIR/zstd
  ZSTD_BUILD_DIR=$DEPS_DIR/zstd/build/cmake/build

  if [ ! -d "$ZSTD_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning zstd repo ${COLOR_OFF}"
    git clone https://github.com/facebook/zstd.git  "$ZSTD_DIR"
  fi
  cd "$ZSTD_DIR"
  git fetch --tags
  git checkout v1.4.5
  echo -e "${COLOR_GREEN}Building zstd ${COLOR_OFF}"
  mkdir -p "$ZSTD_BUILD_DIR"
  cd "$ZSTD_BUILD_DIR" || exit

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}zstd is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}

function setup_folly() {
  FOLLY_DIR=$DEPS_DIR/folly
  FOLLY_BUILD_DIR=$DEPS_DIR/folly/build/

  if [ ! -d "$FOLLY_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning folly repo ${COLOR_OFF}"
    git clone https://github.com/facebook/folly.git "$FOLLY_DIR"
    synch_dependency_to_commit "$FOLLY_DIR" "$BASE_DIR"/build/deps/github_hashes/facebook/folly-rev.txt
  fi
  if [ "$PLATFORM" = "Mac" ]; then
    # Homebrew installs OpenSSL in a non-default location on MacOS >= Mojave
    # 10.14 because MacOS has its own SSL implementation.  If we find the
    # typical Homebrew OpenSSL dir, load OPENSSL_ROOT_DIR so that cmake
    # will find the Homebrew version.
    dir=/usr/local/opt/openssl
    if [ -d $dir ]; then
        export OPENSSL_ROOT_DIR=$dir
    fi
  fi
  cd "$FOLLY_DIR"
  git fetch --tags
  git checkout f601d24
  echo -e "${COLOR_GREEN}Building Folly ${COLOR_OFF}"
  mkdir -p "$FOLLY_BUILD_DIR"
  cd "$FOLLY_BUILD_DIR" || exit
  MAYBE_DISABLE_JEMALLOC="-DFOLLY_USE_JEMALLOC=1"
  if [ "$NO_JEMALLOC" == true ] ; then
    MAYBE_DISABLE_JEMALLOC="-DFOLLY_USE_JEMALLOC=0"
  fi

  MAYBE_USE_STATIC_DEPS=""
  MAYBE_USE_STATIC_BOOST=""
  MAYBE_BUILD_SHARED_LIBS=""
  if [ "$BUILD_FOR_FUZZING" == true ] ; then
    MAYBE_USE_STATIC_DEPS="-DUSE_STATIC_DEPS_ON_UNIX=ON"
    MAYBE_USE_STATIC_BOOST="-DBOOST_LINK_STATIC=ON"
    MAYBE_BUILD_SHARED_LIBS="-DBUILD_SHARED_LIBS=OFF"
  fi

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
    -DBUILD_TESTS=OFF                             \
    -DBUILD_SHARED_LIBS=ON                        \
    "$MAYBE_USE_STATIC_BOOST"                     \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    $MAYBE_DISABLE_JEMALLOC                       \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}Folly is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function setup_boost() {
  BOOST_DIR=$DEPS_DIR/boost
  BOOST_BUILD_DIR=$DEPS_DIR/boost/build/

  if [ ! -d "$BOOST_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning boost repo ${COLOR_OFF}"
    git clone --recurse-submodules -b boost-1.82.0 https://github.com/boostorg/boost.git "$BOOST_DIR"
  fi
  cd "$BOOST_DIR"
  # git submodule update --init tools/build
  # git submodule update --init tools/boost_install
  # git submodule update --init libs/config
  # git submodule update --init libs/headers
  # git submodule update --init libs/detail
  # git submodule update --init libs/system
  # git submodule update --init libs/exception
  # git submodule update --init libs/any
  # git submodule update --init libs/predef
  # git submodule update --init libs/assert
  # git submodule update --init libs/throw_exception
  # git submodule update --init libs/smart_ptr
  # git submodule update --init libs/type_index
  # git submodule update --init libs/bind
  # git submodule update --init libs/utility
  # git submodule update --init libs/type_traits
  # git submodule update --init libs/static_assert
  # git submodule update --init libs/iostreams
  # git submodule update --init libs/context
  # git submodule update --init libs/filesystem
  # git submodule update --init libs/program_options
  # git submodule update --init libs/regex
  
  echo -e "${COLOR_GREEN}Building boost ${COLOR_OFF}"
  mkdir -p "$BOOST_BUILD_DIR"
  
  ./bootstrap.sh
  ./b2 --prefix=../../installed --build-dir=./build --with-thread --with-system --with-iostreams --with-date_time --with-context --with-filesystem --with-program_options --with-regex install
  
  echo -e "${COLOR_GREEN}boost is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function setup_log4cplus() {
  LOG4CPLUS_DIR=$DEPS_DIR/log4cplus
  LOG4CPLUS_BUILD_DIR=$DEPS_DIR/log4cplus/build/

  if [ ! -d "$LOG4CPLUS_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning log4cplus repo ${COLOR_OFF}"
    git clone  --recurse-submodules -b REL_2_0_8 https://github.com/log4cplus/log4cplus.git "$LOG4CPLUS_DIR"
  fi
  cd "$LOG4CPLUS_DIR"
  echo -e "${COLOR_GREEN}Building log4cplus ${COLOR_OFF}"
  mkdir -p "$LOG4CPLUS_BUILD_DIR"
  cd "$LOG4CPLUS_BUILD_DIR" || exit

  cmake                                     \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"         \
    "$MAYBE_OVERRIDE_CXX_FLAGS"             \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"        \
	  ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}log4cplus is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function setup_openssl() {
  OPENSSL_DIR=$DEPS_DIR/openssl
  # OPENSSL_BUILD_DIR=$DEPS_DIR/openssl/build/

  if [ ! -d "$OPENSSL_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning openssl repo ${COLOR_OFF}"
    git clone -b openssl-3.0.0 https://github.com/openssl/openssl.git "$OPENSSL_DIR"
  fi
  cd "$OPENSSL_DIR"
  echo -e "${COLOR_GREEN}Building openssl ${COLOR_OFF}"
  
  ./config --prefix="$PREFIX" no-unit-test
  make install_sw

  echo -e "${COLOR_GREEN}openssl is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function setup_pahomqttc() {
  PAHOMQTTC_DIR=$DEPS_DIR/pahomqttc
  PAHOMQTTC_BUILD_DIR=$DEPS_DIR/pahomqttc/build/

  if [ ! -d "$PAHOMQTTC_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning pahomqttc repo ${COLOR_OFF}"
    git clone https://github.com/eclipse/paho.mqtt.c.git  "$PAHOMQTTC_DIR"
  fi
  cd "$PAHOMQTTC_DIR"
  git fetch --tags
  git checkout v1.3.13
  echo -e "${COLOR_GREEN}Building pahomqttc ${COLOR_OFF}"
  mkdir -p "$PAHOMQTTC_BUILD_DIR"
  cd "$PAHOMQTTC_BUILD_DIR" || exit

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
	-DPAHO_BUILD_STATIC=FALSE                      \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}pahomqttc is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}

function setup_pahomqttcpp() {
  PAHOMQTTCPP_DIR=$DEPS_DIR/pahomqttcpp
  PAHOMQTTCPP_BUILD_DIR=$DEPS_DIR/pahomqttcpp/build/

  if [ ! -d "$PAHOMQTTCPP_DIR" ] ; then
    echo -e "${COLOR_GREEN}[ INFO ] Cloning pahomqttcpp repo ${COLOR_OFF}"
    git clone https://github.com/eclipse/paho.mqtt.cpp.git  "$PAHOMQTTCPP_DIR"
  fi
  cd "$PAHOMQTTCPP_DIR"
  git fetch --tags
  git checkout v1.3.2
  echo -e "${COLOR_GREEN}Building pahomqttcpp ${COLOR_OFF}"
  mkdir -p "$PAHOMQTTCPP_BUILD_DIR"
  cd "$PAHOMQTTCPP_BUILD_DIR" || exit

  cmake                                           \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"               \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"                   \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"              \
	-DPAHO_WITH_SSL=OFF							  \
	-DPAHO_WITH_MQTT_C=FALSE					  \
	-DPAHO_MQTT_C_LIBRARIES="$PREFIX"/lib/libpaho-mqtt3a.so \
	-DPAHO_MQTT_C_INCLUDE_DIRS="$PREFIX"/include/  \
    ..
  make -j "$JOBS" $MAYBE_VERBOSE
  make install $MAYBE_VERBOSE
  echo -e "${COLOR_GREEN}pahomqttcpp is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}

function setup_mapf() {
  MAPF_DIR=$BASE_DIR
  MAPF_BUILD_DIR=$BASE_DIR/build/
  echo -e "${COLOR_GREEN}Building MAPF ${COLOR_OFF}"  

  mkdir -p "$MAPF_BUILD_DIR"
  cd "$MAPF_BUILD_DIR" || exit
  
  MAYBE_BUILD_TESTS="-DBUILD_TESTS=ON"
  if [ "$NO_BUILD_TESTS" == true ] ; then
    MAYBE_BUILD_TESTS="-DBUILD_TESTS=OFF"
  fi

  BUILD_MODULES=${MODULE}
  if [ "${BUILD_COMPONENTS}" != "trading" ]; then
    BUILD_MODULES="all"
  elif [ "${BUILD_MODULES}" = "" ]; then
    BUILD_MODULES="all"
  fi 
  
  cmake                                     \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"        \
    -DCMAKE_PREFIX_PATH="$DEPS_DIR"         \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"        \
    -DCMAKE_MODULE="$BUILD_MODULES"         \
    "$MAYBE_BUILD_QUIC"                     \
    "$MAYBE_BUILD_TESTS"                    \
    "$MAYBE_BUILD_FUZZERS"                  \
    "$MAYBE_BUILD_SHARED_LIBS"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"             \
    "$MAYBE_USE_STATIC_DEPS"                \
    "$MAYBE_LIB_FUZZING_ENGINE"             \
    "$MAPF_DIR"
  echo -e "haha4"
  
  make -j "$JOBS" $MAYBE_VERBOSE
  
  echo -e "haha5"
  
  make install $MAYBE_VERBOSE
   
  echo -e "${COLOR_GREEN}MAPF is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function setup_trading() {
  TRADING_DIR=$BASE_DIR
  TRADING_BUILD_DIR=$BASE_DIR/build/
  echo -e "${COLOR_GREEN}Building TRADING ${COLOR_OFF}"  

  mkdir -p "$TRADING_BUILD_DIR"
  cd "$TRADING_BUILD_DIR" || exit
  
  MAYBE_BUILD_TESTS="-DBUILD_TESTS=ON"
  if [ "$NO_BUILD_TESTS" == true ] ; then
    MAYBE_BUILD_TESTS="-DBUILD_TESTS=OFF"
  fi

  BUILD_MODULES=${MODULE}
  if [ "${BUILD_COMPONENTS}" != "trading" ]; then
    BUILD_MODULES="all"
  elif [ "${BUILD_MODULES}" = "" ]; then
    BUILD_MODULES="all"
  fi 
  
  cmake                                     \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"        \
    -DCMAKE_PREFIX_PATH="$PREFIXs"         \
    -DCMAKE_INSTALL_PREFIX="$PREFIX"        \
    -DCMAKE_MODULE="$BUILD_MODULES"         \
    "$MAYBE_BUILD_QUIC"                     \
    "$MAYBE_BUILD_TESTS"                    \
    "$MAYBE_BUILD_FUZZERS"                  \
    "$MAYBE_BUILD_SHARED_LIBS"              \
    "$MAYBE_OVERRIDE_CXX_FLAGS"             \
    "$MAYBE_USE_STATIC_DEPS"                \
    "$MAYBE_LIB_FUZZING_ENGINE"             \
    "$TRADING_DIR"
  echo -e "${COLOR_GREEN}Finish CMAKE configuration ${COLOR_OFF}"
  
  #make -j "$JOBS" $MAYBE_VERBOSE
  $CMAKE -DCMAKE_MAKE_PROGRAM=$NINJA -G Ninja -S . -B "$TRADING_BUILD_DIR"
  $CMAKE --build "$TRADING_BUILD_DIR" --target clean -j "$JOBS"
  $CMAKE --build "$TRADING_BUILD_DIR" --target all -j "$JOBS"

  #cmake --build "$TRADING_BUILD_DIR" -- -j"$JOBS" ${MAYBE_VERBOSE:+-v}
  #cmake --build "$TRADING_BUILD_DIR" --target install
  # 或：cmake --install "$TRADING_BUILD_DIR" --prefix "$PREFIX"
  
  echo -e "${COLOR_GREEN}Finish cmake build ${COLOR_OFF}"
  
  #make install $MAYBE_VERBOSE
  $CMAKE --build "$TRADING_BUILD_DIR" --target install
   
  echo -e "${COLOR_GREEN}TRADING is installed ${COLOR_OFF}"
  cd "$BASE_DIR" || exit
}


function install_header_only_headers() {
  HEADER_ONLY_SRC_DIR="${HEADER_ONLY_SRC_DIR:-$DEPS_DIR}"
  HEADER_ONLY_DST_DIR="${HEADER_ONLY_DST_DIR:-$PREFIX/include}"
  HEADER_ONLY_LIBS="${HEADER_ONLY_LIBS:-concurrentqueue json struct_pack}"  # add more names if needed

  if [ ! -d "$HEADER_ONLY_SRC_DIR" ]; then
    echo -e "${COLOR_RED}[ ERROR ] header-only source base directory not found at $HEADER_ONLY_SRC_DIR ${COLOR_OFF}"
    return 1
  fi

  echo -e "${COLOR_GREEN}[ INFO ] Installing header-only libraries from $HEADER_ONLY_SRC_DIR to $HEADER_ONLY_DST_DIR ${COLOR_OFF}"
  mkdir -p "$HEADER_ONLY_DST_DIR"

  for lib in $HEADER_ONLY_LIBS; do
    src="$HEADER_ONLY_SRC_DIR/$lib"
    if [ -d "$src" ]; then
      echo -e "${COLOR_GREEN}[ INFO ] Copying header-only lib $lib ${COLOR_OFF}"
      mkdir -p "$HEADER_ONLY_DST_DIR/$lib"
      cp -a "$src"/. "$HEADER_ONLY_DST_DIR/$lib"/ || {
        echo -e "${COLOR_RED}[ ERROR ] failed to copy $src to $HEADER_ONLY_DST_DIR/$lib ${COLOR_OFF}"
        return 1
      }
    else
      echo -e "${COLOR_GREEN}[ INFO ] header-only lib not present: $src (skip) ${COLOR_OFF}"
    fi
  done

  echo -e "${COLOR_GREEN}[ INFO ] header-only files installed successfully. ${COLOR_OFF}"
}

function clean_trading() {
  echo clean_trading
  rm -rf build/
}

function build() {

  if [ "${BUILD_COMPONENTS}" = "" ]; then
    BUILD_COMPONENTS="trading"
  fi

  install_header_only_headers

  case "${BUILD_COMPONENTS}" in
    "all" | "fmt")
      setup_fmt
      ;;&
    "all" | "googletest")
      setup_googletest
      ;;&
    "all" | "zstd")
      setup_zstd
      ;;&
    "all" | "boost")
      setup_boost
      ;;&
    "all" | "folly")
      setup_folly
      ;;&
    "all" | "log4cplus")
      setup_log4cplus
      ;;&
    "all" | "openssl")
      setup_openssl
      ;;&
    "all" | "trading")
      setup_trading
      ;;&
  esac
}

function build_third_party() {

  echo -e "${COLOR_GREEN}build_third_party lib ${COLOR_OFF}"
  
  if [ "${BUILD_COMPONENTS}" = "" ]; then
    BUILD_COMPONENTS="all"
  fi

  case "${BUILD_COMPONENTS}" in
    "all" | "fmt")
      setup_fmt
      ;;&
    "all" | "googletest")
      setup_googletest
      ;;&
    "all" | "zstd")
      setup_zstd
      ;;&
    "all" | "boost")
      setup_boost
      ;;&
    "all" | "folly")
      setup_folly
      ;;&
    "all" | "log4cplus")
      setup_log4cplus
      ;;&
    "all" | "openssl")
      setup_openssl
      ;;&	  
    "all" | "pahomqttc")
      setup_pahomqttc
      ;;&
    "all" | "pahomqttcpp")
      setup_pahomqttcpp
      ;;&
    "all" | "trading")
      setup_trading
      ;;&
  esac
}

function package() {
  cd $BASE_DIR/build/
  make package
}

# Parse args
# JOBS="$( grep -c ^processor /proc/cpuinfo )"
# Must execute from the directory containing this script
cd "$(dirname "$0")"
BASE_DIR=$(pwd)
trap 'cd $BASE_DIR' EXIT

JOBS=10
WITH_QUIC=false
INSTALL_DEPENDENCIES=false
FETCH_DEPENDENCIES=true
PREFIX=$BASE_DIR/installed
DEPS_DIR=$BASE_DIR/third-party
HEADER_ONLY_SRC_DIR="$DEPS_DIR"
HEADER_ONLY_DST_DIR="$PREFIX/include"
MODULE=""
BUILD_COMPONENTS=""
NEED_CLEAN=false
COMPILER_FLAGS=""
BUILD_TYPE="Debug"
MAYBE_VERBOSE=""
GIT_TYPE="http"
MAYBE_BUILD_TESTS="-DBUILD_TESTS=OFF"
USAGE="./build.sh [-a|--all]  [-c|--clean]\n \
	  [-i|--install-dependencies]\n \
      examples: \n \   	
      ./build.sh -a\n \
       \t build the system including the third party and mapf source \n \
	   ./build.sh \n \
       \t build the mapf source only \n \
      ./build.sh -c\n \
       \t   Clean mapf "
	   
while [ "$1" != "" ]; do
  case $1 in
    -a | --all )
      BUILD_COMPONENTS="all"
      ;;
    -fmt )
      BUILD_COMPONENTS="fmt"
      ;;
    -folly )
      BUILD_COMPONENTS="folly"
      ;;
    -log4cplus )
      BUILD_COMPONENTS="log4cplus"
      ;;
	 -c | --clean )
      NEED_CLEAN=true
      ;;
    -i | --install-dependencies )
      detect_platform
      install_dependencies
      exit 1
      ;;
    * | -h | --help)
      echo -e $USAGE
      exit 1
esac
  shift
done
GIT_COMMIT_TAG="$(git log -n1 --format="%h")"
GIT_COMMIT_TIME="$(git log -1  --date=format:"%Y/%m/%d %T" --format="%ad")"
VERSION="${GIT_COMMIT_TIME}-${GIT_COMMIT_TAG}"
BUILD_TIME=$(date +"%Y/%m/%d %T")
MAYBE_OVERRIDE_CXX_FLAGS="-DCMAKE_CXX_FLAGS=-fPIC -Wall -Os -DVERSION=\"${VERSION}\" -DBUILD_TIME=\"${BUILD_TIME}\""
if [ -n "$COMPILER_FLAGS" ] ; then
  MAYBE_OVERRIDE_CXX_FLAGS="-DCMAKE_CXX_FLAGS=$COMPILER_FLAGS"
fi

if [ ${NEED_CLEAN} = true ] ; then
  clean_trading
  exit 0
fi

build
