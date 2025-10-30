set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB SOURCES "*/*.cpp")

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/trading)

set(3rd_LIBS
  ${FOLLY_LIBRARIES}
  ${Boost_LIBRARIES}
  ${OPENSSL_LIBRARIES}
  ${GFLAGS_LIBRARIES}
  ${LIBEVENT_LIB}
  ${DOUBLE_CONVERSION_LIBRARIES}
  ${CMAKE_DL_LIBS}
  ${LIBRT_LIBRARIES}      
  #log4cplus
)

add_library(libtrading STATIC ${SOURCES})

target_include_directories(libtrading
  PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}         
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(libtrading PUBLIC
    Boost::iostreams
    Boost::system
    Boost::context
    fmt::fmt
    #Folly::folly
    #${FOLLY_LIBRARIES}
    OpenSSL::SSL 
    OpenSSL::Crypto   
    Threads::Threads
    log4cplus::log4cplus   
    #${3rd_LIBS}
    pthread
    numa
)


link_directories(
    ${CMAKE_SOURCE_DIR}/installed/lib
)

install(
  TARGETS libtrading
  DESTINATION trading/${BIN_INSTALL_DIR}
)



# install(
#    FILES 
#      config/config.json
#    DESTINATION trading/${CONFIG_INSTALL_DIR}
# )