/* 
 * File:   PCAPFile.h
 * Author: Sergio
 *
 * Created on 27 de diciembre de 2017, 11:20
 */

#ifndef PCAPFILE_H
#define PCAPFILE_H

#include "config.h"
#include "use.h"

class PCAPFile
{
public:
	PCAPFile() = default;
	~PCAPFile();
	int Open(const char* filename);
	void WriteUDP(QWORD currentTimeMillis,DWORD originIp, short originPort, DWORD destIp, short destPort,BYTE* data, DWORD size);
	void Close();
private:
	int fd = -1;
	Mutex mutex;

};

#endif /* PCAPFILE_H */

