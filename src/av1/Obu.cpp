
#include "Obu.h"

#include <optional>
 	
DWORD ObuHeader::Serialize(BufferWritter& writter) const
{
	
	if (!writter.Assert(GetSize())) return 0;

	auto mark = writter.Mark();

	BitWritter bitwritter(writter, extension.has_value() ? 2 : 1);
	bitwritter.Put(1, 0); //forbidden
	bitwritter.Put(4, type);
	bitwritter.Put(1, extension.has_value());
	bitwritter.Put(1, length.has_value());
	bitwritter.Put(1, 0); //reserved

	if (extension.has_value())
	{
		bitwritter.Put(3, extension->layerInfo.temporalLayerId);
		bitwritter.Put(2, extension->layerInfo.spatialLayerId);
		bitwritter.Put(3, 0); //extended reserved
	}

	bitwritter.Flush();

	if (length.has_value())
		writter.EncodeLeb128(length.value());

	//Return consumed bytes
	return writter.GetOffset(mark);
}

size_t ObuHeader::GetSize() const
{
	size_t sz = 1;
	if (extension.has_value()) sz++;
	//Leb128 size
	if (length.has_value())
	{
		auto value = length.value();
		while (value >= 0x80)
		{
			value >>= 7;
			++sz;
		}
		++sz;
	}
	return sz;
}


bool ObuHeader::Parse(BufferReader& reader)
{
	if (!reader.Assert(1)) return false;
	
	BitReader bitreader(reader.PeekData(),reader.GetLeft());
	
	auto forbiddenBit = bitreader.Get(1);
	if (forbiddenBit != 0) return false;
	
	type = bitreader.Get(4);
	bool extensionFlag = bitreader.Get(1);
	bool hasSizeField = bitreader.Get(1);
	bitreader.Skip(1);
	
	if (extensionFlag && bitreader.Left()>5)
	{
		extension = Extension{
			LayerInfo{
				static_cast<uint8_t>(bitreader.Get(3)),
				static_cast<uint8_t>(bitreader.Get(2))
			}
		};
		bitreader.Skip(3);
	}

	reader.Skip(bitreader.Flush());

	if (hasSizeField)
		length = reader.DecodeLev128();

	return true;
}
