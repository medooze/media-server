#ifndef H26xNAL_H
#define	H26xNAL_H

#include <cstdint>
#include <limits>
#include <optional>
#include <functional>


#include "config.h"
#include "log.h"
#include "tools.h"
#include "Buffer.h"
#include "BufferReader.h"
#include "bitstream/BitReader.h"

constexpr uint32_t AnnexBStartCode = 0x01;

// H.264 NAL logic that can be shared with H.265 (mostly emulation prevention, annex B stream)

/**
 * This reader consumes an RBSP (usually the payload of an H.26x NALU)
 * and provides the unescaped bytes of the SODB, by skipping over
 * `emulation_prevention_three_byte`s as necessary (H.264 section 7.4.1.1 step 1).
 * Other codes (00 00 [00..02]) like start codes, as well as incomplete codes,
 * are currently passed through.
 */
class RbspReader
{
public:
	RbspReader(const uint8_t* data, const size_t size) :
		data(data),
		size(size)
	{
		this->pos = 0;
	}

	inline bool   Assert(size_t num) const
	{
		DWORD len = 0;
		DWORD i = pos;
		DWORD z = zeros;

		while (i < size)
		{
			if (z >= 2 && data[i] == 0x03)
			{
				//Skip input
				i++;
				//No zeros
				z = 0;
			}
			else
			{
				//Check consecutive zeros
				if (data[i] == 0)
					z++;
				//move pointers
				i++;
				len++;
			}
			if (len == num)
				return true;
		}
		return false;
	}

	inline uint8_t  Peek1() { return Peek(1); }
	inline uint16_t Peek2() { return Peek(2); }
	inline uint32_t Peek3() { return Peek(3); }
	inline uint32_t Peek4() { return Peek(4); }

	inline void Skip(size_t num)
	{
		//Don't update pos if we have not consumed any bytes
		if (!num) return;

		DWORD len = 0;
		DWORD z = zeros;

		while (pos < size)
		{
			if (z >= 2 && data[pos] == 0x03)
			{
				//Skip input
				pos++;
				//No zeros
				z = 0;
			}
			else
			{
				//Check consecutive zeros
				if (data[pos] == 0)
					z++;
				//move pointers
				pos++;
				len++;
			}
			if (len == num)
				return;
		}

		throw std::range_error("couldn't skip more bytes");
	}

protected:
	uint32_t Peek(size_t num)
	{
		uint32_t value = 0;

		DWORD len = 0;
		DWORD i = pos;
		DWORD z = zeros;

		while (i < size)
		{
			if (z >= 2 && data[i] == 0x03)
			{
				//Skip input
				i++;
				//No zeros
				z = 0;
			}
			else 
			{
				//Acu value
				value = value << 8 | data[i];
				//Check consecutive zeros
				if (data[i] == 0)
					z++;
				//move pointers
				i++;
				len++;
			}
			if (len == num)
				return value;
		}
		
		throw std::range_error("couldn't peek more bytes");
	}
private:
	const uint8_t* data = nullptr;
	size_t size = 0;
	size_t pos = 0;
	size_t zeros = 0;
};

using RbspBitReader = BitReaderBase<RbspReader>;


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

inline Buffer NalUnescapeRbsp(const BYTE* src, DWORD size)
{
	Buffer escaped(size);
	DWORD len = NalUnescapeRbsp(escaped.GetData(), src, size);
	escaped.SetSize(len);
	return escaped;
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

				try 
				{
					//Process current NALU
					onNalu(nalu);
				}
				catch (const std::exception& e)
				{
					Warning("-NalSliceAnnexB() exception on onNalu()\n");
				}
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
		try
		{
			//Process current NALU
			onNalu(nalu);
		}
		catch (const std::exception& e)
		{
			Warning("-NalSliceAnnexB() exception on onNalu()\n");
		}
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
		if (nalSize == 1 && !pos)
			//Done
			return;
		//Set annexB start code
		set4(data, pos, 1);
		//Next
		pos += 4 + nalSize;
	}
}

inline void NalToAnnexB(Buffer& buffer)
{
	NalToAnnexB(buffer.GetData(), buffer.GetSize());
}


#endif	/* H264NAL_H */
