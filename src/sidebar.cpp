/* 
 * File:   sidebar.cpp
 * Author: Sergio
 * 
 * Created on 9 de agosto de 2012, 15:26
 */
#include <string.h>
#include "sidebar.h"
#include "log.h"

Sidebar::Sidebar()
{
}

Sidebar::~Sidebar()
{
}

void Sidebar::Update(int id,SWORD *samples,DWORD len)
{
	//Check size
	if (len>MIXER_BUFFER_SIZE)
		//error
		return Error("-Sidebar error updating particionat, len bigger than mixer max buffer size [len:%d,size:%d]\n",len,MIXER_BUFFER_SIZE);
	//Check if
	if (participants.find(id)==participants.end())
		//Exit
		return Error("-Sidebar error updating particionat not found [id:%d]\n",id);

	//Mix the audio
	for(int i = 0; i < len; ++i)
		//MIX
		mixer_buffer[i] += samples[i];

	//OK
	return len;
}

void Sidebar::Reset()
{
	//zero the mixer buffer
	memset(mixer_buffer, 0, MIXER_BUFFER_SIZE*sizeof(SWORD));
}

void Sidebar::AddParticipant(int id)
{
	participants.insert(id);
}

void Sidebar::RemoveParticipant(int id)
{
	participants.erase(id);
}

bool Sidebar::HasParticipant(int id)
{
	//Check if
	if (participants.find(id)==participants.end())
		//Exit
		return false;
	//Found
	return true;
}

	