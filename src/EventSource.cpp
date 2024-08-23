/* 
 * File:   EventSource.cpp
 * Author: Sergio
 * 
 * Created on 24 de marzo de 2017, 23:52
 */

#include "EventSource.h"
#include "eventstreaminghandler.h"


EvenSource::EvenSource()
{
	//alocate mem
	source = (char*)malloc(sizeof(void*)*2+4);
	//Create event source
	sprintf(source,"%p",this);
	//Create source
	EventStreamingHandler::getInstance().CreateEvenSource(source);
}

EvenSource::EvenSource(const char* str)
{
	//Duplicate
	source = strdup(str);
	//Create source
	EventStreamingHandler::getInstance().CreateEvenSource(source);
}

EvenSource::EvenSource(const std::wstring &str)
{
	//Convert to utf
	UTF8Parser parser(str);
	//Get size
	DWORD size = parser.GetUTF8Size();
	//alocate mem
	source = (char*)malloc(size+1);
	//Serialize
	parser.Serialize((BYTE*)source,size);
	//End it
	source[size] = 0;
	//Create source
	EventStreamingHandler::getInstance().CreateEvenSource(source);
}

EvenSource::~EvenSource()
{
	//Destroy source
	EventStreamingHandler::getInstance().DestroyEvenSource(source);
	//Free memory
	free(source);
}

void EvenSource::SendEvent(const char* type,const char* msg,...)
{
	va_list params;
	//Initialize parameeter list
	va_start(params, msg);
	//Write event
	EventStreamingHandler::getInstance().WriteEvent(source,type,msg,params);
	//End parameter list
	va_end(params);
}