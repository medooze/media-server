
#include "AV1CodecConfigurationRecord.h"
#include "Obu.h"
#include "BufferWritter.h"

DWORD AV1CodecConfigurationRecord::Serialize(BufferWritter& writter) const
{
	if (!writter.Assert(GetSize())) return 0;

	auto mark = writter.Mark();

	memcpy(writter.Consume(sizeof(Fields)), reinterpret_cast<const void*>(&fields), sizeof(Fields));
	
	if (sequenceHeader)
		writter.Set(*sequenceHeader);
	
	return writter.GetOffset(mark);
}

bool AV1CodecConfigurationRecord::Parse(BufferReader& reader)
{
	if (!reader.Assert(sizeof(Fields))) return false;
	
	fields = *reinterpret_cast<const Fields*>(reader.GetData(sizeof(Fields)));

	//Init of obu
	auto mark = reader.Mark();
	auto* obu = reader.PeekData();
		
	//Parse header
	ObuHeader obuHeader;
	if (!obuHeader.Parse(reader))
		return false;

	//Get obu body size
	auto payloadSize = obuHeader.length.value_or(reader.GetLeft());
	
	if (!reader.Assert(payloadSize)) return 0;

	//Go to the end of the obu
	reader.Skip(payloadSize);

	auto obuSize = reader.GetOffset(mark);

	//If it is a sequence header
	if (obuHeader.type == ObuType::ObuSequenceHeader)
		sequenceHeader.emplace(obu, obuSize);
	
	return true;
}


size_t AV1CodecConfigurationRecord::GetSize() const
{
	size_t sz = sizeof(Fields);	
	if (sequenceHeader)
		sz += sequenceHeader->GetSize();
	
	return sz;
}