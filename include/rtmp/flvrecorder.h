#ifndef _FLVRECORDER_H_
#define _FLVRECORDER_H_
#include "flv.h"
#include "recordercontrol.h"
#include "rtmpmessage.h"
#include "rtmpstream.h"

class FLVRecorder :
	public RecorderControl,
	public RTMPMediaStream::Listener
{
public:
	FLVRecorder();
	~FLVRecorder();

	//Recorder interface
	virtual bool Create(const char *filename);
	virtual bool Record(bool) { return Record(); }
	virtual bool Record();
	virtual bool Stop();
	virtual bool Close();

	virtual RecorderControl::Type GetType()	{ return RecorderControl::FLV;	}

	bool Write(RTMPMediaFrame *frame);
	bool Set(RTMPMetaData *meta);
	

	/* RTMPMediaStream listener interface*/
	virtual void onAttached(RTMPMediaStream *stream) {};
	virtual void onMediaFrame(DWORD id,RTMPMediaFrame *frame) { Write(frame);};
	virtual void onMetaData(DWORD id,RTMPMetaData *meta) { Set(meta); };
	virtual void onCommand(DWORD id,const wchar_t *name,AMFData* obj) {};
	virtual void onStreamBegin(DWORD id) {};
	virtual void onStreamEnd(DWORD id) {};
	//virtual void onStreamIsRecorded(DWORD id);
	virtual void onStreamReset(DWORD id) {};
	virtual void onDetached(RTMPMediaStream *stream){};
private:
	int	fd;
	DWORD	offset;
	bool	recording;
	QWORD 	first;
	QWORD	last;
	RTMPMetaData *meta;
};

#endif
