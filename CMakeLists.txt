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
    ${CMAKE_CURRENT_LIST_DIR}/src/DependencyDescriptorLayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/h264/H264LayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/h265/H265LayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/DependencyDescriptor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/LayerInfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPCommonHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPSenderReport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPDepacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeaderExtension.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPOutgoingSource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPOutgoingSourceGroup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPayload.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPSource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPStreamTransponder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/EventLoop.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/FrameDelayCalculator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/FrameDispatchCoordinator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/MediaFrameListenerBridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/PacketHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/SimulcastMediaFrameListener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/VideoLayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vp8/vp8depacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vp8/VP8LayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vp9/VP9LayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/vp9/VP9PayloadDescription.cpp
)

target_include_directories(MediaServerLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/ext/libdatachannels/src/internal
)

# Unit test executable
add_executable(MediaServerUnitTest
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestAccumulator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestCircularBuffer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestCircularQueue.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestFrameDelayCalculator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestFrameDispatchCoordinator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestMovingCounter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestRTPStreamTransponder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestSimulcastMediaFrameListener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestVP8Depacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/data/FramesArrivalInfo.cpp
)

target_link_libraries(MediaServerUnitTest
    MediaServerLib
    gtest
    gtest_main
)


add_test(NAME MediaServerUnitTest
         COMMAND MediaServerUnitTest)
