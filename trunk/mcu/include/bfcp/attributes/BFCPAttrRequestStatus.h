#ifndef BFCPATTRREQUESTSTATUS_H
#define	BFCPATTRREQUESTSTATUS_H


#include "bfcp/BFCPAttribute.h"
#include <string>


// 5.2.5.  REQUEST-STATUS
//
//    The following is the format of the REQUEST-STATUS attribute.
//
//       0                   1                   2                   3
//       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//      |0 0 0 0 1 0 1|M|0 0 0 0 0 1 0 0|Request Status |Queue Position |
//      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


class BFCPAttrRequestStatus : public BFCPAttribute
{
/* Static members. */

public:
	enum Status {
		Pending = 1,
		Accepted,
		Granted,
		Denied,
		Cancelled,
		Released,
		Revoked
	};

	// For BFCP JSON.
	static std::map<enum Status, std::wstring>  mapStatus2JsonStr;

public:
	static void Init();

/* Instance members. */

public:
	BFCPAttrRequestStatus();
	BFCPAttrRequestStatus(enum Status status, int queuePosition);
	void Dump();
	void Stringify(std::wstringstream &json_stream);

	void SetStatus(enum Status status);
	void SetQueuePosition(int queuePosition);
	enum Status GetStatus();
	std::wstring GetStatusString();
	int GetQueuePosition();

private:
	enum Status status;
	int queuePosition;
};


#endif
