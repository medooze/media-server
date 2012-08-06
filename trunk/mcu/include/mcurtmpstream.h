#ifndef _MCURTMPSTREAM_H_
#define _MCURTMPSTREAM_H_
#include "config.h"
#include "rtmpstream.h"

class MCURTMPBroadcastStream : public RTMPStream
{
public:
	MCURTMPBroadcastStream(MCU *mcu)
	virtual bool Play(std::wstring& url);	
private:
	MCU* mcu;
};

#endif
