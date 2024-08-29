/* 
 * File:   sidebar.h
 * Author: Sergio
 *
 * Created on 9 de agosto de 2012, 15:26
 */

#ifndef SIDEBAR_H
#define	SIDEBAR_H
#include "config.h"
#include "tools.h"
#include <set>

class Sidebar
{
public:
	Sidebar();
	~Sidebar();

	int  Update(int index,SWORD *samples,DWORD len);
	void Reset();

	void AddParticipant(int id);
	bool HasParticipant(int id);
	void RemoveParticipant(int id);

	SWORD* GetBuffer()	{ return mixer_buffer; }
public:
	static const DWORD MIXER_BUFFER_SIZE = 4096;
private:
	typedef std::set<int> Participants;
private:
	//Audio mixing buffer
	SWORD* mixer_buffer;
	Participants participants;
};

#endif	/* SIDEBAR_H */

