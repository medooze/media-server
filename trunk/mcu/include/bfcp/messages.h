#ifndef BFCP_MESSAGES_H
#define	BFCP_MESSAGES_H


/* BFCP requests we can receive, parse and process. */
#include "messages/BFCPMsgFloorRequest.h"
#include "messages/BFCPMsgFloorRelease.h"
#include "messages/BFCPMsgFloorQuery.h"
#include "messages/BFCPMsgHello.h"

/* BFCP responses and notifications we can generate and send. */
#include "messages/BFCPMsgError.h"
#include "messages/BFCPMsgFloorRequestStatus.h"
#include "messages/BFCPMsgFloorStatus.h"


#endif
