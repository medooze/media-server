#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include "flash.h"
#include "log.h"

int Flash::Init()
{
	return true;
}

int Flash::End()
{
	return true;
}

int Flash::StartPlaying(char *ip,int audioPort,int videoPort,char *url)
{

	//Fork process	
	int pid = fork();

	//If forked
	if (!pid)
	{
		char *argv[6];
		char aport [10];
		char vport [10];
			
		//Set arguments
		sprintf(aport,"%d",audioPort);
		sprintf(vport,"%d",videoPort);
		argv[0] = "./flashstreamer";
		argv[1] = ip;
		argv[2] = aport;
		argv[3] = vport;
		argv[4] = url;
		argv[5] = NULL;

		//Exec flashstreamer
		execve(argv[0],argv,NULL);
	}

	//Return pid
	return pid;

	
}

int Flash::StopPlaying(int id)
{
	//Kill child
	return kill(id,9);
}
