#ifndef WRAPEXTENDER_H
#define WRAPEXTENDER_H

#include <limits>


class WrapExtender
{
public:
	uint32_t Extend(uint32_t seqNum)
	{
		//If this is first
		if (!extSeqNum)
		{
			//Update seq num
			this->extSeqNum = seqNum;
			//Done
			return cycles;
		}
		
		//Check if we have a sequence wrap
		if (seqNum<GetSeqNum() && GetSeqNum()-seqNum>std::numeric_limits<uint32_t>::max()/2)
			//Increase cycles
			cycles++;
		//If it is an out of order packet from previous cycle
		else if (seqNum>GetSeqNum() &&  seqNum-GetSeqNum()>std::numeric_limits<uint32_t>::max()/2 && cycles)
			//Do nothing and return prev one
			return cycles-1;
		//Get ext seq
		uint64_t extSeqNum = ((uint64_t)cycles)<<32 | seqNum;

		//If we have a not out of order packet
		if (extSeqNum > this->extSeqNum)
			//Update seq num
			this->extSeqNum = extSeqNum;
		
		//Current cycle
		return cycles;
	}
		
	uint32_t RecoverCycles(uint64_t seqNum)
	{
		//Check secuence wrap
	       if (seqNum>GetSeqNum() &&  seqNum-GetSeqNum()>std::numeric_limits<uint32_t>::max()/2 && cycles)
		       //It is from the past cycle
		       return cycles - 1;
	       //From this
	       return cycles;
	}
	
	uint64_t GetExtSeqNum() const	{ return extSeqNum;	}
	uint32_t GetSeqNum() const	{ return extSeqNum;	}
	uint64_t GetCycles() const	{ return cycles;	}
	
	void Reset()
	{
		extSeqNum	= 0;
		cycles		= 0;
	}
	
private:
	uint64_t extSeqNum	= 0;
	uint32_t cycles		= 0;
};

#endif /* WRAPEXTENDER_H */

