###########################################
# Fichero de configuracion
###########################################
include config.mk

#OPTS+= -fPIC -DPIC -DNDEBUG

#DEBUG
ifeq ($(DEBUG),yes)
	TAG=debug
	OPTS+= -g -O0 -DMCUDEBUG 
else
	OPTS+= -O3
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
JSR309OBJ=RTPEndpoint.o Player.o Endpoint.o MediaSession.o JSR309Manager.o RTPMultiplexer.o xmlrpcjsr309.o AudioDecoderWorker.o AudioEncoderWorker.o AudioMixerResource.o VideoDecoderWorker.o VideoEncoderWorker.o VideoMixerResource.o RTPMultiplexerSmoother.o Recorder.o VideoTranscoder.o

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

VP8DIR=vp8
VP8OBJ=vp8encoder.o vp8decoder.o

GSMDIR=gsm
GSMOBJ=gsmcodec.o

SPEEXDIR=speex
SPEEXOBJ=speexcodec.o

NELLYDIR=nelly
NELLYOBJ=NellyCodec.o

OPUSDIR=opus
OPUSOBJ=opusdecoder.o opusencoder.o

OBJS=audio.o video.o mcu.o multiconf.o rtpparticipant.o rtmpparticipant.o videomixer.o audiomixer.o xmlrpcserver.o xmlhandler.o xmlstreaminghandler.o statushandler.o xmlrpcmcu.o   rtpsession.o audiostream.o videostream.o pipeaudioinput.o pipeaudiooutput.o pipevideoinput.o pipevideooutput.o framescaler.o sidebar.o mosaic.o partedmosaic.o asymmetricmosaic.o pipmosaic.o logo.o overlay.o amf.o rtmpmessage.o rtmpchunk.o rtmpstream.o rtmpconnection.o  rtmpserver.o broadcaster.o broadcastsession.o rtmpflvstream.o flvrecorder.o FLVEncoder.o xmlrpcbroadcaster.o mediagateway.o mediabridgesession.o xmlrpcmediagateway.o textmixer.o textmixerworker.o textstream.o pipetextinput.o pipetextoutput.o mp4player.o mp4streamer.o audioencoder.o audiodecoder.o textencoder.o mp4recorder.o rtmpmp4stream.o rtmpnetconnection.o avcdescriptor.o RTPSmoother.o rtp.o rtmpclientconnection.o vad.o stunmessage.o crc32calc.o remoteratecontrol.o remoterateestimator.o uploadhandler.o http.o appmixer.o fecdecoder.o videopipe.o
OBJS+= $(G711OBJ) $(H263OBJ) $(GSMOBJ)  $(H264OBJ) ${FLV1OBJ} $(SPEEXOBJ) $(NELLYOBJ) $(JSR309OBJ) $(VADOBJ) $(VP8OBJ) $(OPUSOBJ)
TARGETS=mcu rtmpdebug

ifeq ($(FLASHSTREAMER),yes)
	GNASHINCLUDE = -I$(GNASHBASE) -I$(GNASHBASE)/server -I$(GNASHBASE)/libbase -I$(GNASHBASE)/libgeometry -I$(GNASHBASE)/server/parser -I$(GNASHBASE)/server/vm -I$(GNASHBASE)/backend -I$(GNASHBASE)/libmedia -DFLASHSTREAMER 
	GNASHLD =  -lgnashserver -lagg  -L$(GNASHLIBS) 
	OBJS+= flash.o xmlrpcflash.o 
	OBJSFS   = flashstreamer.o FlashPlayer.o FlashSoundHandler.o $(OBJS)
	OBJSFSCLIENT = xmlrpcclient.o xmlrpcflashclient.o
	TARGETS += flashstreamer flashclient testflash
endif

ifeq ($(VADWEBRTC),yes)
	VADINCLUDE = -I$(WEBRTCINCLUDE) -I$(WEBRTCINCLUDE)/webrtc
	VADLD = $(WEBRTDIROBJ)/common_audio/libvad.a $(WEBRTDIROBJ)/common_audio/libsignal_processing.a
	OPTS+= -DVADWEBRTC
else
	VADINCLUDE =
	VADLD =
endif

OBJSMCU = $(OBJS) main.o
OBJSLIB = $(OBJS)
OBJSMCUCLIENT = xmlrpcclient.o xmlrpcmcuclient.o
OBJSRTMPDEBUG = $(OBJS) rtmpdebug.o

BUILDOBJSMCU = $(addprefix $(BUILD)/,$(OBJSMCU))
BUILDOBJOBJSLIB = $(addprefix $(BUILD)/,$(OBJSLIB))
BUILDOBJSMCUCLIENT= $(addprefix $(BUILD)/,$(OBJSMCUCLIENT))
BUILDOBJSRTMPDEBUG= $(addprefix $(BUILD)/,$(OBJSRTMPDEBUG))
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
VPATH +=  %.cpp $(SRCDIR)/src/$(JSR309DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(VP8DIR)
VPATH +=  %.cpp $(SRCDIR)/src/$(OPUSDIR)


INCLUDE+= -I$(SRCDIR)/include/ $(VADINCLUDE)
LDFLAGS+= -lavcodec -lgsm -lpthread -lswscale -lavformat -lavutil -lx264 -lssl -lmp4v2 -lspeex -lspeexdsp -lcrypto -lsrtp -lvpx -lopus
LDXMLFLAGS+= -lxmlrpc -lxmlrpc_xmlparse -lxmlrpc_xmltok -lxmlrpc_abyss -lxmlrpc_server -lxmlrpc_util
LDFLAGS+= $(LDXMLFLAGS)

#For abyss
OPTS 	+= -D_UNIX -D__STDC_CONSTANT_MACROS
CFLAGS  += $(INCLUDE) $(OPTS)
CXXFLAGS+= $(INCLUDE) $(OPTS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $(BUILD)/$@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $(BUILD)/$@


############################################
#Targets
############################################
all: touch mkdirs $(TARGETS)

touch:
	touch $(SRCDIR)/include/version.h
	svn propset builtime "`date`" $(SRCDIR)/include/version.h
mkdirs:  
	mkdir -p $(BUILD)
	mkdir -p $(BIN)
	cp $(SRCDIR)/logo.png $(BIN)
clean:
	rm -f $(BUILDOBJSMCU)
	rm -f $(BUILDOBJSFS)
	rm -f "$(BIN)/mcu"
	rm -f "$(BIN)/flashstreamer"

install:
	mkdir -p  $(TARGET)/lib
	mkdir -p  $(TARGET)/include/mcu
	

############################################
#MCU
############################################


mcu: $(OBJSMCU)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSMCU) $(LDFLAGS) $(VADLD)

rtmpdebug: $(OBJSRTMPDEBUG)
	$(CXX) -o $(BIN)/$@ $(BUILDOBJSRTMPDEBUG) $(LDFLAGS) $(VADLD)

libmediamixer: $(OBJSLIB)
	gcc $(CXXFLAGS) -c lib/mediamixer.cpp -o $(BUILD)/mediamixer.o -DPIC -fPIC
	gcc -shared -o $(BIN)/$@.so $(BUILDOBJOBJSLIB) $(BUILD)/mediamixer.o $(LDFLAGS) $(VADLD)
flashstreamer: $(OBJSFS) $(OBJS)
	g++ -o $(BIN)/$@ $(BUILDOBJSFS) $(BUILDOBJS) $(GNASHBASE)/backend/.libs/libgnashagg.a /usr/lib/libagg.a $(LDFLAGS) $(GNASHLD)

