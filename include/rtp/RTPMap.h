#ifndef RTPMAP_H
#define RTPMAP_H
#include "config.h"
#include "log.h"
#include "media.h"

class RTPMap
{
public:
        RTPMap() { clear(); }

        bool empty() const
        {
                return firstCodecType == NotFound;
        }

        void clear()
        {
                forward.fill(NotFound);
                reverse.fill(NotFound);
                firstCodecType = NotFound;
        }

        BYTE GetFirstCodecType()
        {
                return firstCodecType;
        }

        bool SetCodecForType(BYTE type, BYTE codec)
        {
                //Do not allow to remove codec or types
                if (type == NotFound || codec == NotFound)
                        //Error
                        return false;

                //Store type<->codec mapping
                forward[type] = codec;

                //Do not override the codec to type lookup if already have a mapping with the same codec
                if (reverse[codec] == NotFound || forward[reverse[codec]] != codec) reverse[codec] = type;

                //Store first type
                if (firstCodecType == NotFound) firstCodecType = type;

                //Done
                return true;
        }

        BYTE GetCodecForType(BYTE type) const { return forward[type]; }
        BYTE GetTypeForCodec(BYTE codec) const { return reverse[codec]; }
        bool HasCodec(BYTE codec) const { return reverse[codec] != NotFound; }

        void Dump(MediaFrame::Type media) const;
public:
        static constexpr BYTE NotFound = -1;
private:
        std::array<BYTE, 256> forward;
        std::array<BYTE, 256> reverse;
        BYTE firstCodecType = NotFound;

};


#endif /* RTPMAP_H */