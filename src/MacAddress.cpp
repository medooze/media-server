#include "MacAddress.h"

#include <stdexcept>
#include <cctype>

MacAddress MacAddress::Parse(const std::string& str) {
	if (str.length() != 17)
		throw std::invalid_argument("invalid MAC address: incorrect length");
	auto HexDigit = [](unsigned char c) {
		static std::string digits = "0123456789abcdef";
		if (auto pos = digits.find(std::tolower(c)); pos != std::string::npos)
			return pos;
		throw std::invalid_argument("invalid MAC address: incorrect hex digit");
	};
	MacAddress result;
	for (size_t i = 0; i < 6; i++)
	{
		result[i] = HexDigit(str[i * 3 + 0]) << 4 | HexDigit(str[i * 3 + 1]);
		if (i < 5 && str[i * 3 + 2] != ':')
			throw std::invalid_argument("invalid MAC address: incorrect separator");
	}
	return result;
}
