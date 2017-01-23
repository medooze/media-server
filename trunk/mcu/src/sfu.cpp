#include "log.h"
#include "assertions.h"
#include "xmlrpcserver.h"
#include "xmlhandler.h"
#include "eventstreaminghandler.h"
#include "xmlstreaminghandler.h"
#include "statushandler.h"
#include "RTPTransport.h"
#include "sfu/SFUManager.h"
#include "OpenSSL.h"
#include "dtls.h"
#include "CPUMonitor.h"
#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cerrno>

extern XmlHandlerCmd sfuCmdList[];

int main(int argc,char **argv)
{
	//Set default values
	bool forking = false;
	bool dump = false;
	int port = 8080;
	char* iface = NULL;
	int wsPort = 9090;
	int rtmpPort = 1935;
	int minPort = RTPTransport::GetMinPort();
	int maxPort = RTPTransport::GetMaxPort();
	int vadPeriod = 2000;
	const char *logfile = "sfu.log";
	const char *pidfile = "sfu.pid";
	const char *crtfile = "sfu.crt";
	const char *keyfile = "sfu.key";
    
	//Get all
	for(int i=1;i<argc;i++)
	{
		//Check options
		if (strcmp(argv[i],"-h")==0 || strcmp(argv[i],"--help")==0)
		{
			//Show usage
			printf("Medooze SFU version %s %s\r\n",MCUVERSION,MCUDATE);
			printf("Usage: sfu [-h] [--help] [--sfu-log logfile] [--sfu-pid pidfile] [--http-port port] [--rtmp-port port] [--min-rtp-port port] [--max-rtp-port port] [--vad-period ms]\r\n\r\n"
				"Options:\r\n"
				" -h,--help        Print help\r\n"
				" -f               Run as daemon in safe mode\r\n"
				" -d               Enable debug logging\r\n"
				" -dd              Enable more debug logging\r\n"
				" -g               Dump core on SEG FAULT\r\n"
				" --sfu-log        Set sfu log file path (default: sfu.log)\r\n"
				" --sfu-pid        Set sfu pid file path (default: sfu.pid)\r\n"
				" --sfu-crt        Set sfu SSL certificate file path (default: sfu.crt)\r\n"
				" --sfu-key        Set sfu SSL key file path (default: sfu.pid)\r\n"
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
		else if (strcmp(argv[i],"--sfu-log")==0 && (i+1<argc))
			//Get rtmp port
			logfile = argv[++i];
		else if (strcmp(argv[i],"--sfu-pid")==0 && (i+1<argc))
			//Get rtmp port
			pidfile = argv[++i];
		else if (strcmp(argv[i],"--sfu-crt")==0 && (i+1<argc))
			//Get certificate file
			crtfile = argv[++i];
		else if (strcmp(argv[i],"--sfu-key")==0 && (i+1<argc))
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


	//Ignore SIGPIPE
	signal( SIGPIPE, SIG_IGN );


	//Hack to allocate fd =0 and avoid bug closure
	int fdzero = socket(AF_INET, SOCK_STREAM, 0);
	//Create monitor
	CPUMonitor	monitor;
	//Create servers
	XmlRpcServer	server(port,iface);

	//Init OpenSSL lib
	if (! OpenSSL::ClassInit()) {
		Error("-OpenSSL failed to initialize, exiting\n");
		exit(1);
	}
	
	//Set port ramge
	if (!RTPTransport::SetPortRange(minPort,maxPort))
		//Using default ones
		Log("-RTPSession using default port range [%d,%d]\n",RTPTransport::GetMinPort(),RTPTransport::GetMaxPort());

	//Set DTLS certificate
	DTLSConnection::SetCertificate(crtfile,keyfile);
	//Log
	Log("-Set SSL certificate files [crt:\"%s\",key:\"%s\"]\n",crtfile,keyfile);

	//Init DTLS
	if (DTLSConnection::ClassInit()) {
		//Print hashes
		Log("-DTLS SHA1   local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA1).c_str());
		Log("-DTLS SHA224 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA224).c_str());
		Log("-DTLS SHA256 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA256).c_str());
		Log("-DTLS SHA384 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA384).c_str());
		Log("-DTLS SHA512 local fingerprint \"%s\"\n",DTLSConnection::GetCertificateFingerPrint(DTLSConnection::SHA512).c_str());
	}
	// DTLS not available.
	else {
		Error("DTLS initialization failed, no DTLS available\n");
	}

	//Create services
	SFUManager	sfuManager;

	//Create xml cmd handlers for the sfu and broadcaster
	XmlHandler xmlrpcsfu(sfuCmdList,(void*)&sfuManager);
	//Get event source handler singleton
	EventStreamingHandler& events = EvenSource::getInstance();

	//Create http streaming for service events
	XmlStreamingHandler xmleventsfu;

	//And default status hanlder
	StatusHandler status;
	
	//Init SFU
	sfuManager.Init(&xmleventsfu);

	//Append sfu cmd handler to the http server
	server.AddHandler("/sfu",&xmlrpcsfu);
	server.AddHandler("/events/sfu",&xmleventsfu);

	//Add the html status handler
	server.AddHandler("/status",&status);

	//Start cpu monitor
	monitor.Start(10000);

	//Run it sync
	server.Start(false);

	//Stop monitor
	monitor.Stop();

	//End SFU
	sfuManager.End();
}

