#ifndef EXPGOLOMBDECODER_H_
#define	EXPGOLOMBDECODER_H_
#include "config.h"
#include "tools.h"
#include <stdexcept>
#include "bitstream/BitReader.h"

class ExpGolombDecoder
{
public:
	static inline  DWORD Decode(BitReader &reader)
	{
		//No len yet
		DWORD len = 0;
		//Count zeros
		while (reader.Left() && !reader.Get(1))
			//Increase len
			++len;
		//Check 0
		if (!len) return 0;
		//Get the exp
		DWORD value = reader.Get(len);
		
		//Calc value
		return (1<<len)-1+value;
	}

	static inline int DecodeSE(BitReader &reader)
	{
		//Get code num
		DWORD codeNum = Decode(reader);
		//Conver to signed
		return codeNum & 0x01 ? codeNum>>1 : -(codeNum>>1);
	}
};

#endif	/* EXPGOLOMBDECODER_H_ */
