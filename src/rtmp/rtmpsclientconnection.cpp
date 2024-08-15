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

void RTMPSClientConnection::Stop()
{
	tls.Shutdown();
	
	RTMPClientConnection::Stop();
}

bool RTMPSClientConnection::IsConnectionReady()
{
	return tls.IsInitialised();
}

void RTMPSClientConnection::OnReadyToTransfer()
{
	tls.PopAllEncrypted([this](auto&& data){
		WriteData(data.data(), data.size());
	});
}

void RTMPSClientConnection::processReceivedData(const uint8_t* data, size_t size)
{
	auto listener = GetListener();
	
	auto ret = tls.Decrypt(data, size);
	switch(ret) 
	{
	case TlsClient::TlsClientError::NoError:
		break;
	case TlsClient::TlsClientError::HandshakeFailed:
		Warning("-RTMPClientConnection::processReceivedData() TLS handshake error\n");
		if (listener) listener->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsHandshakeError);
		return;
	case TlsClient::TlsClientError::Failed:
	default:
		Warning("-RTMPClientConnection::processReceivedData() Failed to decrypt\n");
		if (listener) listener->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsDecryptError);
		return;
	}
	
	tls.PopAllDecypted([this](auto&& data) {
		//Parse data
		ParseData(data.data(), data.size());
	});
}

void RTMPSClientConnection::addPendingRtmpData(const uint8_t* data, size_t size)
{
	if (tls.Encrypt(data, size) != TlsClient::TlsClientError::NoError)
	{
		Error("-RTMPClientConnection::addPendingRtmpData() TLS encrypt error\n");
		
		if (GetListener()) GetListener()->onDisconnected(this, RTMPClientConnection::ErrorCode::TlsEncryptError);
		return;
	}	
}
