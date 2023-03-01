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

# Unit test executable
add_executable(MediaServerUnitTest 
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestSimulcastVideoMultiplexer.cpp
)

# Include directories
target_include_directories(MediaServerUnitTest PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/ext/libdatachannels/src/internal
)

target_link_libraries(MediaServerUnitTest 
    gtest 
    gtest_main
)


add_test(NAME MediaServerUnitTest
         COMMAND MediaServerUnitTest)
