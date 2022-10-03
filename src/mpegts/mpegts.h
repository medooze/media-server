#ifndef MPEGTS_H_
#define MPEGTS_H_

#include <optional>
#include "BufferReader.h"

namespace mpegts
{

enum AdaptationFieldControl
{
	Reserved = 0,
	PayloadOnly = 1,
	AdaptationFieldOnly = 2,
	AdaptationFiedlAndPayload = 3
};

struct Header
 {
	uint8_t  syncByte;
	bool	 transportErrorIndication;
	bool	 payloadUnitStartIndication;
	bool	 transportPriority;
	uint16_t packetIdentifier;
	uint8_t  transportScramblingControl;
	AdaptationFieldControl adaptationFieldControl;
	uint8_t  continuityCounter;

	static Header Parse(BufferReader& reader);
	void Dump() const;

};

struct AdaptationField
{
	static AdaptationField Parse(BufferReader& reader);

	bool discontinuityIndicator;
	bool randomAccessIndicator;
	bool elementaryStreamPriorityIndicator;
	bool pcrFlag;
	bool opcrFlag;
	bool splicingPointFlag;
	bool transportPrivateDataFlag;
	bool adaptationFieldExtensionFlag;

};

struct Packet
{
	Header header;
	std::optional<AdaptationField> adaptationField = {};
	std::optional<uint8_t> payloadPointer = {};

	static Packet Parse(BufferReader& reader);
};


namespace pes
{

enum StreamType
{
	Invalid,
	PrivateStream1,
	PaddingStream,
	PrivateStream2,
	AudioStream,
	VideoStream
};

StreamType GetStreamType(const uint8_t& streamId);

enum PTSDTSIndicator
{
	None = 0,
	Forbiden = 1,
	OnlyPTS = 2,
	Both = 3
};

struct Header
{
	uint32_t packetStartCodePrefix;
	uint8_t  streamId;
	uint16_t packetLength;

	static Header Parse(BufferReader& reader);
};

struct HeaderExtension
{

	uint8_t markerBits;
	uint8_t scramblingControl;
	bool	priority;
	bool	dataAlignmentIndicator;
	bool	copyrigth;
	bool	original;
	PTSDTSIndicator ptsdtsIndicator;
	bool	escrFlag;
	bool	rateFlag;
	bool	trickModeFlag;
	bool	aditionalInfoFlag;
	bool	crcFlag;
	bool	extensionFlag;
	uint8_t remainderHeaderLength;

	std::optional<uint64_t> pts = {};
	std::optional<uint64_t> dts = {};

	static HeaderExtension Parse(BufferReader& reader);
};

struct Packet
{
	Header	header;
	std::optional<HeaderExtension> headerExtension = {};

	static Packet Parse(BufferReader& reader);
	static StreamType GetStreamType(const uint8_t& streamId);
};

namespace adts 
{

struct Header
{
	uint16_t syncWord;
	bool     version;
	uint8_t  layer;
	bool     protectionAbsence;
	uint8_t  objectType;
	uint8_t  samplingFrequency;
	bool     priv;
	uint8_t  channelConfiguration;
	bool     originality;
	bool     home;
	bool     copyrigth;
	bool     copyrigthStart;
	uint16_t frameLength;
	uint16_t bufferFullness;
	uint8_t  numberOfFrames;
	uint16_t crc;

	static Header Parse(BufferReader& reader);
};
}; //namespace mpegts::pes::adts
}; //namespace mpegts::pes
}; //namespace mpegts

#endif //MPEGTS_H_