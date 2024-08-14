#ifndef _RTMPSCLIENTPCONNECTION_H_
#define _RTMPSCLIENTPCONNECTION_H_

#include "rtmpclientconnection.h"
#include "TlsClient.h"

class RTMPSClientConnection :
	public RTMPClientConnection
{

public:
	RTMPSClientConnection(const std::wstring& tag);

protected:
	virtual RTMPClientConnection::ErrorCode Start() override;
	virtual void Stop() override;
	virtual bool IsConnectionReady() override;
	virtual void OnReadyToTransfer() override;
	virtual void processReceivedData(const uint8_t* data, size_t size) override;
	virtual void sendRtmpData(const uint8_t* data, size_t size) override;

private:
	TlsClient tls;
};

#endif
