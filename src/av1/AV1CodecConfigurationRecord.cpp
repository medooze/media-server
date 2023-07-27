
#include "AV1CodecConfigurationRecord.h"
#include "Obu.h"
#include "BufferWritter.h"

DWORD AV1CodecConfigurationRecord::Serialize(BYTE* buffer,DWORD bufferLength) const
{
	if (bufferLength < GetSize()) return 0;
	
	memcpy(buffer, reinterpret_cast<const void*>(&fields), sizeof(Fields));
	
	buffer += sizeof(Fields);
	bufferLength -= sizeof(Fields);
	
	if (sequenceHeader)
	{
		if (bufferLength >= sequenceHeader->size())
			memcpy(buffer, sequenceHeader->data(), sequenceHeader->size());	
		else
			return 0;
	}
	
	return GetSize();
}

bool AV1CodecConfigurationRecord::Parse(const BYTE* data,DWORD size)
{
	if (size < sizeof(Fields)) return false;
	
	fields = *reinterpret_cast<const Fields*>(data);
	
	data += sizeof(Fields);
	size -= sizeof(Fields);
	
	while (size > 0)
	{
		auto info = GetObuInfo(data, size);
		if (!info) return false;
		
		if (info->obuType == ObuType::ObuSequenceHeader)
		{
			sequenceHeader.emplace(data, data + info->obuSize);
		}
		
		size -= info->obuSize;
	}
	
	return true;
}


size_t AV1CodecConfigurationRecord::GetSize() const
{
	size_t sz = sizeof(Fields);	
	if (sequenceHeader)
	{
		sz += sequenceHeader->size();
	}
	
	return sz;
}