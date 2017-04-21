###########################################
# Fichero de configuracion
###########################################
include config.mk
OPTS+= -fPIC -DPIC -msse -msse2 -msse3 -DSPX_RESAMPLE_EXPORT= -DRANDOM_PREFIX=mcu -DOUTSIDE_SPEEX -DFLOATING_POINT -D__SSE2__ -Wno-narrowing

#DEBUG
ifeq ($(DEBUG),yes)
	TAG=debug
	OPTS+= -g -O0
	#SANITIZE
	ifeq ($(SANITIZE),yes)
		OPTS+= -fsanitize=address -fsanitize=leak -fno-omit-frame-pointer
		LDFLAGS+= -fsanitize=leak  -fsanitize=address
		SFULDFLAGS+= -fsanitize=leak  -fsanitize=address
	endif
else
	OPTS+= -g -O4 -fexpensive-optimizations -funroll-loops
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
DEPACKETIZERSOBJ=
G711DIR=g711
G711OBJ=g711.o pcmucodec.o pcmacodec.o

H263DIR=h263
H263OBJ=h263.o h263codec.o mpeg4codec.o h263-1996codec.o

JSR309DIR=jsr309
JSR309OBJ=RTPEndpoint.o Player.o Endpoint.o MediaSession.o JSR309Manager.o RTPMultiplexer.o xmlrpcjsr309.o AudioDecoderWorker.o AudioEncoderWorker.o AudioMixerResource.o VideoDecoderWorker.o VideoEncoderWorkerMultiplexer.o VideoMixerResource.o RTPMultiplexerSmoother.o Recorder.o VideoTranscoder.o

ifeq ($(FLV1PARSER),yes)
	FLV1DIR=flv1
	FLV1OBJ=flv1codec.o flv1Parser.o
	OPTS+= -DFLV1PARSER
else
	FLV1DIR=flv1
	FLV1OBJ=flv1codec.o
endif

H264DIR=h264
H264OBJ=h264encoder.o h264decoder.o 
DEPACKETIZERSOBJ+= h264depacketizer.o

VP6DIR=vp6
VP6OBJ=vp6decoder.o

VP8DIR=vp8
VP8OBJ=vp8encoder.o vp8decoder.o 
DEPACKETIZERSOBJ+= vp8depacketizer.o

VP9DIR=vp9
VP9OBJ=
DEPACKETIZERSOBJ+= VP9PayloadDescription.o VP9LayerSelector.o

GSMDIR=gsm
GSMOBJ=gsmcodec.o

SPEEXDIR=speex
SPEEXOBJ=speexcodec.o resample.o

NELLYDIR=nelly
NELLYOBJ=NellyCodec.o

OPUSDIR=opus
OPUSOBJ=opusdecoder.o opusencoder.o

G722DIR=g722
G722OBJ=g722codec.o

AACDIR=aac
AACOBJ=aacencoder.o

SFUDIR=sfu

COREOBJ=VideoEncoderWorker.o
COREDIR=core

BASE= rtp.o xmlrpcserver.o xmlhandler.o xmlstreaminghandler.o statushandler.o  dtls.o CPUMonitor.o OpenSSL.o RTPTransport.o  EventSource.o eventstreaminghandler.o httpparser.o   stunmessage.o crc32calc.o http.o avcdescriptor.o utf8.o   RTPStreamTransponder.o
BASE+= RTCPCommonHeader.o RTPHeader.o RTPHeaderExtension.o RTCPApp.o RTCPExtendedJitterReport.o RTCPPacket.o RTCPReport.o RTCPSenderReport.o RTPMap.o RTCPBye.o RTCPFullIntraRequest.o RTCPPayloadFeedback.o RTCPRTPFeedback.o RTPDepacketizer.o RTPPacket.o  RTCPCompoundPacket.o RTCPNACK.o RTCPReceiverReport.o RTCPSDES.o RTPPacketSched.o

OBJS=  rtpsession.o RTPSmoother.o  remoteratecontrol.o remoterateestimator.o xmlrpcsfu.o SFUManager.o Room.o SFUParticipant.o RTPBundleTransport.o DTLSICETransport.o audio.o video.o  $(BASE) $(COREOBJ) cpim.o  groupchat.o websocketserver.o websocketconnection.o  mcu.o rtpparticipant.o multiconf.o  rtmpparticipant.o  xmlrpcmcu.o  mp4player.o mp4streamer.o mp4recorder.o  audiostream.o videostream.o amf.o rtmpmessage.o rtmpchunk.o rtmpstream.o rtmpconnection.o  rtmpserver.o broadcaster.o broadcastsession.o rtmpflvstream.o flvrecorder.o FLVEncoder.o xmlrpcbroadcaster.o mediagateway.o mediabridgesession.o xmlrpcmediagateway.o textmixer.o textmixerworker.o textstream.o pipetextinput.o pipetextoutput.o  logo.o overlay.o audioencoder.o audiodecoder.o textencoder.o rtmpmp4stream.o rtmpnetconnection.o   rtmpclientconnection.o vad.o  uploadhandler.o  appmixer.o  videopipe.o framescaler.o sidebar.o mosaic.o partedmosaic.o asymmetricmosaic.o pipmosaic.o videomixer.o audiomixer.o audiotransrater.o pipeaudioinput.o pipeaudiooutput.o pipevideoinput.o pipevideooutput.o 
OBJS+= $(G711OBJ) $(H263OBJ) $(GSMOBJ)  $(H264OBJ) ${FLV1OBJ} $(SPEEXOBJ) $(NELLYOBJ) $(G722OBJ) $(JSR309OBJ) $(VADOBJ) $(VP6OBJ) $(VP8OBJ) $(VP9OBJ) $(OPUSOBJ) $(AACOBJ) $(DEPACKETIZERSOBJ)
TARGETS=mcu test 

ifeq ($(FLASHSTREAMER),yes)
	GNASHINCLUDE = -I$(GNASHBASE) -I$(GNASHBASE)/server -I$(GNASHBASE)/libbase -I$(GNASHBASE)/libgeometry -I$(GNASHBASE)/server/parser -I$(GNASHBASE)/server/vm -I$(GNASHBASE)/backend -I$(GNASHBASE)/libmedia -DFLASHSTREAMER
	GNASHLD =  -lgnashserver -lagg  -L$(GNASHLIBS)
	OBJS+= flash.o xmlrpcflash.o
	OBJSFS   = flashstreamer.o FlashPlayer.o FlashSoundHandler.o $(OBJS)
	OBJSFSCLIENT = xmlrpcclient.o xmlrpcflashclient.o
	TARGETS += flashstreamer flashclient testflash
endif

ifeq ($(VADWEBRTC),yes)
	VADINCLUDE = -I$(SRCDIR)/ext
	VADLD = $(SRCDIR)/ext/out/Release/obj/webrtc/common_audio/libcommon_audio.a
	OPTS+= -DVADWEBRTC
else
	VADINCLUDE =
	VADLD =
endif

OBJSSFU = sfu.o nocodecs.o xmlrpcsfu.o SFUManager.o Room.o SFUParticipant.o RTPBundleTransport.o DTLSICETransport.o $(DEPACKETIZERSOBJ) $(BASE) 
OBJSMCU = $(OBJS) main.o
OBJSLIB = $(OBJS)
OBJSTEST = $(OBJS) test/main.o test/test.o test/cpim.o test/rtp.o test/fec.o test/overlay.o test/vp9.o


BUILDOBJSMCU = $(addprefix $(BUILD)/,$(OBJSMCU))
BUILDOBJSSFU = $(addprefix $(BUILD)/,$(OBJSSFU))
BUILDOBJOBJSLIB = $(addprefix $(BUILD)/,$(OBJSLIB))
BUILDOBJSTEST= $(addprefix $(BUILD)/,$(OBJSTEST))


###################################
#Compilacion condicional
###################################
VPATH  =  %.o $(BUILD)/
VPATH +=  %.c $(SRCDIR)/lib/
VPATH +=  %.c $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/rtp
VPATH +=  %.cpp $(SRCDIR)/src/$(G711DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(GSMDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H263DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H264DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(FLV1DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(SPEEXDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(NELLYDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(G722DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(JSR309DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(SFUDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP6DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP8DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP9DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(OPUSDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(AACDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(COREDIR)


INCLUDE+= -I$(SRCDIR)/src -I$(SRCDIR)/include/ $(VADINCLUDE)
LDFLAGS+= -lgsm -lpthread -lsrtp2

ifeq ($(STATIC_OPENSSL),yes)
	INCLUDE+= -I$(OPENSSL_DIR)/include
	LDFLAGS+= $(OPENSSL_DIR)/libssl.a $(OPENSSL_DIR)/libcrypto.a
else
	LDFLAGS+= -lssl -lcrypto
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
	LDFLAGS+=/usr/local/lib/libmp4v2.a
else
	LDFLAGS+= -lavcodec -lswscale -lavformat -lavutil -lavresample  -lmp4v2 -lspeex -lvpx -lopus -lmp4v2 -lx264
endif

LDFLAGS+= -lxmlrpc -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_util -lnsl -lpthread -lz -ljpeg -lpng -lresolv -L/lib/i386-linux-gnu -lgcrypt

SFULDFLAGS+= -lpthread -lsrtp2 -lxmlrpc -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_util -lnsl -lpthread -lresolv -L/lib/i386-linux-gnu -lgcrypt -lssl -lcrypto
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

############################################
#Targets
############################################
all: touch mkdirs $(TARGETS) certs

touch:
	touch $(SRCDIR)/include/version.h
	svn propset builtime "`date`" $(SRCDIR)/include/version.h || true
mkdirs:
	mkdir -p $(BUILD)
	mkdir -p $(BUILD)/test
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

sfu: touch mkdirs $(OBJSSFU) certs
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSSFU) $(SFULDFLAGS)
	@echo [OUT] $(TAG) $(BIN)/$@

mcu: $(OBJSMCU)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSMCU) $(LDFLAGS) $(VADLD)
	@echo [OUT] $(TAG) $(BIN)/$@
	
buildtest: $(OBJSTEST)
	$(CXX) -o $(BIN)/test $(BUILDOBJSTEST) $(LDFLAGS) $(VADLD) 
	
test: buildtest
	$(BIN)/$@ -lavcodec
	
libmediamixer: $(OBJSLIB)
	gcc $(CXXFLAGS) -c lib/mediamixer.cpp -o $(BUILD)/mediamixer.o -DPIC -fPIC
	gcc -shared -o $(BIN)/$@.so $(BUILDOBJOBJSLIB) $(BUILD)/mediamixer.o $(LDFLAGS) $(VADLD) 

