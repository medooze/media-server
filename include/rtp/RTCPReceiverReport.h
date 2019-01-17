/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   RTCPReceiverReport.h
 * Author: Sergio
 *
 * Created on 3 de febrero de 2017, 12:00
 */

#ifndef RTCPRECEIVERREPORT_H
#define RTCPRECEIVERREPORT_H

#include "config.h"
#include "rtp/RTCPPacket.h"
#include "rtp/RTCPCommonHeader.h"
#include "rtp/RTCPReport.h"

class RTCPReceiverReport : public RTCPPacket
{
public:
	RTCPReceiverReport();
	RTCPReceiverReport(DWORD ssrc);
	virtual ~RTCPReceiverReport() = default;
	virtual void Dump();
	virtual DWORD GetSize();
	virtual DWORD Parse(const BYTE* data,DWORD size);
	virtual DWORD Serialize(BYTE* data,DWORD size);

	DWORD GetCount() const				{ return reports.size();	}
	RTCPReport::shared GetReport(BYTE i) const	{ return reports[i];		}
	void AddReport(const RTCPReport::shared& report){ reports.push_back(report);	}
private:
	DWORD ssrc	= 0;     /* receiver generating this report */

private:
	Reports	reports;
};
#endif /* RTCPRECEIVERREPORT_H */

