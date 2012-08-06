#ifndef _FLASH_H_
#define _FLASH_H_

class Flash
{
public:
	int Init();
	int StartPlaying(char *ip,int audioPort,int videoPort,char *url);
	int StopPlaying(int id);
	int End();
};

#endif
