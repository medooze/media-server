###########################################
# Fichero de configuracion
###########################################
include config.mk

OPTS+= -fPIC -DPIC -msse -msse2 -msse3 -DSPX_RESAMPLE_EXPORT= -DRANDOM_PREFIX=mcu -DOUTSIDE_SPEEX -DFLOATING_POINT -D__SSE2__

#DEBUG
ifeq ($(DEBUG),yes)
	TAG=debug
	OPTS+= -g -O0
	#SANITIZE
	ifeq ($(SANITIZE),yes)
		OPTS+= -fsanitize=leak -fno-omit-frame-pointer
		LDFLAGS+= -fsanitize=leak
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
H264OBJ=h264encoder.o h264decoder.o h264depacketizer.o

VP6DIR=vp6
VP6OBJ=vp6decoder.o

VP8DIR=vp8
VP8OBJ=vp8encoder.o vp8decoder.o

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

VNCDIR=vnc
VNCOBJ=VNCViewer.o VNCServer.o server.o rfbproto.o cursor.o minilzo.o tls_none.o rfbserver.o rfbregion.o scale.o ultra.o corre.o hextile.o draw.o font.o rre.o selbox.o stats.o tight.o translate.o zlib.o zrle.o zrleoutstream.o zrlepalettehelper.o zywrletemplate.o turbojpeg.o auth.o server_cursor.o vncauth.o d3des.o md5.o sha1.o cargs.o

BFCPOBJ=\
	bfcp.o  \
	BFCPFloorControlServer.o  \
	BFCPUser.o  \
	BFCPFloorRequest.o  \
	BFCPMessage.o  \
	BFCPAttribute.o  \
	BFCPMsgFloorQuery.o  \
	BFCPMsgFloorRelease.o  \
	BFCPMsgFloorRequest.o  \
	BFCPMsgHello.o  \
	BFCPMsgError.o  \
	BFCPMsgFloorRequestStatus.o  \
	BFCPMsgFloorStatus.o  \
	BFCPAttrBeneficiaryId.o  \
	BFCPAttrFloorId.o  \
	BFCPAttrFloorRequestId.o  \
	BFCPAttrParticipantProvidedInfo.o  \
	BFCPAttrErrorCode.o  \
	BFCPAttrErrorInfo.o  \
	BFCPAttrFloorRequestInformation.o  \
	BFCPAttrOverallRequestStatus.o  \
	BFCPAttrFloorRequestStatus.o  \
	BFCPAttrRequestStatus.o  \
	BFCPAttrBeneficiaryInformation.o  \
	BFCPAttrRequestedByInformation.o  \
	BFCPAttrStatusInfo.o
BFCPDIR=bfcp

COREOBJ=VideoEncoderWorker.o
COREDIR=core

OBJS=  $(COREOBJ) $(BFCPOBJ) $(VNCOBJ) cpim.o  groupchat.o httpparser.o websocketserver.o websocketconnection.o audio.o video.o mcu.o rtpparticipant.o multiconf.o  rtmpparticipant.o videomixer.o audiomixer.o xmlrpcserver.o xmlhandler.o xmlstreaminghandler.o statushandler.o xmlrpcmcu.o   rtpsession.o audiostream.o videostream.o audiotransrater.o pipeaudioinput.o pipeaudiooutput.o pipevideoinput.o pipevideooutput.o framescaler.o sidebar.o mosaic.o partedmosaic.o asymmetricmosaic.o pipmosaic.o logo.o overlay.o amf.o rtmpmessage.o rtmpchunk.o rtmpstream.o rtmpconnection.o  rtmpserver.o broadcaster.o broadcastsession.o rtmpflvstream.o flvrecorder.o FLVEncoder.o xmlrpcbroadcaster.o mediagateway.o mediabridgesession.o xmlrpcmediagateway.o textmixer.o textmixerworker.o textstream.o pipetextinput.o pipetextoutput.o mp4player.o mp4streamer.o audioencoder.o audiodecoder.o textencoder.o mp4recorder.o rtmpmp4stream.o rtmpnetconnection.o avcdescriptor.o RTPSmoother.o rtp.o rtmpclientconnection.o vad.o stunmessage.o crc32calc.o remoteratecontrol.o remoterateestimator.o uploadhandler.o http.o appmixer.o fecdecoder.o videopipe.o eventstreaminghandler.o dtls.o CPUMonitor.o OpenSSL.o
OBJS+= $(G711OBJ) $(H263OBJ) $(GSMOBJ)  $(H264OBJ) ${FLV1OBJ} $(SPEEXOBJ) $(NELLYOBJ) $(G722OBJ) $(JSR309OBJ) $(VADOBJ) $(VP6OBJ) $(VP8OBJ) $(OPUSOBJ) $(AACOBJ)
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

OBJSMCU = $(OBJS) main.o
OBJSLIB = $(OBJS)
OBJSTEST = $(OBJS) test/main.o test/test.o test/cpim.o
OBJSRTMPDEBUG = $(OBJS) rtmpdebug.o
OBJSFLVDUMP = $(OBJS) flvdump.o

BUILDOBJSMCU = $(addprefix $(BUILD)/,$(OBJSMCU))
BUILDOBJOBJSLIB = $(addprefix $(BUILD)/,$(OBJSLIB))
BUILDOBJSTEST= $(addprefix $(BUILD)/,$(OBJSTEST))
BUILDOBJSRTMPDEBUG= $(addprefix $(BUILD)/,$(OBJSRTMPDEBUG))
BUILDOBJSFLVDUMP= $(addprefix $(BUILD)/,$(OBJSFLVDUMP))
BUILDOBJSFS= $(addprefix $(BUILD)/,$(OBJSFS))
BUILDOBJSFSCLIENT= $(addprefix $(BUILD)/,$(OBJSFSCLIENT))

###################################
#Compilacion condicional
###################################
VPATH  =  %.o $(BUILD)/
VPATH +=  %.c $(SRCDIR)/lib/
VPATH +=  %.c $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/
VPATH +=  %.cpp $(SRCDIR)/src/$(G711DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(GSMDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H263DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(H264DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(FLV1DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(SPEEXDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(NELLYDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(G722DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(JSR309DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP6DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP8DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(OPUSDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(AACDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VNCDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VNCDIR)/libvncserver
VPATH +=  %.cpp $(SRCDIR)/src/$(VNCDIR)/libvncclient
VPATH +=  %.cpp $(SRCDIR)/src/$(VNCDIR)/common
VPATH +=  %.cpp $(SRCDIR)/src/$(BFCPDIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(BFCPDIR)/attributes
VPATH +=  %.cpp $(SRCDIR)/src/$(BFCPDIR)/messages
VPATH +=  %.cpp $(SRCDIR)/src/$(COREDIR)



INCLUDE+= -I$(SRCDIR)/include/ $(VADINCLUDE) -I$(SRCDIR)/src/vnc/common -I$(SRCDIR)/src/vnc/libvncserver
LDFLAGS+= -lgsm -lpthread -lsrtp

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
	LDFLAGS+= -lavcodec -lswscale -lavformat -lavutil -lavresample -lx264 -lmp4v2 -lspeex -lvpx -lopus
endif

LDFLAGS+= -lxmlrpc -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_util -lnsl -lpthread -lz -ljpeg -lpng -lresolv -L/lib/i386-linux-gnu -lgcrypt

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
	mkdir -p $(BUILD)/libvncserver
	mkdir -p $(BIN)
ifeq ($(wildcard $(BIN)/logo.png), )
	cp $(SRCDIR)/logo.png $(BIN)
endif
clean:
	rm -f $(BUILDOBJSMCU)
	rm -f $(BUILDOBJSFS)
	rm -f $(BUILDOBJSTEST)
	rm -f "$(BIN)/mcu"
	rm -f "$(BIN)/flashstreamer"

install:
	mkdir -p  $(TARGET)/libA
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
	@$(CXX) -o $(BIN)/$@ $(BUILDOBJSMCU) $(LDFLAGS) $(VADLD)
	@echo [OUT] $(TAG) $(BIN)/$@
	
buildtest: $(OBJSTEST)
	$(CXX) -o $(BIN)/test $(BUILDOBJSTEST) $(LDFLAGS) $(VADLD)
	
test: buildtest
	$(BIN)/$@

rtmpdebug: $(OBJSRTMPDEBUG)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSRTMPDEBUG) $(LDFLAGS) $(VADLD)

flvdump: $(OBJSFLVDUMP)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSFLVDUMP) $(LDFLAGS) $(VADLD)


libmediamixer: $(OBJSLIB)
	gcc $(CXXFLAGS) -c lib/mediamixer.cpp -o $(BUILD)/mediamixer.o -DPIC -fPIC
	gcc -shared -o $(BIN)/$@.so $(BUILDOBJOBJSLIB) $(BUILD)/mediamixer.o $(LDFLAGS) $(VADLD)
flashstreamer: $(OBJSFS) $(OBJS)
	g++ -o $(BIN)/$@ $(BUILDOBJSFS) $(BUILDOBJS) $(GNASHBASE)/backend/.libs/libgnashagg.a /usr/lib/libagg.a $(LDFLAGS) $(GNASHLD)

