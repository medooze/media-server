#ifndef MPEGTS_H_
#define MPEGTS_H_

#include <optional>
#include "BufferReader.h"
#include "BufferWritter.h"

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
	uint8_t  syncByte = 0;
	bool	 transportErrorIndication = false;
	bool	 payloadUnitStartIndication = false;
	bool	 transportPriority = false;
	uint16_t packetIdentifier = 0;
	uint8_t  transportScramblingControl = 0;
	AdaptationFieldControl adaptationFieldControl = AdaptationFieldControl::Reserved;
	uint8_t  continuityCounter = 0;

	void Encode(BufferWritter& writer);
	static Header Parse(BufferReader& reader);
	
	
	void Dump() const;

};

struct AdaptationField
{
	void Encode(BufferWritter& writer);
	static AdaptationField Parse(BufferReader& reader);

	uint8_t adaptationFieldLength = 0;
	bool discontinuityIndicator = false;
	bool randomAccessIndicator = false;
	bool elementaryStreamPriorityIndicator = false;
	bool pcrFlag = false;
	bool opcrFlag = false;
	bool splicingPointFlag = false;
	bool transportPrivateDataFlag = false;
	bool adaptationFieldExtensionFlag = false;

};

struct Packet
{
	Header header;
	std::optional<AdaptationField> adaptationField = {};
	std::optional<uint8_t> payloadPointer = {};

	void Encode(BufferWritter& writer);
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
	uint32_t packetStartCodePrefix = 0x000001;
	uint8_t  streamId = 0;
	uint16_t packetLength = 0;

	void Encode(BufferWritter& writer);
	static Header Parse(BufferReader& reader);
};

struct HeaderExtension
{

	uint8_t markerBits = 0x10;
	uint8_t scramblingControl = 0;
	bool	priority = false;
	bool	dataAlignmentIndicator = false;
	bool	copyright = false;
	bool	original = false;
	PTSDTSIndicator ptsdtsIndicator = PTSDTSIndicator::None;
	bool	escrFlag = flase;
	bool	rateFlag = false;
	bool	trickModeFlag = false;
	bool	aditionalInfoFlag = false;
	bool	crcFlag = false;
	bool	extensionFlag = false;
	uint8_t remainderHeaderLength = 0;

	std::optional<uint64_t> pts = {};
	std::optional<uint64_t> dts = {};

	void Encode(BufferWritter& writer);
	static HeaderExtension Parse(BufferReader& reader);
};

struct Packet
{
	Header	header;
	std::optional<HeaderExtension> headerExtension = {};

	void Encode(BufferWritter& writer);
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
	bool     copyright;
	bool     copyrightStart;
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