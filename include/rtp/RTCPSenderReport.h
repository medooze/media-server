/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPSenderReport.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 11:59
 */

#ifndef RTCPSENDERREPORT_H
#define RTCPSENDERREPORT_H
#include "config.h"
#include "rtp/LayerInfo.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPReport.h"
#include "rtp/RTCPCommonHeader.h"
#include <vector>
#include <memory>
#include <math.h>

class RTCPSenderReport : public RTCPPacket
{
public:
	using shared = std::shared_ptr<RTCPSenderReport>;
public:
	RTCPSenderReport();
	virtual ~RTCPSenderReport() = default;
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	void SetOctectsSent(DWORD octectsSent)	{ this->octectsSent = octectsSent;	}
	void SetPacketsSent(DWORD packetsSent)	{ this->packetsSent = packetsSent;	}
	void SetRtpTimestamp(DWORD rtpTimestamp){ this->rtpTimestamp = rtpTimestamp;	}
	void SetSSRC(DWORD ssrc)		{ this->ssrc = ssrc;			}
	void SetNTPFrac(DWORD ntpFrac)		{ this->ntpFrac = ntpFrac;		}
	void SetNTPSec(DWORD ntpSec)		{ this->ntpSec = ntpSec;		}

	DWORD GetOctectsSent()	const		{ return octectsSent;			}
	DWORD GetPacketsSent()	const		{ return packetsSent;			}
	DWORD GetRTPTimestamp() const		{ return rtpTimestamp;			}
	DWORD GetNTPFrac()	const		{ return ntpFrac;			}
	DWORD GetNTPSec()	const		{ return ntpSec;			}
	QWORD GetNTPTimestamp()	const		{ return ((QWORD)ntpSec)<<32 | ntpFrac; }
	DWORD GetSSRC()		const		{ return ssrc;				}

	DWORD GetCount()	const				{ return reports.size();		}
	RTCPReport::shared GetReport(BYTE i) const		{ return reports[i];			}
	void  AddReport(const RTCPReport::shared& report)	{ reports.push_back(report);		}

	void  SetTimestamp(QWORD time);
	QWORD GetTimestamp() const;

private:
	DWORD ssrc		= 0; /* sender generating this report */
	DWORD ntpSec		= 0; /* NTP timestamp */
	DWORD ntpFrac		= 0;
	DWORD rtpTimestamp	= 0; /* RTP timestamp */
	DWORD packetsSent	= 0; /* packets sent */
	DWORD octectsSent	= 0; /* octets sent */

	Reports	reports;
};


#endif /* RTCPSENDERREPORT_H */

