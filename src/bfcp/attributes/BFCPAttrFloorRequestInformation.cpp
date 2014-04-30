#include "bfcp/attributes/BFCPAttrFloorRequestInformation.h"
#include "log.h"
#include <algorithm>


/* Instance methods */

BFCPAttrFloorRequestInformation::BFCPAttrFloorRequestInformation(int floorRequestId) :
	floorRequestId(new BFCPAttrFloorRequestId(floorRequestId)),
	overallRequestStatus(NULL),
	beneficiaryInformation(NULL),
	requestedByInformation(NULL)
{
}


BFCPAttrFloorRequestInformation::~BFCPAttrFloorRequestInformation()
{
	::Debug("BFCPAttrFloorRequestInformation::~BFCPAttrFloorRequestInformation() | free memory\n");

	delete this->floorRequestId;
	int num_floor_request_statuses = this->floorRequestStatuses.size();
	for (int i=0; i < num_floor_request_statuses; i++)
		delete this->floorRequestStatuses[i];
	if (this->overallRequestStatus)
		delete this->overallRequestStatus;
	if (this->beneficiaryInformation)
		delete this->beneficiaryInformation;
	if (this->requestedByInformation)
		delete this->requestedByInformation;
}


void BFCPAttrFloorRequestInformation::Dump()
{
	::Debug("[BFCPAttrFloorRequestInformation]\n");
	::Debug("- floorRequestId: %d\n", this->floorRequestId->GetValue());
	int num_floor_request_statuses = this->floorRequestStatuses.size();
	for (int i=0; i < num_floor_request_statuses; i++) {
		::Debug("- floorRequestStatus:\n");
		this->floorRequestStatuses[i]->Dump();
	}
	if (this->overallRequestStatus) {
		::Debug("- overallRequestStatus:\n");
		this->overallRequestStatus->Dump();
	}
	if (this->beneficiaryInformation) {
		::Debug("- beneficiaryInformation:\n");
		this->beneficiaryInformation->Dump();
	}
	if (this->requestedByInformation) {
		::Debug("- requestedByInformation:\n");
		this->requestedByInformation->Dump();
	}
	::Debug("[/BFCPAttrFloorRequestInformation]\n");
}


void BFCPAttrFloorRequestInformation::Stringify(std::wstringstream &json_stream)
{
	json_stream << L"{";
	json_stream << L"\n  \"floorRequestId\": " << this->floorRequestId->GetValue();
	json_stream << L",\n  \"floorRequestStatus\": [";
	int num_floor_request_statuses = this->floorRequestStatuses.size();
	for (int i=0; i < num_floor_request_statuses; i++) {
		json_stream << L"\n";
		this->floorRequestStatuses[i]->Stringify(json_stream);
		if (i < num_floor_request_statuses - 1)
			json_stream << L", ";
	}
	json_stream << L"\n]";
	if (this->overallRequestStatus) {
		json_stream << L",\n  \"overallRequestStatus\": ";
		this->overallRequestStatus->Stringify(json_stream);
	}
	if (this->beneficiaryInformation) {
		json_stream << L",\n  \"beneficiaryInformation\": ";
		this->beneficiaryInformation->Stringify(json_stream);
	}
	if (this->requestedByInformation) {
		json_stream << L",\n  \"requestedByInformation\": ";
		this->requestedByInformation->Stringify(json_stream);
	}
	json_stream << L"\n}";
}


void BFCPAttrFloorRequestInformation::AddFloorRequestStatus(BFCPAttrFloorRequestStatus *floorRequestStatus)
{
	// Avoid duplicated floorRequestStatuses.
	if (std::find(this->floorRequestStatuses.begin(), this->floorRequestStatuses.end(), floorRequestStatus) != this->floorRequestStatuses.end())
		return;

	this->floorRequestStatuses.push_back(floorRequestStatus);
}


void BFCPAttrFloorRequestInformation::SetOverallRequestStatus(BFCPAttrOverallRequestStatus *overallRequestStatus)
{
	if (this->overallRequestStatus)
		delete this->overallRequestStatus;

	this->overallRequestStatus = overallRequestStatus;
}


void BFCPAttrFloorRequestInformation::SetBeneficiaryInformation(BFCPAttrBeneficiaryInformation *beneficiaryInformation)
{
	if (this->beneficiaryInformation)
		delete this->beneficiaryInformation;

	this->beneficiaryInformation = beneficiaryInformation;
}


void BFCPAttrFloorRequestInformation::SetRequestedByInformation(BFCPAttrRequestedByInformation *requestedByInformation)
{
	if (this->requestedByInformation)
		delete this->requestedByInformation;

	this->requestedByInformation = requestedByInformation;
}


void BFCPAttrFloorRequestInformation::SetDescription(std::wstring& statusInfo)
{
	if (! this->overallRequestStatus)
		return;

	this->overallRequestStatus->SetStatusInfo(new BFCPAttrStatusInfo(statusInfo));
}
