#ifndef _RTMPSCLIENTPCONNECTION_H_
#define _RTMPSCLIENTPCONNECTION_H_

#include "rtmpclientconnection.h"
#include "TlsClient.h"

class RTMPSClientConnection :
	public RTMPClientConnection
{

public:
	RTMPSClientConnection(const std::wstring& tag);

	ErrorCode Connect(const char* server, int port, const char* app, RTMPClientConnection::Listener* listener) override;
	
protected:
	virtual bool IsConnectionReady() override;
	virtual void OnReadyToTransfer() override;
	virtual void ProcessReceivedData(const uint8_t* data, size_t size) override;
	virtual void AddPendingRtmpData(const uint8_t* data, size_t size) override;

	virtual void OnLoopExit(int exitCode) override;
private:
	TlsClient tls;
};

#endif
