/* 
 * File:   groupchat.cpp
 * Author: Sergio
 * 
 * Created on 29 de julio de 2014, 16:10
 */

#include "groupchat.h"
#include <sstream>



GroupChat::GroupChat(std::wstring tag)
{
    //Store tag
    this->tag = tag;
}

GroupChat::~GroupChat()
{
    End();
}

bool GroupChat::Init()
{
	return true;    
}

bool GroupChat::End()
{
        //LOck
        use.WaitUnusedAndLock();

	//Get client iterator
	Clients::iterator it=clients.begin();

	//Close clients
	while (it!=clients.end())
	{
		//Get client
		Client *client = it->second;
		//Erase it and move forward
		clients.erase(it++);
		//End it
		client->Disconnect();
		//Delete it
		delete(client);
	}
        
	//Unlock
	use.Unlock();

}

int GroupChat::WebsocketConnectRequest(int partId,WebSocket *ws)
{
        //Lock
        Log("-WebsocketConnectRequest chat [partId:%d]\n",partId);
    
        //Lock
        use.WaitUnusedAndLock();

	//Create new client
	Client *client = new Client(partId,this);
	//Set client as as user data
	ws->SetUserData(client);

	//Check if there was one already ws connected for that user
	Clients::iterator it = clients.find(partId);
	//If it was
	if (it!=clients.end())
	{
		//Get old client
		Client *old = it->second;
		//End it
		old->Disconnect();
		//Delete it
		delete(old);
		//Set new one
		it->second = client;
	} else {
		//Append to client list
		clients[partId] = client;
	}

	//Unlock clients list
	use.Unlock();

	//Accept incoming connection and add us as listeners
	ws->Accept(this);

	//Connect to socket
	return client->Connect(ws);
}

int GroupChat::Disconnect(WebSocket *socket)
{
	Log("-GroupChat::Disconnect [ws:%p]\n",socket);

	//Get client
	Client *client = (Client *)socket->GetUserData();

	//If no client was attached
	if (!client)
		//Nothing more
		return 0;

	//Lock
	use.WaitUnusedAndLock();
	//Remove it
	clients.erase(client->GetId());
	//Unlock
	use.Unlock();

	//Let it finish
	client->Disconnect();
	//Delete it
	delete(client);

	//OK
	return 1;
}

void GroupChat::onIncomingMessage(Client* client,CPIMMessage *msg)
{
	Debug("-GroupChat::onIncomingMessage [from:%ls,to:%ls]\n",msg->GetFrom().GetURI().c_str(),msg->GetTo().GetURI().c_str());
	//Get from and to uris
	WideStringParser fromParser(msg->GetFrom().GetURI());
	WideStringParser toParser(msg->GetTo().GetURI());

	//Parse scheme
	if (!fromParser.MatchString(L"im:"))
	{
		Error("-GroupChat::onIncomingMessage Incorrect message received: from schema not im [from:%ls]\n",fromParser.GetBuffer());
		return;
	}

	//Get from participant id
	if (!fromParser.ParseInteger())
	{
		Error("-GroupChat::onIncomingMessage Incorrect message received: from username is not an integer [from:%ls]\n",fromParser.GetBuffer());
		return;
	}

	//Get value
	int from = fromParser.GetIntegerValue();
	
	//Ensure it is from the client
	if (from!=client->GetId())
		return;

	//Parse scheme
	if (!toParser.MatchString(L"im:"))
	{
		Error("-GroupChat::onIncomingMessage Incorrect message received: to schema not im [from:%ls]\n",toParser.GetBuffer());
		return;
	}

	//Get from participant id
	if (!toParser.ParseInteger())
	{
		Error("-GroupChat::onIncomingMessage Incorrect message received: to username is not an integer [from:%ls]\n",toParser.GetBuffer());
		return;
	}

	//Get value
	int to = toParser.GetIntegerValue();

	//Send message
	Send(from,to,msg->GetContent()->Clone());
}

bool GroupChat::SendMessage(int from, int to,std::wstring text)
{
	return Send(from,to,new MIMETextWrapper(text));  
}

bool GroupChat::Send(int from, int to,MIMEWrapper *content)
{
	Debug("-Sending message from:%d to:%d\n",from,to);

	std::wstringstream  fromUri;
	std::wstringstream  toUri;

	//Create uris
	fromUri << L"im:" << from << L"@" << tag;
	toUri  << L"im:"<< to << L"@" << tag;

	//Create message
	CPIMMessage msg(fromUri.str(),toUri.str(),content);

	//Lock list
	use.IncUse();

	//If it is a private 
	if (to>0)
	{
	    //Find it
	    Clients::iterator it = clients.find(to);
	    //If found
	    if (it!=clients.end())
		//Send it
		it->second->Send(msg);
	} else {
	    //Send to all
	    for  (Clients::iterator it = clients.begin(); it!=clients.end(); ++it)
		//Except himself
		if (it->second->GetId()!=from)
			//Send
			it->second->Send(msg);
	}

	//Free list
	use.DecUse();

	//Ok
	return 1;

}
void GroupChat::onOpen(WebSocket *ws)
{

}

void GroupChat::onMessageStart(WebSocket *ws,const WebSocket::MessageType type,const DWORD length)
{
        //Get client
	Client *client = (Client *)ws->GetUserData();
	//Process it
	client->MessageStart();
}

void GroupChat::onMessageData(WebSocket *ws,const BYTE* data, const DWORD size)
{
	//Get client
	Client *client = (Client *)ws->GetUserData();
	//Process it
	client->MessageData(data,size);
}

void GroupChat::onMessageEnd(WebSocket *ws)
{
	//Get client
	Client *client = (Client *)ws->GetUserData();
	//Process recevied message
	client->MessageEnd();
}

void GroupChat::onClose(WebSocket *ws)
{
	//Disconnect ws
	Disconnect(ws);
}

void GroupChat::onError(WebSocket *ws)
{
	//Disconnect ws
	Disconnect(ws);
}

void GroupChat::onWriteBufferEmpty(WebSocket *ws)
{
	//Nothing
}

GroupChat::Client::Client(int id,GroupChat* server)
{
	//Store id
	this->id = id;
	//Store server
	this->server = server;
}

GroupChat::Client::~Client()
{
        //Discponnect just in case
        Disconnect();
}

int GroupChat::Client::Connect(WebSocket *ws)
{
	Debug(">GroupChat::Client::Connect [ws:%p,this:%p]\n",ws,this);
	//Store websocekt
	this->ws = ws;
}

int GroupChat::Client::Disconnect()
{
	Debug(">GroupChat::Client::Disconnect [this:%p]\n",this);

	//Lock wait
	lock.Lock();

	//If we did had a ws
	if (ws)
	{
		//Detach listeners
		ws->Detach();
		//Remove ws data
		ws->SetUserData(NULL);
		//Close just in cae
		ws->ForceClose();
		//Unset websocket
		ws = NULL;
	}
	
	//Unlock
	lock.Unlock();

	//OK
	return 1;
}

void GroupChat::Client::MessageStart()
{
    //No lenght
    length = 0;
}   

void GroupChat::Client::MessageData(const BYTE* data, const DWORD size)
{
    //Check size
    if (length+size>65535)
        //Exit
        return;
    //Append
    memcpy(buffer+length,data,size);
    //Increase length
    length += size;
}

void GroupChat::Client::MessageEnd()
{
    //Parse CPIM message
    CPIMMessage *msg = CPIMMessage::Parse(buffer,length);
    
    //Check if found
    if (!msg)
    {
        //Error
        Error("-Incorrect CPIM message found");
        //Exit
        return;
    }

    Debug("-GroupChat::Client::MessageEnd Got message from: %d\n",id);
    msg->Dump();
    
    //send to server
    server->onIncomingMessage(this,msg);
    //Delete it
    delete(msg);
}

int GroupChat::Client::Send(const CPIMMessage &msg)
{
	Debug("-GroupChat::Client::Send [id:%d]\n",id);

	BYTE aux[65535];
	
	//Serialize
	DWORD len = msg.Serialize(aux,65535);

	msg.Dump();
	
	//Lock
	lock.Lock();

	//Check if we have ws
	if (ws)
		//Send it
		ws->SendMessage(WebSocket::Text,aux,len);

	//Unlock
	lock.Unlock();
	
	return 1;
}
