#include <sys/stat.h> 
#include <fcntl.h>
#include "SendSideBandwidthEstimation.h"


SendSideBandwidthEstimation::SendSideBandwidthEstimation()
{
	//Add dummy stream to stimator
	estimator.AddStream(0);
}

SendSideBandwidthEstimation::~SendSideBandwidthEstimation()
{
	//If  dumping
	if (fd!=FD_INVALID)
		//Close file
		close(fd);
}
	
void SendSideBandwidthEstimation::SentPacket(const PacketStats::shared& stats)
{
	//Add to stats vector
	transportWideSentPacketsStats[stats->transportWideSeqNum] = stats;
	//Protect against missing feedbacks, remove too old lost packets
	auto it = transportWideSentPacketsStats.begin();
	//If we have more than 5s diff
	while(it!=transportWideSentPacketsStats.end() && (stats->time-it->second->time)>5E6)
		//Erase it and move iterator
		it = transportWideSentPacketsStats.erase(it);
}

void SendSideBandwidthEstimation::ReceivedFeedback(uint8_t feedbackNum, const std::map<uint32_t,uint64_t>& packets)
{
	uint32_t lost = 0;
	
	//For each packet
	for (const auto& feedback : packets)
	{
		//Get feedback data
		auto transportSeqNum	= feedback.first;
		auto receivedTime	= feedback.second; 
		
		//Get packets stats
		auto it = transportWideSentPacketsStats.find(transportSeqNum);
		//If found
		if (it!=transportWideSentPacketsStats.end())
		{
			//Get stats
			const auto& stat = it->second;
			//Get sent time
			const auto sentTime = stat->time;
			
			//Check first feedback received
			if (!firstSent)
				firstSent = sentTime;
			if (!firstRecv)
				firstRecv = receivedTime;
			
			//Correc ts
			auto sent = sentTime - firstSent;
			auto recv = receivedTime ? receivedTime - firstRecv : -1;
			
			//Check if it was not lost
			if (receivedTime)
			{
				//Get deltas
				auto deltaSent = sent - prevSent;
				auto deltaRecv = recv - prevRecv;
				auto delta = deltaRecv - deltaSent;

				//Add it to sender side estimator
				estimator.Update(0,receivedTime/1000,sentTime/1000,stat->size,stat->mark);
				
				//Update last received time
				lastRecv = receivedTime;
				//And previous
				prevSent = sent;
				prevRecv = recv;
				//Dump stats
				//UltraDebug("#%u sent:%.8lu (+%.6lu) recv:%.8lu (+%.6lu) delta:%.6ld fb:%u, size:%u, bwe:%u\n",transportSeqNum,sent,deltaSent,recv,deltaRecv,delta,feedbackNum, stat->size, estimator.GetEstimatedBitrate());
				
				//If dumping to file
				if (fd)
				{
					char msg[1024];
					//Create log
					int len = snprintf(msg,1024,"%u|%u|%u|%.8lu|%.8lu|%.6lu|%.6lu|%.6ld|%u|%u\n",transportSeqNum,feedbackNum, stat->size,sent,recv,deltaSent,deltaRecv,delta,estimator.GetEstimatedBitrate(),rtt);
					//Write it
					write(fd,msg,len);
				}
			} else {
				//Get deltas
				auto deltaSent = sent - prevSent;
				uint64_t deltaRecv = 0;
				uint64_t delta = 0;
				
				//Dump stats
				//UltraDebug("#%u sent:%.8lu (+%.6lu) recv:%.8lu (+%.6lu) delta:%.6ld fb:%u, size:%u\n",transportSeqNum,sent,0,recv,0,0,feedbackNum, stat->size);
				lost++;
				
				
				//If dumping to file
				if (fd)
				{
					char msg[1024];
					//Create log
					int len = snprintf(msg,1024,"%u|%u|%u|%.8lu|%.8lu|%.6lu|%.6lu|%.6ld|%u|%u\n",transportSeqNum,feedbackNum, stat->size,sent,recv,deltaSent,deltaRecv,delta,estimator.GetEstimatedBitrate(),rtt);
					//Write it
					write(fd,msg,len);
				}
			}
			
			//store last one
			lastSent = stat->time;
			
			//Erase it
			transportWideSentPacketsStats.erase(it);
		}
	}
	//If any lost
	estimator.UpdateLost(0,lost,lastRecv/1000);
}

void SendSideBandwidthEstimation::UpdateRTT(uint32_t rtt)
{
	this->rtt = rtt;
	estimator.UpdateRTT(0,rtt,lastRecv/1000);
}

uint32_t SendSideBandwidthEstimation::GetEstimatedBitrate()
{
	return estimator.GetEstimatedBitrate();
}

int SendSideBandwidthEstimation::Dump(const char* filename) 
{
	//If already dumping
	if (fd!=FD_INVALID)
		//Error
		return 0;
	
	Log("-SendSideBandwidthEstimation::Dump [\"%s\"]\n",filename);
	
	//Open file
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600))<0)
		//Error
		return Error("Could not open file [err:%d]\n",errno);

	//Done
	return 1;
}
