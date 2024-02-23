#ifndef H26xNAL_H
#define	H26xNAL_H

#include <cstdint>
#include <limits>
#include <optional>
#include <functional>
#include "bitstream.h"

constexpr uint32_t AnnexBStartCode = 0x01;

// H.264 NAL logic that can be shared with H.265 (mostly emulation prevention, annex B stream)

inline DWORD NalUnescapeRbsp(BYTE *dst, const BYTE *src, DWORD size)
{
	DWORD len = 0;
	DWORD i = 0;
	while(i<size)
	{
		//Check if next BYTEs are the escape sequence
		if((i+2<size) && (get3(src,i)==0x03))
		{
			//Copy the first two
			dst[len++] = get1(src,i);
			dst[len++] = get1(src,i+1);
			//Skip the three
			i += 3;
		} else {
			dst[len++] = get1(src,i++);
		}
	}
	return len;
}

inline std::optional<DWORD> NalEscapeRbsp(BYTE *dst, DWORD dstsize, const BYTE *src, DWORD size)
{
	DWORD len = 0;
	DWORD i = 0;
	while(i<size)
	{
		//Check if next BYTEs are an emulation code (00 00 [00-03])
		if((i+3<=size) ? (get3(src,i)<4) : (i+2==size && !get2(src,i)))
		{
			//Emit emulation prevention code (00 00 03)
			if (len+3 > dstsize)
				return std::nullopt;
			set3(dst, len, 3);
			len += 3;
			//Skip the two zeros
			i += 2;
		} else {
			if (len >= dstsize)
				return std::nullopt;
			dst[len++] = get1(src,i++);
		}
	}
	return std::optional(len);
}

inline void NalSliceAnnexB(BufferReader& reader, std::function<void(BufferReader& nalReader)> onNalu)
{
	//Start of current nal unit
	uint32_t start = std::numeric_limits<uint32_t>::max();
	
	//Parse h264 stream
	while (reader.GetLeft())
	{
		uint8_t  startCodeLength = 0;
		//Check if we have a nal start
		if (reader.GetLeft()>4 && reader.Peek4() == 0x01)
			startCodeLength = 4;
		else if (reader.GetLeft()>3 && reader.Peek3() == 0x01)
			startCodeLength = 3;

		//If we found a nal unit and not first
		if (startCodeLength)
		{
			//Get nal end
			uint32_t end = reader.Mark();

			//If we have a nal unit
			if (end > start)
			{
				//Get nalu reader
				BufferReader nalu = reader.GetReader(start, end - start);
				//Process current NALU
				onNalu(nalu);
			}
			//Skip start code
			reader.Skip(startCodeLength);
			//Begin new NALU
			start = reader.Mark();
		} else {
			//Next
			reader.Skip(1);
		}
	}

	//Get nal end
	uint32_t end = reader.Mark();

	//If we have a nal unit
	if (end > start)
	{
		//Get nalu reader
		BufferReader nalu = reader.GetReader(start, end - start);
		//Process current NALU
		onNalu(nalu);
	}
}

inline void NalToAnnexB(BYTE* data, DWORD size)
{
	DWORD pos = 0;

	while (pos < size)
	{
		//Get nal size
		DWORD nalSize = get4(data, pos);
		//If it was already in annex B
		if (nalSize==1 && !pos)
			//Done
			return;
		//Set annexB start code
		set4(data, pos, 1);
		//Next
		pos += 4 + nalSize;
	}
}

#endif	/* H264NAL_H */
