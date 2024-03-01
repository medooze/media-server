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

# get/clone medooze/mp4v2 and add/populate it to the project
include(FetchContent)
fetchcontent_declare(
	mp4v2
	GIT_REPOSITORY https://github.com/medooze/mp4v2.git
	GIT_TAG master
	SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/ext/mp4v2
	)

FetchContent_MakeAvailable(mp4v2)


file(GLOB_RECURSE SOURCES_H265 RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/h265/*.cpp")
file(GLOB_RECURSE SOURCES_H264 RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/h264/*.cpp")
file(GLOB_RECURSE SOURCES_VP8 RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/vp8/*.cpp")
file(GLOB_RECURSE SOURCES_VP9 RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/vp9/*.cpp")
file(GLOB_RECURSE SOURCES_JPEG RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/jpeg/*.cpp")
file(GLOB_RECURSE SOURCES_WEBP RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/webp/*.cpp")
file(GLOB_RECURSE SOURCES_AAC RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/aac/*.cpp")

file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_LIST_DIR} "src/*.cpp")

# Media server lib
add_library(MediaServerLib
	${SOURCES_H264}
	${SOURCES_H265}
	${SOURCES_VP8}
	${SOURCES_VP9}
	${SOURCES_JPEG}
	${SOURCES_WEBP}
    ${SOURCES_AAC}
    ${CMAKE_CURRENT_LIST_DIR}/src/DependencyDescriptorLayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/av1/AV1LayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/av1/Obu.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/mpegts/SpliceInfoSection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpserver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/amf.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpclientconnection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpmessage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpserver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpconnection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpstream.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpnetconnection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmpchunk.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtmp/rtmppacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/DependencyDescriptor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/LayerInfo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPCommonHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPSenderReport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPReport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPDepacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPHeaderExtension.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPApp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPExtendedJitterReport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPMap.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPOutgoingSource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPOutgoingSourceGroup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPBye.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPFullIntraRequest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPPayloadFeedback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPRTPFeedback.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPayload.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPSource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPCompoundPacket.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPNACK.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPReceiverReport.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTCPSDES.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPPacketSched.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPStreamTransponder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPLostPackets.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPIncomingMediaStreamMultiplexer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPIncomingMediaStreamDepacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPIncomingSource.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/rtp/RTPIncomingSourceGroup.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/avcdescriptor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/EventLoop.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/stunmessage.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/FrameDelayCalculator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/FrameDispatchCoordinator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/MediaFrameListenerBridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/PacketHeader.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/mp4recorder.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/VideoBufferScaler.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/SimulcastMediaFrameListener.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/VideoLayerSelector.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/VideoCodecFactory.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/utf8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/ForwardErrorCorrection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/FecProbeGenerator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/crc32calc.cpp
)

target_include_directories(MediaServerLib PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${CMAKE_CURRENT_LIST_DIR}/ext/libdatachannels/src/internal
    ${CMAKE_CURRENT_LIST_DIR}/ext/mp4v2/include
)

target_link_libraries(MediaServerLib
    pthread
    vpx
    x264
    avcodec
    avutil
    swscale
    swresample
    crypto
)

#for debugging
#target_compile_options(MediaServerLib PUBLIC
#    -ggdb
#    -O0
#)

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
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestTimestampChecker.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestVP8Depacketizer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestAMFNumber.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestVideoLayersAllocation.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestTools.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestForwardErrorCorrection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestFecProbeGenerator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/TestSpliceInfoSection.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/unit/data/FramesArrivalInfo.cpp
)

target_link_libraries(MediaServerUnitTest
    MediaServerLib
    gtest
    gtest_main
)


add_test(NAME MediaServerUnitTest
         COMMAND MediaServerUnitTest)


add_executable(MediaServerLegacyTest
    ${CMAKE_CURRENT_LIST_DIR}/test/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/aac.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/cpim.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/dd.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/ddls.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/eventloop.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/fec.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/h264.cpp
    #${CMAKE_CURRENT_LIST_DIR}/test/overlay.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/rtmp.cpp
    #${CMAKE_CURRENT_LIST_DIR}/test/rtp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/stun.cpp
    #${CMAKE_CURRENT_LIST_DIR}/test/tools.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/vp8.cpp
    ${CMAKE_CURRENT_LIST_DIR}/test/vp9.cpp
)

target_link_libraries(MediaServerLegacyTest
    MediaServerLib
)

add_test(NAME MediaServerLegacyTest
         COMMAND MediaServerLegacyTest)
