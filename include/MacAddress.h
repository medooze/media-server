#ifndef MAC_ADDRESS_H
#define MAC_ADDRESS_H

#include <cstdint>
#include <array>
#include <string>

class MacAddress : public std::array<uint8_t, 6> {
public:
	/**
	 * @brief Parse an "xx:xx:xx:xx:xx:xx" string into a MacAddress
	 */
	static MacAddress Parse(const std::string& str);
};

#endif /* MAC_ADDRESS_H */
