#include "videostream.h"
#define log_debug printf
#include "FlashPlayer.h"

int main(int argc, char* argv[])
{
	if(argc<5)
	{
		printf("Usage: flashstreamer ip audio_port video_port url_swf\n");
		return 1;
	}

	FlashPlayer 	player;
	VideoStream	stream;

	char *ip = argv[1];
	int aport = atoi(argv[2]);
	int vport = atoi(argv[3]);
	char *url = argv[4];

	//Init Player
	player.Init(url,352,288);

	//Init video stream
	stream.Init(player.GetInput(),NULL);

	//Set video codecs
	stream.SetVideoCodec(H263_1998,CIF,15,512,1,31);

	//Start sending
	stream.StartSending(ip,vport);
	
	//Run
	player.Start();

	getchar();

	//End
	player.End();
	stream.End();
}
