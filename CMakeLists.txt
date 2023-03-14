cmake_minimum_required( VERSION 3.13.0 )
project( MediaServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Do not allow in-source build.
if( ${PROJECT_SOURCE_DIR} STREQUAL ${PROJECT_BINARY_DIR} )
    message( FATAL_ERROR "In-source build is not allowed. Please build in a separate directory, such as ${PROJECT_SOURCE_DIR}/build." )
endif()

# Enable testing
enable_testing()

add_subdirectory(ext/googletest)

# Media server lib
add_library(MediaServerLib
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/DependencyDescriptor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPDepacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeaderExtension.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPayload.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/SimulcastMediaFrameListener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vp8/vp8depacketizer.cpp
)

target_include_directories(MediaServerLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/ext/libdatachannels/src/internal
)

# Unit test executable
add_executable(MediaServerUnitTest
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestSimulcastMediaFrameListener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestVP8Depacketizer.cpp
)

target_link_libraries(MediaServerUnitTest
    MediaServerLib
    gtest
    gtest_main
)


add_test(NAME MediaServerUnitTest
         COMMAND MediaServerUnitTest)
