#ifndef DEINTERLACER_CPP
#define DEINTERLACER_CPP

struct AVFilter;
struct AVFilterContext;
struct AVFilterGraph;
struct AVFilterInOut;

#include "VideoBuffer.h"
#include "VideoBufferPool.h"

class Deinterlacer
{
public:
	Deinterlacer();
	~Deinterlacer();
	int Start(uint32_t width, uint32_t height);
	void Process(const VideoBuffer::const_shared& videoBuffer);
	VideoBuffer::shared GetNextFrame();
private:
	VideoBufferPool	    videoBufferPool;

	uint32_t width	= 0;
	uint32_t height = 0;

	const AVFilter*	 bufferSrc	= nullptr;
	const AVFilter*	 bufferSink	= nullptr;
	AVFilterContext* bufferSrcCtx	= nullptr;
	AVFilterContext* bufferSinkCtx	= nullptr;
	AVFilterGraph*   filterGraph	= nullptr;
	AVFilterInOut*   outputs	= nullptr;
	AVFilterInOut*   inputs		= nullptr;
};

#endif//DEINTERLACER_CPP