###########################################
# Fichero de configuracion
###########################################
include config.mk
OPTS+= -fPIC -DPIC -msse -msse2 -msse3 -msse4.1 -DSPX_RESAMPLE_EXPORT= -DRANDOM_PREFIX=mcu -DOUTSIDE_SPEEX -DFLOATING_POINT -D__SSE2__ -Wno-narrowing -std=c++17 -std=gnu++17 -march=native

#GET OS
OS = $(shell uname -s)
PLATFORM = $(shell uname -p)

ifeq ($(OS),"Linux")
OPTS+=-DHAVE_STD_ALIGNED_ALLOC -DLINUX
else
OPTS+=-DDARWIN
endif

#DEBUG
ifeq ($(DEBUG),yes)
	TAG=debug
	OPTS+= -g -O0
	#SANITIZE
	ifeq ($(SANITIZE),yes)
		OPTS+= -fsanitize=address -fsanitize=leak -fsanitize=undefined -fno-omit-frame-pointer
		SANITIZEFLAGS+= -fsanitize=address -fsanitize=leak -fsanitize=undefined 
		LDFLAGS += -fsanitize=address -fsanitize=leak -fsanitize=undefined 
	endif
else
	OPTS+= -g -O3
	TAG=release
endif

#LOG
ifeq ($(LOG),yes)
	OPTS+= -DLOG_
endif


############################################
#Directorios
############################################
BUILD = $(SRCDIR)/build/$(TAG)
BIN   = $(SRCDIR)/bin/$(TAG)

############################################
#Objetos
############################################
DEPACKETIZERSOBJ= RTPDepacketizer.o DependencyDescriptorLayerSelector.o
G711DIR=g711
G711OBJ=g711.o pcmucodec.o pcmacodec.o

H263DIR=h263
H263OBJ=h263.o h263codec.o mpeg4codec.o h263-1996codec.o


FLV1DIR=flv1
FLV1OBJ=flv1codec.o

H264DIR=h264
H264OBJ=h264encoder.o h264decoder.o 
DEPACKETIZERSOBJ+= h264depacketizer.o H264LayerSelector.o

VP6DIR=vp6
VP6OBJ=vp6decoder.o

VP8DIR=vp8
VP8OBJ=vp8encoder.o vp8decoder.o
DEPACKETIZERSOBJ+= vp8depacketizer.o VP8LayerSelector.o

VP9DIR=vp9
VP9OBJ=VP9Decoder.o
DEPACKETIZERSOBJ+= VP9PayloadDescription.o VP9LayerSelector.o VP9Depacketizer.o 

GSMDIR=gsm
GSMOBJ=gsmcodec.o

SPEEXDIR=speex
SPEEXOBJ=speexcodec.o resample.o

NELLYDIR=nelly
NELLYOBJ=NellyCodec.o

OPUSDIR=opus
OPUSOBJ=opusdecoder.o opusencoder.o
DEPACKETIZERSOBJ+= opusdepacketizer.o

G722DIR=g722
G722OBJ=g722codec.o g722_decode.o g722_encode.o

AACDIR=aac
AACOBJ=aacencoder.o aacdecoder.o

RTP=  LayerInfo.o RTPMap.o  RTPPacket.o RTPPayload.o RTPPacketSched.o  RTPLostPackets.o RTPSource.o RTPHeader.o RTPHeaderExtension.o DependencyDescriptor.o
RTCP= RTCPCompoundPacket.o RTCPNACK.o RTCPReceiverReport.o RTCPCommonHeader.o  RTCPApp.o RTCPExtendedJitterReport.o RTCPPacket.o RTCPReport.o RTCPSenderReport.o RTCPBye.o RTCPFullIntraRequest.o RTCPPayloadFeedback.o RTCPRTPFeedback.o RTCPSDES.o 
CORE= RTPIncomingMediaStreamDepacketizer.o RTPIncomingMediaStreamMultiplexer.o RTPIncomingSource.o RTPIncomingSourceGroup.o RTPOutgoingSource.o RTPOutgoingSourceGroup.o RTPSmoother.o SRTPSession.o dtls.o OpenSSL.o RTPTransport.o  stunmessage.o crc32calc.o http.o httpparser.o avcdescriptor.o utf8.o rtpsession.o RTPStreamTransponder.o VideoLayerSelector.o remoteratecontrol.o remoterateestimator.o RTPBundleTransport.o DTLSICETransport.o PCAPFile.o PCAPReader.o PCAPTransportEmulator.o ActiveSpeakerDetector.o EventLoop.o Datachannels.o crc32c.o crc32c_sse42.o crc32c_portable.o MediaFrameListenerBridge.o SendSideBandwidthEstimation.o
MP4= mp4streamer.o mp4recorder.o mp4player.o

RTMP= rtmpparticipant.o amf.o rtmpmessage.o rtmpchunk.o rtmpstream.o rtmpconnection.o  rtmpserver.o  rtmpflvstream.o flvrecorder.o flvencoder.o rtmppacketizer.o

OBJS= xmlrpcserver.o xmlhandler.o xmlstreaminghandler.o statushandler.o CPUMonitor.o   EventSource.o eventstreaminghandler.o  AudioCodecFactory.o VideoCodecFactory.o cpim.o  groupchat.o websocketserver.o websocketconnection.o  mcu.o rtpparticipant.o multiconf.o    xmlrpcmcu.o    audiostream.o videostream.o  textmixer.o textmixerworker.o textstream.o pipetextinput.o pipetextoutput.o  logo.o overlay.o VideoEncoderWorker.o audioencoder.o audiodecoder.o textencoder.o rtmpmp4stream.o rtmpnetconnection.o   rtmpclientconnection.o vad.o  uploadhandler.o  appmixer.o  videopipe.o framescaler.o sidebar.o mosaic.o partedmosaic.o asymmetricmosaic.o pipmosaic.o videomixer.o audiomixer.o audiotransrater.o pipeaudioinput.o pipeaudiooutput.o pipevideoinput.o pipevideooutput.o broadcastsession.o  AudioPipe.o
OBJS+= ${CORE} ${RTP} ${RTCP} ${RTMP} $(G711OBJ) $(H263OBJ) $(GSMOBJ)  $(H264OBJ) ${FLV1OBJ} $(SPEEXOBJ) $(NELLYOBJ) $(G722OBJ)  $(VADOBJ) $(VP6OBJ) $(VP8OBJ) $(VP9OBJ) $(OPUSOBJ) $(AACOBJ) $(DEPACKETIZERSOBJ) $(MP4)
TARGETS=mcu test

ifeq ($(VADWEBRTC),yes)
	VADINCLUDE = -I$(SRCDIR)/ext
	VADLD = $(SRCDIR)/ext/out/Release/obj/webrtc/common_audio/libcommon_audio.a
	OPTS+= -DVADWEBRTC
else
	VADINCLUDE =
	VADLD =
endif

OBJSMCU = $(OBJS) main.o
OBJSBASE = ${CORE} ${RTP} ${RTCP} $(DEPACKETIZERSOBJ) 
OBJSLIB = ${CORE} ${RTP} ${RTCP} $(DEPACKETIZERSOBJ) $(MP4)
OBJSTEST = $(OBJS) test/main.o test/test.o test/ddls.o test/dd.o test/h264.o test/aac.o test/cpim.o test/rtp.o test/fec.o test/overlay.o test/vp8.o test/vp9.o test/stun.o test/rtmp.o
OBJSFUZZ = ${RTP} ${RTCP} fuzz/fuzz.o


BUILDOBJS = $(addprefix $(BUILD)/,$(OBJS))
BUILDOBJSMCU = $(addprefix $(BUILD)/,$(OBJSMCU))
BUILDOBJSBASE  = $(addprefix $(BUILD)/,$(OBJSBASE))
BUILDOBJOBJSLIB = $(addprefix $(BUILD)/,$(OBJSLIB))
BUILDOBJSTEST= $(addprefix $(BUILD)/,$(OBJSTEST))
BUILDOBJSFUZZ= $(addprefix $(BUILD)/,$(OBJSFUZZ))


###################################
#Compilacion condicional
###################################
VPATH  =  %.o $(BUILD)/
VPATH +=  %.c $(SRCDIR)/lib/
VPATH +=  %.c $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/rtp
VPATH +=  %.cpp $(SRCDIR)/src/pcc
VPATH +=  %.cpp $(SRCDIR)/src/rtmp
VPATH +=  %.cpp $(SRCDIR)/src/ws
VPATH +=  %.cpp $(SRCDIR)/src/$(G711DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(GSMDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H263DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H264DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(FLV1DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(SPEEXDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(NELLYDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(G722DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP6DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP8DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP9DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(OPUSDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(AACDIR)
VPATH +=  %.cpp $(SRCDIR)/ext/libdatachannels/src
VPATH +=  %.cc  $(SRCDIR)/ext/crc32c/src/


INCLUDE+= -I/usr/local/include -I$(SRCDIR)/src -I$(SRCDIR)/include/ $(VADINCLUDE) -I$(SRCDIR)/ext/libdatachannels/src -I$(SRCDIR)/ext/libdatachannels/src/internal -I$(SRCDIR)/ext/crc32c/include -I$(SRCDIR)/ext/crc32c/config/${OS}-${PLATFORM}/

ifeq ($(STATIC_OPENSSL),yes)
	INCLUDE+= -I$(OPENSSL_DIR)/include
	LDFLAGS+= $(OPENSSL_DIR)/libssl.a $(OPENSSL_DIR)/libcrypto.a
	ARLIBFLAGS+= $(OPENSSL_DIR)/libssl.a $(OPENSSL_DIR)/libcrypto.a
else
	LDFLAGS+= -lssl -lcrypto
	LDLIBFLAGS+= -lssl -lcrypto
endif

ifeq ($(STATIC_LIBSRTP),yes)
	INCLUDE+= -I$(LIBSRTP_DIR)/include
	LDFLAGS+= $(LIBSRTP_DIR)/libsrtp2.a
	ARLIBFLAGS+= $(LIBSRTP_DIR)/libsrtp2.a
else
	LDFLAGS+= -lsrtp2
	LDLIBFLAGS+= -lsrtp2
endif

ifeq ($(STATIC_LIBMP4),yes)
	INCLUDE+= -I$(LIBMP4_DIR)/include
	LDFLAGS+= $(LIBMP4_DIR)/.libs/libmp4v2.a
	ARLIBFLAGS+= $(LIBMP4_DIR)/.libs/libmp4v2.a
else
	LDFLAGS+= -lmp4v2
	LDLIBFLAGS+= -lmp4v2
endif

ifeq ($(IMAGEMAGICK),yes)
	OPTS+=-DHAVE_IMAGEMAGICK `pkg-config --cflags ImageMagick++`
	LDFLAGS+=`pkg-config --libs ImageMagick++`
endif


ifeq ($(STATIC),yes)
	LDFLAGS+=/usr/local/src/ffmpeg/libavformat/libavformat.a
	LDFLAGS+=/usr/local/src/ffmpeg/libavcodec/libavcodec.a
	LDFLAGS+=/usr/local/src/ffmpeg/libavresample/libavresample.a
	LDFLAGS+=/usr/local/src/ffmpeg/libswscale/libswscale.a
	LDFLAGS+=/usr/local/src/ffmpeg/libavutil/libavutil.a
	LDFLAGS+=/usr/local/src/x264/libx264.a
	LDFLAGS+=/usr/local/src/opus-1.0.2/.libs/libopus.a
	LDFLAGS+=/usr/local/src/speex-1.2rc1/libspeex/.libs/libspeex.a
	LDFLAGS+=/usr/local/src/libvpx/libvpx.a
	LDFLAGS+=/usr/local/lib/
else
	LDFLAGS+= -lavcodec -lswscale -lavformat -lavutil -lavresample  -lspeex -lvpx -lopus  -lx264 
endif

LDFLAGS+= -lgsm -lxmlrpc -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_util -lnsl -lz -ljpeg -lpng -lresolv -L/lib/i386-linux-gnu -lgcrypt -lpthread -ldl
LDLIBFLAGS+= -lpthread

#For abyss
OPTS 	+= -D_UNIX -D__STDC_CONSTANT_MACROS
CFLAGS  += $(INCLUDE) $(OPTS)
CXXFLAGS+= $(INCLUDE) $(OPTS) 

%.o: %.c
	@echo "[CC ] $(TAG) $<"
	@gcc $(CFLAGS) -c $< -o $(BUILD)/$@

%.o: %.cpp
	@echo "[CXX] $(TAG) $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $(BUILD)/$@

%.o: %.cc
	@echo "[CXX] $(TAG) $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $(BUILD)/$@
	
############################################
#Targets
############################################
all: touch mkdirs $(TARGETS) certs

touch:
	@touch $(SRCDIR)/include/version.h
	@(git log -1 --pretty=tformat:"#ifndef VERSION_H%n#define VERSION_H%n#define MCUVERSION \"%h\"%n#define MCUDATE \"%cd\"%n#endif%n" | sed 's/\\r\\n/\r\n/g' >  $(SRCDIR)/include/version.h) || true
mkdirs:
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/test
	mkdir -p $(BUILD)/fuzz
	mkdir -p $(BIN)
ifeq ($(wildcard $(BIN)/logo.png), )
	cp $(SRCDIR)/logo.png $(BIN)
endif
clean:
	rm -f $(BUILDOBJSMCU)
	rm -f $(BUILDOBJSTEST)
	rm -f "$(BIN)/mcu"
install:
	mkdir -p  $(TARGET)/lib
	mkdir -p  $(TARGET)/include/mcu

certs:
ifeq ($(wildcard $(BIN)/mcu.crt), )
	@echo "Generating DTLS certificate files"
	@openssl req -sha256 -days 3650 -newkey rsa:1024 -nodes -new -x509 -keyout $(BIN)/mcu.key -out $(BIN)/mcu.crt
endif

############################################
#MCU
############################################

mcu: $(OBJSMCU)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSMCU) $(LDFLAGS) $(VADLD)
	@echo [OUT] $(TAG) $(BIN)/$@
	
buildtest: touch mkdirs $(OBJSTEST)
	$(CXX) -o $(BIN)/test $(BUILDOBJSTEST) $(LDFLAGS) $(VADLD) 

buildfuzz: touch mkdirs $(OBJSFUZZ)
	$(CXX) -o $(BIN)/fuzz $(BUILDOBJSFUZZ) $(SANITIZEFLAGS)
	
test: buildtest
	$(BIN)/$@ -lavcodec

fuzz: buildfuzz
	$(BIN)/$@ 

bwe: bwe.o $(OBJSBASE) 
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSBASE) $(LDLIBFLAGS) $(addprefix $(BUILD)/,$@.o)
	
sender:  sender.o $(OBJSBASE) 
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSBASE) $(LDLIBFLAGS) $(addprefix $(BUILD)/,$@.o)
	
receiver: receiver.o $(OBJSBASE) 
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSBASE) $(LDLIBFLAGS) $(addprefix $(BUILD)/,$@.o)
	
libmediaserver.so: touch mkdirs $(OBJSLIB)
	$(CXX) -shared -o $(BIN)/$@ $(BUILDOBJOBJSLIB) $(LDLIBFLAGS)
	@echo [OUT] $(TAG) $(BIN)/$@

libmediaserver.a: touch mkdirs $(OBJSLIB)
	${AR} rsc  $(BIN)/$@ $(BUILDOBJOBJSLIB) 
	@echo [OUT] $(TAG) $(BIN)/$@
 
libmediamixer.a: touch mkdirs $(OBJS)
	${AR} rsc  $(BIN)/$@ $(BUILDOBJS)
	@echo [OUT] $(TAG) $(BIN)/$@
