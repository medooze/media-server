#include "rtmp/rtmpsclientconnection.h"


RTMPSClientConnection::RTMPSClientConnection(const std::wstring& tag) :
	RTMPClientConnection(tag)
{
	
}

RTMPClientConnection::ErrorCode RTMPSClientConnection::Start()
{
	if (!tls.Initialize())
	{
		Error("Failed to initialise tls.\n");
		return RTMPClientConnection::ErrorCode::TlsInitError;
	}
	
	// Start handshake
	if (tls.Handshake() == TlsClient::SslStatus::Failed)
	{
		return RTMPClientConnection::ErrorCode::TlsHandshakeError;
	}
	
	return RTMPClientConnection::Start();
}

int RTMPSClientConnection::Disconnect()
{
	RTMPClientConnection::Disconnect();
	
	tls.Shutdown();
}

bool RTMPSClientConnection::IsConnectionReady()
{
	return tls.IsInitialised();
}

void RTMPSClientConnection::OnReadyToTransfer()
{
	bool result = tls.ReadEncrypted([this](const uint8_t* data, size_t size){
		WriteData(data, size);
	});
	
	if (!result)
	{
		Error("-RTMPSClientConnection::OnReadyToTransfer() Failed to read TLS encrypted data.");
	}
}

void RTMPSClientConnection::ProcessReceivedData(const uint8_t* data, size_t size)
{
	auto listener = GetListener();
	
	auto ret = tls.Decrypt(data, size, [this](const uint8_t* data, size_t size){
		ParseData(data, size);
	});
	
	switch(ret) 
	{
	case TlsClient::TlsClientError::NoError:
		break;
	case TlsClient::TlsClientError::HandshakeFailed:
		Warning("-RTMPClientConnection::ProcessReceivedData() TLS handshake error\n");
		if (listener) listener->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsHandshakeError);
		return;
	case TlsClient::TlsClientError::Failed:
	default:
		Warning("-RTMPClientConnection::ProcessReceivedData() Failed to decrypt\n");
		if (listener) listener->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsDecryptError);
		return;
	}
}

void RTMPSClientConnection::AddPendingRtmpData(const uint8_t* data, size_t size)
{
	if (tls.Encrypt(data, size) != TlsClient::TlsClientError::NoError)
	{
		Error("-RTMPClientConnection::addPendingRtmpData() TLS encrypt error\n");
		
		if (GetListener()) GetListener()->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsEncryptError);
		return;
	}	
}
