#ifndef _FRAMESCALER_H_
#define _FRAMESCALER_H_
extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}
#include <config.h>

class FrameScaler
{
public:
	FrameScaler();
	~FrameScaler();
	int SetResize(int srcWidth,int srcHeight,int srcLineWidth,int dstWidth,int dstHeight,int dstLineWidth,bool keepAspectRatio = true);
	int Resize(BYTE *srcY,BYTE *srcU,BYTE *srcV,BYTE *dstY, BYTE *dstU, BYTE *dstV);
	int Resize(BYTE *src,DWORD srcWidth,DWORD srcHeight,BYTE *dst,DWORD dstWidth,DWORD dstHeight,bool keepAspectRatio = true);

private:
	struct SwsContext* resizeCtx;
	int     resizeWidth;
	int     resizeHeight;
	int	resizeDstWidth;
	int     resizeDstHeight;
	int     resizeDstAdjustedHeight;
	int     resizeDstAdjustedWidth;
	int	resizeDstLineWidth;
	int     resizeSrc[3];
	int     resizeDst[3];
	int     resizeFlags;
	int	tmpWidth;
	int	tmpHeight;
	int	tmpBufferSize;
	BYTE*	tmpBuffer;
	BYTE*	tmpY;
	BYTE* 	tmpU;
	BYTE* 	tmpV;

};

#endif
