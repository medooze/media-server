cmake_minimum_required(VERSION 3.13)
project(medooze-media_server)

set(CMAKE_CXX_STANDARD 17)

#加载自定义模块
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake")

add_definitions(-DFLOATING_POINT)
add_definitions(-DSPX_RESAMPLE_EXPORT= )
add_definitions(-DOPENSSL_USE_STATIC_LIBS)
add_definitions(-DGCM)

set (CMAKE_CXX_FLAGS"${CMAKE_CXX_FLAGS} -Wno-narrowing")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
file(GLOB PROJECT_SRC_LIST
        ${CMAKE_SOURCE_DIR}/src/*.c
        ${CMAKE_SOURCE_DIR}/src/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*.h
        ${CMAKE_SOURCE_DIR}/src/*/*.c
        ${CMAKE_SOURCE_DIR}/src/*/*.cpp
        ${CMAKE_SOURCE_DIR}/src/*/*.h
        )

#Remove redundant files
list(REMOVE_ITEM PROJECT_SRC_LIST ${CMAKE_SOURCE_DIR}/src/bwe.cpp)
list(REMOVE_ITEM PROJECT_SRC_LIST ${CMAKE_SOURCE_DIR}/src/receiver.cpp)
list(REMOVE_ITEM PROJECT_SRC_LIST ${CMAKE_SOURCE_DIR}/src/sender.cpp)
list(REMOVE_ITEM PROJECT_SRC_LIST ${CMAKE_SOURCE_DIR}/src/nocodecs.cpp)
list(REMOVE_ITEM PROJECT_SRC_LIST ${CMAKE_SOURCE_DIR}/src/rtmp/rtmpdebug.cpp)

#查找openssl是否安装
find_package(OpenSSL REQUIRED)
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL Not Found")
endif(NOT OPENSSL_FOUND)
include_directories(${OPENSSL_INCLUDE_DIR})
list(APPEND LINK_LIB_LIST ${OPENSSL_LIBRARIES})
message(STATUS "openssl path: ${OPENSSL_INCLUDE_DIR}, lib: ${OPENSSL_LIBRARIES}")

#查找X264
find_package(x264 REQUIRED)
if(NOT X264_FOUND)
    message(FATAL_ERROR "x264 not found.")
endif(NOT X264_FOUND)
include_directories(${X264_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${X264_LIBS})
message(STATUS "x264 path: ${X264_INCLUDE_DIRS}, lib: ${X264_LIBS}")

#查找FFMPEG
find_package(FFmpeg REQUIRED)
if(NOT FFMPEG_FOUND)
    message(FATAL_ERROR "FFMPEG not found.")
endif(NOT FFMPEG_FOUND)
include_directories(${FFMPEG_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${FFMPEG_LIBRARIES})
message(STATUS "ffmpeg path: ${FFMPEG_INCLUDE_DIRS}, lib: ${FFMPEG_LIBRARIES}")
add_definitions(${FFMPEG_DEFINITIONS})

#查找srtp
find_package(SRTP REQUIRED)
if(NOT SRTP_FOUND)
    message(FATAL_ERROR "Could not find SRTP library")
endif(NOT SRTP_FOUND)
include_directories(${SRTP_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${SRTP_LIBRARIES})
message(STATUS "srtp path: ${SRTP_INCLUDE_DIRS}, lib: ${SRTP_LIBRARIES}")

#查找GSM
find_package(Gsm REQUIRED)
if(NOT GSM_FOUND)
    message(FATAL_ERROR "Cound not find Gsm library")
endif(NOT GSM_FOUND)
include_directories(${GSM_INCLUDE_DIR})
list(APPEND LINK_LIB_LIST ${GSM_LIBRARIES})
message(STATUS "libgsm path: ${GSM_INCLUDE_DIR}, lib: ${GSM_LIBRARIES}")

#查找XMLRPC
find_package(XMLRPC REQUIRED c++ libwww-client cgi-server abyss-server)
if(NOT XMLRPC_C_FOUND)
    message(FATAL_ERROR "Cound not find XMLRPC library")
endif(NOT XMLRPC_C_FOUND)
include_directories(${XMLRPC_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${XMLRPC_LIBRARIES})
message(STATUS "xmlp-c path: ${XMLRPC_INCLUDE_DIRS}, lib: ${XMLRPC_LIBRARIES}")

#查找MP4V2
find_package(MP4V2 REQUIRED)
if(NOT MP4V2_FOUND)
    message(FATAL_ERROR "Cound not find MP4V2 library")
endif(NOT MP4V2_FOUND)
include_directories(${MP4V2_INCLUDE_DIR})
list(APPEND LINK_LIB_LIST ${MP4V2_LIBRARIES})
message(STATUS "mp4v2 path: ${MP4V2_INCLUDE_DIR}, lib: ${MP4V2_LIBRARIES}")

find_package(Opus REQUIRED)
if(NOT OPUS_FOUND)
    message(FATAL_ERROR "Cound not find Opus library")
endif(NOT OPUS_FOUND)
include_directories(${OPUS_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${OPUS_LIBRARIES})
message(STATUS "opus path: ${OPUS_INCLUDE_DIRS}, lib: ${OPUS_LIBRARIES}")

find_package(VPX REQUIRED)
if(NOT VPX_FOUND)
    message(FATAL_ERROR "Cound not find VPX library")
endif(NOT VPX_FOUND)
include_directories(${VPX_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${VPX_LIBRARIES})
message(STATUS "vpx path: ${VPX_INCLUDE_DIRS}, lib: ${VPX_LIBRARIES}")

find_package(Speex REQUIRED)
if(NOT SPEEX_FOUND)
    message(FATAL_ERROR "Cound not find Speex library")
endif(NOT SPEEX_FOUND)
include_directories(${SPEEX_INCLUDE_DIRS})
list(APPEND LINK_LIB_LIST ${SPEEX_LIBRARIES})
message(STATUS "speex path: ${SPEEX_INCLUDE_DIRS}, lib: ${SPEEX_LIBRARIES}")

add_subdirectory(${CMAKE_SOURCE_DIR}/ext/libdatachannels)
include_directories(${CMAKE_SOURCE_DIR}/ext/libdatachannels/src)
include_directories(${CMAKE_SOURCE_DIR}/ext/libdatachannels/src/internal)
link_libraries(libdatachannels)

add_subdirectory(${CMAKE_SOURCE_DIR}/ext/crc32c)
link_libraries(crc32c)

add_executable(${CMAKE_PROJECT_NAME} ${PROJECT_SRC_LIST})
target_link_libraries(${CMAKE_PROJECT_NAME} ${LINK_LIB_LIST})
