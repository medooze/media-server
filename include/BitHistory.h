#ifndef BITHISTORY_H
#define BITHISTORY_H

template <uint16_t N>
class BitHistory
{
private:
	constexpr static uint16_t Size = N/64 + 1;
public:
	bool Add(uint64_t seq)
	{
		//Check if it is a previous one
		if (seq<=last)
			//Add offset
			return AddOffset(last - seq);
		
		//Get how many we need to add
		uint64_t delta = seq - last;
		
		//Get element and bit
		uint16_t index	= delta / 64;
		uint16_t bit	= delta % 64;
		
		//Move ringbuffer position
		pos = (pos + index) % Size;
		
		//Last should be 1
		uint64_t reminder = 1;
		
		//Update ringbuffer
		for (int i=0;i<Size;++i)
		{
			//Get ringbuffer position
			uint64_t cur = (Size + pos - i) % Size;
			//Clean moved old positions in ringbuffer
			if (i<index)
				history[cur] = 0;
			if (bit)
			{
				//Move bits
				uint64_t overflown = history[cur] >> (64-bit);
				history[cur] = history[cur] << bit | reminder;
				reminder = overflown;
			}
		}
		
		//Store last
		last = seq;
		
		//Done
		return true;
	}
	
	bool AddOffset(uint64_t offset)
	{
		//If it is in window history
		if (offset>=N)
			//Not in window
			return false;
		//Get element and bit
		uint16_t index	= offset / 64;
		uint16_t bit	= offset % 64;
		//Set it
		history[ (Size + pos - index) % Size] |= 1ull << bit;
		
		//Done
		return true;
	}
	
	bool Contains(uint64_t seq)
	{
		//If it is newer that our last one
		if (seq>last)
			return false;
		//Check difference
		return ContainsOffset(last-seq);
	}
	
	bool ContainsOffset(uint64_t offset)
	{
		//If it is not in history window
		if (offset>=N)
			return false;
		
		//Get element and bit
		uint16_t index	= offset / 64;
		uint16_t bit	= offset % 64;
		//Check if we have it in history
		return ( history[ (Size + pos - index) % Size ] >> bit ) & 1;
	}
	
	void Dump()
	{
		std::string str;
		
		for (int i=0;i<Size;++i)
		{
			for (int j=63;j>=0;--j)
				str += (history[(i + pos + 1) % Size] >> j) & 1 ? "1" : "0";
			str += "\n";
		}
		
		Log("[BitHistory last=%llu pos=%llu]\n%s[/BitHistory]\n",last,pos,str.c_str());
	}
	
private:
	std::array<uint64_t,Size> history = {};
	uint64_t last = 0;
	uint16_t pos  = 0;
};
#endif /* BITHISTORY_H */

