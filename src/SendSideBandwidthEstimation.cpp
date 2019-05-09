#include "SendSideBandwidthEstimation.h"


SendSideBandwidthEstimation::SendSideBandwidthEstimation()
{
	//Add dummy stream to stimator
	estimator.AddStream(0);
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
			//Check if it was lost
			if (receivedTime)
			{
				//UltraDebug("-Update seq:%d,sent:%llu,ts:%ll,size:%d\n",remote.first,remote.second/1000,stat->time/1000,stat->size);
				//Add it to sender side estimator
				estimator.Update(0,receivedTime/1000,stat->time/1000,stat->size,stat->mark);
				//store last one
				last = stat->time;
			} else {
				lost++;
			}
			//Erase it
			transportWideSentPacketsStats.erase(it);
		}
	}
	//If any lost
	estimator.UpdateLost(0,lost,last/1000);
}

void SendSideBandwidthEstimation::UpdateRTT(uint32_t rtt)
{
	estimator.UpdateRTT(0,rtt,last/1000);
}

uint32_t SendSideBandwidthEstimation::GetEstimatedBitrate()
{
	return estimator.GetEstimatedBitrate();
}
