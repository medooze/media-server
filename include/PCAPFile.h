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
#include "UDPDumper.h"

class PCAPFile :
	public UDPDumper
{
public:
	PCAPFile() = default;
	~PCAPFile();
	int Open(const char* filename);
	virtual void WriteUDP(QWORD currentTimeMillis,DWORD originIp, short originPort, DWORD destIp, short destPort,const BYTE* data, DWORD size, DWORD truncate = 0) override;
	virtual void Close() override;
private:
	int fd = -1;
	Mutex mutex;

};

#endif /* PCAPFILE_H */

