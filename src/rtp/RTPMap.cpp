#include "rtp/RTPMap.h"
#include "codecs.h"

void RTPMap::Dump(MediaFrame::Type media) const
{
	Debug("[RTPMap]\n");
	for (size_t i=0; i<255; ++i)
		if (forward[i] != RTPMap::NotFound)
			Debug("\t[Codec name=%s payload=%lu/]\n", GetNameForCodec(media,forward[i]), i);
	Debug("[/RTPMap]\n");
}
