/* 
 * File:   uploadhandler.h
 * Author: Sergio
 *
 * Created on 10 de enero de 2013, 11:33
 */

#ifndef UPLOADHANDLER_H
#define	UPLOADHANDLER_H

#include "xmlrpcserver.h"
#include "http.h"

class UploadHandler :
	public Handler
{
public:
	class Listener
	{
	public:
		//Virtual desctructor
		virtual ~Listener(){};
	public:
		//Interface
		virtual int onFileUploaded(const char* url, const char *filename) = 0;
	};
public:
	UploadHandler(Listener *listener);
	virtual int ProcessRequest(TRequestInfo *req,TSession * const ses);
private:
	Listener *listener;
};

#endif	/* UPLOADHANDLER_H */

