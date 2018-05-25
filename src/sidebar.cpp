/* 
 * File:   sidebar.cpp
 * Author: Sergio
 * 
 * Created on 9 de agosto de 2012, 15:26
 */
#include <string.h>
#include <emmintrin.h>
#include "sidebar.h"
#include "log.h"

Sidebar::Sidebar()
{
	//Alloc alligned
	mixer_buffer = (SWORD*) malloc32(MIXER_BUFFER_SIZE*sizeof(SWORD));
}

Sidebar::~Sidebar()
{
	free(mixer_buffer);
}

int Sidebar::Update(int id,SWORD *samples,DWORD len)
{
	//Check size
	if (len>MIXER_BUFFER_SIZE)
		//error
		return Error("-Sidebar error updating particionat, len bigger than mixer max buffer size [len:%d,size:%d]\n",len,MIXER_BUFFER_SIZE);

	//Get pointers to buffer
	__m128i* d = (__m128i*) mixer_buffer;
	__m128i* s = (__m128i*) samples;

	//Sum 8 ech time
	for(DWORD n = (len + 7) >> 3; n != 0; --n,++d,++s)
	{
		//Load data in SSE registers
		__m128i xmm1 = _mm_load_si128(d);
		__m128i xmm2 = _mm_load_si128(s);
		//SSE2 sum
		_mm_store_si128(d, _mm_add_epi16(xmm1,xmm2));
	}

	//OK
	return len;
}

void Sidebar::Reset()
{
	//zero the mixer buffer
	memset((BYTE*)mixer_buffer, 0, MIXER_BUFFER_SIZE*sizeof(SWORD));
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

	
