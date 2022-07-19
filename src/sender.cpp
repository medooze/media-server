#include "EventSource.h"
#include "log.h"
#include "RTPBundleTransport.h"
#include "OpenSSL.h"
#include <functional>

using namespace std::chrono_literals;

EvenSource::EvenSource(){}
EvenSource::EvenSource(const char* str){}
EvenSource::EvenSource(const std::wstring &str){}
EvenSource::~EvenSource(){}
void EvenSource::SendEvent(const char* type,const char* msg,...){}

RTPBundleTransport endpoint;

class  DTLSICETransportListener :
	public DTLSICETransport::Listener
{
public:
	DTLSICETransportListener(std::function<void(const DTLSICETransport::DTLSState)> callback) : callback(callback) {}
	
	virtual void onDTLSStateChanged(const DTLSICETransport::DTLSState state) override
	{
		callback(state);
	}
private:
	std::function<void(const DTLSICETransport::DTLSState)> callback;
};

int main(int argc, char** argv)
{
	//Set default values
	uint16_t localPort = 0;
	const char* remoteIp = "127.0.0.1";
	uint16_t remotePort = 9090;
	const char* hash = "SHA-256";
	const char* fingerprint = "";
	const char* crtfile = nullptr;
	const char* keyfile = nullptr;
	std::string localUsername	= "sender";
	std::string localPassword	= "sender";
	std::string remoteUsername	= "receiver";
	std::string remotePassword	= "receiver";
	bool useRTX = true;
	
	//Get all
	for(int i=1;i<argc;i++)
	{
		//Check options
		if (strcmp(argv[i],"-h")==0 || strcmp(argv[i],"--help")==0)
		{
			//Show usage
			printf("Medooze Sender test");
			printf("Usage: sender [-h] [--help] [-d] [-dd] [--disable-rtx] [--local-port port]  [--dest-ip port] [--dest-port port] [--remote-hash hash] [--remote-fingerprint fingerprint]\r\n\r\n"
				"Options:\r\n"
				" -h,--help		Print help\r\n"
				" -f			Run as daemon in safe mode\r\n"
				" -d			Enable debug logging\r\n"
				" -dd			Enable more debug logging\r\n"
				" --local-ice-ufrag     Set local udp port\r\n"
				" --local-ice-pwd	Set local udp port\r\n"
				" --local-port		Set local udp port\r\n"
				" --dest-ip		Set remote ip addressip\r\n"
				" --dest-port		Set remote ip port\r\n"
				" --ssl-key		Set SSL key file path\r\n"
				" --ssl-crt		Set SSL certificate file path\r\n"
				" --disable-rtx		Disable RTX\n"
				
			);
			//Exit
			return 0;
		} 
		else if (strcmp(argv[i],"-d")==0)
			//Enable debug
			Logger::EnableDebug(true);
		else if (strcmp(argv[i],"-dd")==0)
			//Enable debug
			Logger::EnableUltraDebug(true);
		else if (strcmp(argv[i],"--local-port")==0 && (i+1<argc))
			//Get local port
			localPort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--dest-ip")==0 && (i+1<argc))
			//Get ip
			remoteIp = argv[++i];
		else if (strcmp(argv[i],"--dest-port")==0 && (i+1<argc))
			//Get dst port
			remotePort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--remote-hash")==0)
			//Get hash
			hash = argv[++i];
		else if (strcmp(argv[i],"--remote-fingerprint")==0)
			//Get fingerprint
			fingerprint = argv[++i];
		else if (strcmp(argv[i],"--ssl-crt")==0 && (i+1<argc))
			//Get certificate file
			crtfile = argv[++i];
		else if (strcmp(argv[i],"--ssl-key")==0 && (i+1<argc))
			//Get certificate key file
			keyfile = argv[++i];
		else if (strcmp(argv[i],"--disable-rtx")==0 && (i+1<argc))
			//Disable rtx
			useRTX = false;
	}
	//Init OpenSSL lib
	if (! OpenSSL::ClassInit()) 
	{
		Error("-OpenSSL failed to initialize, exiting\n");
		exit(1);
	}
	
	//Check if we have certificate and key file
	if (crtfile && keyfile)
		//Set DTLS certificate
		DTLSConnection::SetCertificate(crtfile,keyfile);
	
	//Init DTLS
	if (DTLSConnection::Initialize()) 
	{
		//Print hashes
		Log("-DTLS SHA1   local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA1).c_str());
		Log("-DTLS SHA224 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA224).c_str());
		Log("-DTLS SHA256 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA256).c_str());
		Log("-DTLS SHA384 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA384).c_str());
		Log("-DTLS SHA512 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA512).c_str());
	} else {
		// DTLS not available.
		Error("DTLS initialization failed, no DTLS available\n");
		exit(1);
	}
	
	//Start local port
	endpoint.Init(localPort);
	
	Properties properties;
	//Put ice properties
	properties.SetProperty("ice.localUsername"	, localUsername);
	properties.SetProperty("ice.localPassword"	, localPassword);
	properties.SetProperty("ice.remoteUsername"	, remoteUsername);
	properties.SetProperty("ice.remotePassword"	, remotePassword);

	//Put remote dtls properties
	properties.SetProperty("dtls.setup"		, "passive");
	properties.SetProperty("dtls.hash"		, hash);
	properties.SetProperty("dtls.fingerprint"	, fingerprint);

	//Put other options
	properties.SetProperty("disableSTUNKeepAlive"	, true);
		
	RTPBundleTransport::Connection* connection = endpoint.AddICETransport(localUsername +":"+ remoteUsername,properties);
	
	
	
	//Set rtp properties
	Properties rtp;
	
	//Set codecs
	rtp.SetProperty("video.codecs.length"	, "1");
	rtp.SetProperty("video.codecs.0.codec"	, "vp8");
	rtp.SetProperty("video.codecs.0.pt"	, "96");
	if (true)
		rtp.SetProperty("video.codecs.0.rtx"	, "97");
	//Set extensions
	rtp.SetProperty("video.ext.length"	, "1");
	rtp.SetProperty("video.ext.0.id"	, "1");
	rtp.SetProperty("video.ext.0.uri"	, "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01");
	
	//Set rtp properties
	connection->transport->SetLocalProperties(rtp);
	connection->transport->SetRemoteProperties(rtp);
	
	//Create outgoing tranport
	RTPOutgoingSourceGroup group(MediaFrame::Video);
	group.media.ssrc = 1;
	group.rtx.ssrc = 2;
	
	connection->transport->AddOutgoingSourceGroup(&group);
	
	Timer::shared timer;
	DTLSICETransportListener listener([&](const auto state){
		Log("-DTLSICETransport::DTLSState [state:%d]\n",state);
		switch(state)
		{
			case DTLSICETransport::DTLSState::Connected:
				timer = endpoint.GetTimeService().CreateTimer(0ms,33ms,[&](auto now){
					for (int i=0;i<10;++i)
					{
						//Create rtp packet
						RTPPacket::shared rtp = std::make_shared<RTPPacket>(MediaFrame::Video,VideoCodec::VP8);

						//Set rtp metadata
						rtp->SetSSRC(group.media.ssrc);
						rtp->SetExtSeqNum(group.media.NextExtSeqNum());
						rtp->SetMark(i==9);

						//Set payload
						rtp->SetMediaLength(400);

						//Enqueue packet	
						connection->transport->Enqueue(rtp);
					}
				});
				break;
			case DTLSICETransport::DTLSState::Failed:
			case DTLSICETransport::DTLSState::Closed:
				if (timer)
					timer->Cancel();
				break;
			default:
				break;
		}
	});
	
	//Listen dtls state changes
	connection->transport->SetListener(&listener);
	
	endpoint.AddRemoteCandidate(localUsername + ":" + remoteUsername,remoteIp,remotePort);
	
	sleep(4);
	
	endpoint.End();
	
	return 0;
}

