#ifndef _RTMPSCLIENTPCONNECTION_H_
#define _RTMPSCLIENTPCONNECTION_H_

#include "rtmpclientconnection.h"
#include "TlsClient.h"

class RTMPSClientConnection :
	public RTMPClientConnection
{

public:
	RTMPSClientConnection(const std::wstring& tag);
	virtual int Disconnect() override;

protected:
	virtual RTMPClientConnection::ErrorCode Start() override;
	virtual bool IsConnectionReady() override;
	virtual void OnReadyToTransfer() override;
	virtual void ProcessReceivedData(const uint8_t* data, size_t size) override;
	virtual void AddPendingRtmpData(const uint8_t* data, size_t size) override;

private:
	TlsClient tls;
};

#endif
