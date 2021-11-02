#ifndef WRAPEXTENDER_H
#define WRAPEXTENDER_H

#include <limits>

template <typename N, typename X>
class WrapExtender
{
public:
	X ExtendOrReset(N seqNum)
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
		if (seqNum < GetSeqNum() && GetSeqNum() - seqNum > std::numeric_limits<N>::max() / 2)
			//Increase cycles
			cycles++;
		
		//Update seq num
		this->extSeqNum = (cycles << (sizeof(N) * 8)) | seqNum;

		//Current cycle
		return cycles;
	}

	X Extend(N seqNum)
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
		if (seqNum < GetSeqNum() && GetSeqNum() - seqNum > std::numeric_limits<N>::max() / 2)
			//Increase cycles
			cycles++;
		//If it is an out of order packet from previous cycle
		else if (seqNum > GetSeqNum() && seqNum - GetSeqNum() > std::numeric_limits<N>::max() / 2 && cycles)
			//Do nothing and return prev one
			return cycles - 1;
		//Get ext seq
		X extSeqNum = (cycles << (sizeof(N)*8)) | seqNum;

		//If we have a not out of order packet
		if (extSeqNum > this->extSeqNum)
			//Update seq num
			this->extSeqNum = extSeqNum;

		//Current cycle
		return cycles;
	}

	X RecoverCycles(X seqNum) const
	{
		//Check secuence wrap
		if (seqNum > GetSeqNum() && seqNum - GetSeqNum() > std::numeric_limits<N>::max() / 2 && cycles)
			//It is from the past cycle
			return cycles - 1;
		//From this
		return cycles;
	}

	X Recover(X seqNum) const
	{
		//Get cycles
		X cycles = RecoverCycles(seqNum);
		//Get ext seq
		X extSeqNum = (cycles << (sizeof(N) * 8)) | seqNum;
		//Get extended seq num
		return extSeqNum;
	}
	
	X GetExtSeqNum() const	{ return extSeqNum;	}
	N GetSeqNum() const	{ return extSeqNum;	}
	X GetCycles() const	{ return cycles;	}
	
	void Reset()
	{
		extSeqNum	= 0;
		cycles		= 0;
	}
	
private:
	X extSeqNum	= 0;
	X cycles	= 0;
};

#endif /* WRAPEXTENDER_H */

