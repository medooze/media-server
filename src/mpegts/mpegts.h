#ifndef MPEGTS_H_
#define MPEGTS_H_

#include <optional>
#include "BufferReader.h"
#include "BufferWritter.h"
#include "Encodable.h"

namespace mpegts
{

constexpr size_t MPEGTSPacketSize = 188;

enum AdaptationFieldControl
{
	Reserved = 0,
	PayloadOnly = 1,
	AdaptationFieldOnly = 2,
	AdaptationFiedlAndPayload = 3
};

struct Header : public Encodable
 {
	uint8_t  syncByte = 0;
	bool	 transportErrorIndication = false;
	bool	 payloadUnitStartIndication = false;
	bool	 transportPriority = false;
	uint16_t packetIdentifier = 0;
	uint8_t  transportScramblingControl = 0;
	AdaptationFieldControl adaptationFieldControl = AdaptationFieldControl::Reserved;
	uint8_t  continuityCounter = 0;

	// Encodable overrides
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static Header Parse(BufferReader& reader);
	
	void Dump() const;
};

struct AdaptationField : public Encodable
{
	bool discontinuityIndicator = false;
	bool randomAccessIndicator = false;
	bool elementaryStreamPriorityIndicator = false;
	bool pcrFlag = false;
	bool opcrFlag = false;
	bool splicingPointFlag = false;
	bool transportPrivateDataFlag = false;
	bool adaptationFieldExtensionFlag = false;
	
	std::optional<uint64_t> pcr;
	
	// Encodable overrides
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static AdaptationField Parse(BufferReader& reader);
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

constexpr uint8_t CreateAudioStreamID(uint8_t streamNumber) { return 0xc0 | (streamNumber & 0x1f); };
constexpr uint8_t CreateVideoStreamID(uint8_t streamNumber) { return 0xe0 | (streamNumber & 0xf); };

enum PTSDTSIndicator
{
	None = 0,
	Forbiden = 1,
	OnlyPTS = 2,
	Both = 3
};

struct Header : public Encodable
{
	uint32_t packetStartCodePrefix = 0x000001;
	uint8_t  streamId = 0;
	uint16_t packetLength = 0;

	// Encodable overrides
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static Header Parse(BufferReader& reader);
};

struct HeaderExtension : public Encodable
{
	uint8_t markerBits = 0x2;
	uint8_t scramblingControl = 0;
	bool	priority = false;
	bool	dataAlignmentIndicator = false;
	bool	copyright = false;
	bool	original = false;
	PTSDTSIndicator ptsdtsIndicator = PTSDTSIndicator::None;
	bool	escrFlag = false;
	bool	rateFlag = false;
	bool	trickModeFlag = false;
	bool	aditionalInfoFlag = false;
	bool	crcFlag = false;
	bool	extensionFlag = false;

	std::optional<uint64_t> pts = {};
	std::optional<uint64_t> dts = {};
	
	size_t	stuffingCount = 0;

	// Encodable overrides
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
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

struct Header : public Encodable
{
	uint16_t syncWord = 0xfff;
	bool     version = false;
	uint8_t  layer = 0;
	bool     protectionAbsence = false;
	uint8_t  objectType = 1;
	uint8_t  samplingFrequency = 0;
	bool     priv = false;
	uint8_t  channelConfiguration = 0;
	bool     originality = false;
	bool     home = false;
	bool     copyright = false;
	bool     copyrightStart = false;
	uint16_t frameLength = 0;
	uint16_t bufferFullness = 0;
	uint8_t  numberOfFramesMinus1 = 0;
	uint16_t crc = 0;

	// Encodable overrides
	void Encode(BufferWritter& writer) override;
	size_t Size() const override;
	
	static Header Parse(BufferReader& reader);
};
}; //namespace mpegts::pes::adts
}; //namespace mpegts::pes
}; //namespace mpegts

#endif //MPEGTS_H_
