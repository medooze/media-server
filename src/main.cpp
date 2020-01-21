#include "log.h"
#include "assertions.h"
#include "xmlrpcserver.h"
#include "xmlhandler.h"
#include "xmlstreaminghandler.h"
#include "ws/websockets.h"
#include "statushandler.h"
#include "audiomixer.h"
#include "rtmp/rtmpserver.h"
#include "mcu.h"
#include "ws/websocketserver.h"
#include "OpenSSL.h"
#include "dtls.h"
#include "groupchat.h"
#include "CPUMonitor.h"
#include "EventSource.h"
#include "eventstreaminghandler.h"
extern "C" {
	#include "libavcodec/avcodec.h"
}
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>

#ifdef CEF
#include "cef/Browser.h"	
#endif 


extern XmlHandlerCmd mcuCmdList[];

void log_ffmpeg(void* ptr, int level, const char* fmt, va_list vl)
{
	static int print_prefix = 1;
	char line[1024];

	if (!Logger::IsUltraDebugEnabled() && level > AV_LOG_ERROR)
		return;

	//Format the
	av_log_format_line(ptr, level, fmt, vl, line, sizeof(line), &print_prefix);

	//Remove buffer errors
	if (strstr(line,"vbv buffer overflow")!=NULL)
		//exit
		return;
	//Log
	Log(line);
}

int lock_ffmpeg(void **param, enum AVLockOp op)
{
	//Get mutex pointer
	pthread_mutex_t* mutex = (pthread_mutex_t*)(*param);
	//Depending on the operation
	switch(op)
	 {
		case AV_LOCK_CREATE:
			//Create mutex
			mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
			//Init it
			pthread_mutex_init(mutex,NULL);
			//Store it
			*param = mutex;
			break;
		case AV_LOCK_OBTAIN:
			//Lock
			pthread_mutex_lock(mutex);
			break;
		case AV_LOCK_RELEASE:
			//Unlock
			pthread_mutex_unlock(mutex);
			break;
		case AV_LOCK_DESTROY:
			//Destroy mutex
			pthread_mutex_destroy(mutex);
			//Free memory
			free(mutex);
			//Clean
			*param = NULL;
			break;
	}
	return 0;
}


class GroupChatTestHandler :
	public WebSocketServer::Handler
{
public:
	GroupChatTestHandler() : chat(L"test")
	{
		chat.Init();
	}
	
	~GroupChatTestHandler()
	{
		chat.End();
	}
	
	virtual void onWebSocketConnection(const HTTPRequest& request,WebSocket *ws)
	{
		Debug("-onUpgradeRequest %s\n", request.GetRequestURI().c_str());
		
		//Get url
		std::string url = request.GetRequestURI();

		StringParser parser(url);

		//Check it is for us
		if (!parser.MatchString("/gct/"))
			//reject
			return ws->Reject(400,"Bad request");

		//Get until next /
		if (!parser.ParseUntilCharset("/"))
			//reject
			return ws->Reject(400,"Bad request");

		//Get conf id value
		std::string value = parser.GetValue();

		//Get id by tag
		int partId = atoi(value.c_str());
		
		//Check int
		if (partId<=0)
			//reject
			return ws->Reject(400,"Bad request");

	
		//Accept request
		chat.WebsocketConnectRequest(partId,ws);
	}
	
	GroupChat chat;
};

int main(int argc,char **argv)
{
	//Set default values
	bool forking = false;
	bool dump = false;
	int port = 8080;
	char* iface = NULL;
	int wsPort = 9090;
	int rtmpPort = 1935;
	int minPort = 0;
	int maxPort = 0;
	int vadPeriod = 2000;
	const char *logfile = "mcu.log";
	const char *pidfile = "mcu.pid";
	const char *crtfile = NULL;
	const char *keyfile = NULL;
    
	//Get all
	for(int i=1;i<argc;i++)
	{
		//Check options
		if (strcmp(argv[i],"-h")==0 || strcmp(argv[i],"--help")==0)
		{
			//Show usage
			printf("Medooze MCU media mixer version %s %s\r\n",MCUVERSION,MCUDATE);
			printf("Usage: mcu [-h] [--help] [--mcu-log logfile] [--mcu-pid pidfile] [--http-port port] [--rtmp-port port] [--min-rtp-port port] [--max-rtp-port port] [--vad-period ms]\r\n\r\n"
				"Options:\r\n"
				" -h,--help        Print help\r\n"
				" -f               Run as daemon in safe mode\r\n"
				" -d               Enable debug logging\r\n"
				" -dd              Enable more debug logging\r\n"
				" -g               Dump core on SEG FAULT\r\n"
				" --mcu-log        Set mcu log file path (default: mcu.log)\r\n"
				" --mcu-pid        Set mcu pid file path (default: mcu.pid)\r\n"
				" --mcu-crt        Set mcu SSL certificate file path (default: mcu.crt)\r\n"
				" --mcu-key        Set mcu SSL key file path (default: mcu.pid)\r\n"
				" --http-port      Set HTTP xmlrpc api port\r\n"
				" --http-ip        Set HTTP xmlrpc api listening interface ip\r\n"
				" --min-rtp-port   Set min rtp port\r\n"
				" --max-rtp-port   Set max rtp port\r\n"
				" --rtmp-port      Set RTMP port\r\n"
				" --websocket-port Set WebSocket server port\r\n"
				" --vad-period     Set the VAD based conference change period in milliseconds (default: 2000ms)\r\n");
			//Exit
			return 0;
		} else if (strcmp(argv[i],"-f")==0)
			//Fork
			forking = true;
		else if (strcmp(argv[i],"-g")==0)
			//Dump core
			dump = true;
		else if (strcmp(argv[i],"-d")==0)
			//Enable debug
			Logger::EnableDebug(true);
		else if (strcmp(argv[i],"-dd")==0)
			//Enable debug
			Logger::EnableUltraDebug(true);
		else if (strcmp(argv[i],"--http-port")==0 && (i+1<argc))
			//Get port
			port = atoi(argv[++i]);
		else if (strcmp(argv[i],"--http-ip")==0 && (i+1<argc))
			//Get ip
			iface = argv[++i];
		else if (strcmp(argv[i],"--rtmp-port")==0 && (i+1<argc))
			//Get rtmp port
			rtmpPort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--websocket-port")==0 && (i+1<argc))
			//Get port
			wsPort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--min-rtp-port")==0 && (i+1<argc))
			//Get rtmp port
			minPort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--max-rtp-port")==0 && (i+1<argc))
			//Get rtmp port
			maxPort = atoi(argv[++i]);
		else if (strcmp(argv[i],"--mcu-log")==0 && (i+1<argc))
			//Get rtmp port
			logfile = argv[++i];
		else if (strcmp(argv[i],"--mcu-pid")==0 && (i+1<argc))
			//Get rtmp port
			pidfile = argv[++i];
		else if (strcmp(argv[i],"--mcu-crt")==0 && (i+1<argc))
			//Get certificate file
			crtfile = argv[++i];
		else if (strcmp(argv[i],"--mcu-key")==0 && (i+1<argc))
			//Get certificate key file
			keyfile = argv[++i];
		else if (strcmp(argv[i],"--vad-period")==0 && (i+1<=argc))
			//Get rtmp port
			vadPeriod = atoi(argv[++i]);
		else if (strcmp(argv[i],"--type=zygote")==0) {
			//Exit process
			Log("Exting zygote process\n");
			//Exit
			exit(0);
		}
		
	}

	//If we need to fork
	if(forking)
	{
		//Create the chld
		pid_t pid = fork();
		// fork error
		if (pid<0) exit(1);
		// parent exits
		if (pid>0) exit(0);
		
		//Loop
		while(true) 
		{
			//Create the safe child
			pid = fork();

			//Check child pid
			if (pid==0)
			{
				//It is the child obtain a new process group
				setsid();
				//for each descriptor opened
				for (int i=getdtablesize();i>=0;--i)
					//Close it
					close(i);
				//Redirect stdout and stderr
				int fd = open(logfile, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
				int ret;
				ret = dup(fd);
				if (ret < 0) {
					fprintf(stderr, "ERROR: dup(fd=%d) failed: %s\n", fd, strerror(errno));
					MCU_ASSERT(ret >= 0);
				}
				ret = dup2(1,2);
				if (ret < 0) {
					fprintf(stderr, "ERROR: dup2(1,2) failed: %s\n", strerror(errno));
					MCU_ASSERT(ret >= 0);
				}
				close(fd);
				//And continule
				break;
			} else if (pid<0)
				//Error
				return 0;
			//Log
			printf("MCU started [pid:%d,child:%d]\r\n",getpid(),pid);

			//Pid string
			char spid[16];
			//Print it
			sprintf(spid,"%d",pid);

			//Write pid to file
			int pfd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			//Write it
			int len = write(pfd,spid,strlen(spid));
			//Close it
			close(pfd);

			int status;

			do
			{
				//Wait for child
				if (waitpid(pid, &status, WUNTRACED | WCONTINUED)<0)
					return -1;
				//If it has exited or stopped
				if (WIFEXITED(status) || WIFSTOPPED(status))
					//Exit
					return 0;
				//If we have been  killed
				if (WIFSIGNALED(status) && WTERMSIG(status)==9)
					//Exit
					return 0;
			} while (!WIFEXITED(status) && !WIFSIGNALED(status));
		}
	}

	//If we have to dump on SEG FAULT
	if (dump) {
		//Dump core on fault
		rlimit l = {RLIM_INFINITY,RLIM_INFINITY};

		//Set new limit
		setrlimit(RLIMIT_CORE, &l);
	}
	//Log version
	Log("-MCU Version %s %s [pid:%d,ppid:%d]\r\n",MCUVERSION,MCUDATE,getpid(),getppid());

	//Register mutext for ffmpeg
	av_lockmgr_register(lock_ffmpeg);

	//Set log level
	av_log_set_callback(log_ffmpeg);

	//Ignore SIGPIPE
	signal( SIGPIPE, SIG_IGN );


	//Hack to allocate fd =0 and avoid bug closure
	int fdzero = socket(AF_INET, SOCK_STREAM, 0);
	//Create monitor
	CPUMonitor	monitor;
	//Create servers
	XmlRpcServer	server(port,iface);
	RTMPServer	rtmpServer;
	WebSocketServer wsServer;

	//Init OpenSSL lib
	if (! OpenSSL::ClassInit()) {
		Error("-OpenSSL failed to initialize, exiting\n");
		exit(1);
	}
	
	//Init avcodecs
	av_register_all();	
	avcodec_register_all();

	//Set default video mixer vad period
	VideoMixer::SetVADDefaultChangePeriod(vadPeriod);

	//Set port ramge
	if (minPort && maxPort && !RTPTransport::SetPortRange(minPort,maxPort))
		//Using default ones
		Log("-RTPSession using default port range [%d,%d]\n",RTPTransport::GetMinPort(),RTPTransport::GetMaxPort());

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
	}

	//Create services
	MCU		mcu;

	//Create xml cmd handlers for the mcu and broadcaster
	XmlHandler xmlrpcmcu(mcuCmdList,(void*)&mcu);
	//Get event source handler singleton
	EventStreamingHandler& events = EventStreamingHandler::getInstance();

	//Create upload handlers
	UploadHandler uploadermcu(&mcu);

	//Create http streaming for service events
	XmlStreamingHandler xmleventmcu;

	//And default status hanlder
	StatusHandler status;
	
	//TODO: Remove, they are just for testing, should not be enabled on production
	TextEchoWebsocketHandler echo;
	GroupChatTestHandler gct;

	//Init de mcu
	mcu.Init(&xmleventmcu);

	//Add the rtmp application from the mcu to the rtmp server
	rtmpServer.AddApplication(L"mcu/",&mcu);
	rtmpServer.AddApplication(L"mcutag/",&mcu);

	//Append mcu cmd handler to the http server
	server.AddHandler("/mcu",&xmlrpcmcu);
	server.AddHandler("/events/mcu",&xmleventmcu);
	//Append stream evetns
	server.AddHandler("/stream",&events);

	//Add uploaders
	server.AddHandler("/upload/mcu/app/",&uploadermcu);

	//Add websocket handlers
	wsServer.AddHandler("/echo", &echo);
	wsServer.AddHandler("/mcu", &mcu);
	wsServer.AddHandler("/gct", &gct);

	//Add the html status handler
	server.AddHandler("/status",&status);

	//Init the rtmp server
	rtmpServer.Init(rtmpPort);

	//Init web socket server
	wsServer.Init(wsPort);

	//Set mcu monitor listener
	monitor.AddListener(&mcu);

	//Start cpu monitor
	monitor.Start(10000);

	//Run it sync
	server.Start(false);
	
	//Stop monitor
	monitor.Stop();

	//End the rtmp server
	rtmpServer.End();

	//End the mcu
	mcu.End();
	//End servers
	rtmpServer.End();
	//ENd ws server
	wsServer.End();

	//End DTLS
	DTLSConnection::Terminate();
}

