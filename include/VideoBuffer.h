#ifndef VIDEOBUFFER_H_
#define VIDEOBUFFER_H_
#include "config.h"
#include <memory>

class Plane
{
public:
	Plane(DWORD width, DWORD height) :
		//64 bytes aligned stride
		stride((width / 64 + 1) * 64),
		width(width),
		height(height),
		size((height + 1) * stride + 64)
	{
#ifdef HAVE_STD_ALIGNED_ALLOC
		buffer = (uint8_t*)std::aligned_alloc(64, size);
#else
		buffer = (uint8_t*)std::malloc(size);
#endif
	}
	~Plane()
	{
		std::free(buffer);
	}

	const BYTE* GetData() const	{ return buffer;	}
	BYTE* GetData()			{ return buffer;	}
	DWORD GetStride() const		{ return stride;	}
	DWORD GetWidth() const		{ return width;		}
	DWORD GetHeight() const		{ return height;	}
	void Fill(BYTE color)
	{
		memset(buffer, color, size);
	}

private:
	DWORD stride = 0;
	DWORD width = 0;
	DWORD height = 0;
	uint8_t* buffer = nullptr;
	size_t size = 0;
};

class VideoBuffer
{
public:
	using shared = std::shared_ptr<VideoBuffer>;
	using const_shared = std::shared_ptr<const VideoBuffer>;

	enum class ColorRange {
		Unknown=-1,
		Partial=0,
		Full=1
	};
	enum class ColorSpace {
		Unknown = 0,
		BT601 = 1, 
		BT709 = 2,
		SMPTE170 = 3, 
		SMPTE240 = 4,
		BT2020 = 5,
		SRGB = 7,
	};
public:
	VideoBuffer() = default;
	VideoBuffer(DWORD width, DWORD height) : 
		planeY(width, height),
		planeU((width + 1) / 2, (height + 1) / 2),
		planeV((width + 1) / 2, (height + 1) / 2),
		width(width),
		height(height)

	{
	}

	const Plane& GetPlaneY() const { return planeY; }
	const Plane& GetPlaneU() const { return planeU; }
	const Plane& GetPlaneV() const { return planeV; }

	Plane& GetPlaneY() { return planeY; }
	Plane& GetPlaneU() { return planeU; }
	Plane& GetPlaneV() { return planeV; }

	DWORD GetWidth() const	{ return width;		}
	DWORD GetHeight() const { return height;	}

	void Fill(BYTE y, BYTE u, BYTE v)
	{
		planeY.Fill(y);
		planeU.Fill(u);
		planeV.Fill(v);
	}

	bool IsInterlaced() const { return isInterlaced; }
	void SetInterlaced(bool isInterlaced) { this->isInterlaced = isInterlaced; }

	void SetColorRange(ColorRange colorRange) { this->colorRange = colorRange; }
	void SetColorSpace(ColorSpace colorSpace) { this->colorSpace = colorSpace; }

	ColorRange GetColorRange() const { return colorRange; }
	ColorSpace GetColorSpace() const { return colorSpace; }

private:
	
	Plane	planeY;
	Plane	planeU;
	Plane	planeV;
	DWORD	width = 0;
	DWORD	height = 0;
	bool	isInterlaced = false;
	ColorSpace colorSpace = ColorSpace::Unknown;
	ColorRange colorRange = ColorRange::Unknown;
	
	
	
};

#endif // !VIDEOBUFFER_H_