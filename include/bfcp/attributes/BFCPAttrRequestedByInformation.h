#ifndef BFCPATTRREQUESTEDBYINFORMATION_H
#define	BFCPATTRREQUESTEDBYINFORMATION_H


#include "bfcp/BFCPAttribute.h"


// 5.2.16.  REQUESTED-BY-INFORMATION
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |0 0 1 0 0 0 0|M|    Length     |       Requested-by ID         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//  REQUESTED-BY-INFORMATION =   (REQUESTED-BY-INFORMATION-HEADER)
//                               [USER-DISPLAY-NAME]
//                               [USER-URI]
//                              *[EXTENSION-ATTRIBUTE]


class BFCPAttrRequestedByInformation : public BFCPAttribute
{
/* Instance members. */

public:
	BFCPAttrRequestedByInformation(int requestedById);
	void Dump();
	void Stringify(std::wstringstream &json_stream);

private:
	// Mandatory attributes.
	int requestedById;
};


#endif
