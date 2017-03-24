/* 
 * File:   EventSource.h
 * Author: Sergio
 *
 * Created on 24 de marzo de 2017, 23:52
 */

#ifndef EVENTSOURCE_H
#define EVENTSOURCE_H
#include <string>

class EvenSource
{
public:
	EvenSource();
	EvenSource(const char* str);
	EvenSource(const std::wstring &str);
	
	~EvenSource();

	void SendEvent(const char* type,const char* msg,...);
private:
	char* source;
};
#endif /* EVENTSOURCE_H */

