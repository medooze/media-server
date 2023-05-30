/* 
 * File:   crc32.h
 * Author: Sergio
 *
 * Created on 8 de noviembre de 2012, 11:54
 */

#ifndef CRC32_H_
#define	CRC32_H_

#include "config.h"

class CRC32Calc
{
public:
	CRC32Calc()
	{
		//Check if inited globally
		if (!inited)
		{
			for (DWORD i = 0; i < 256; ++i) {
				DWORD c = i;
				for (DWORD j = 0; j < 8; ++j) {
					if (c & 1) {
						c = 0xEDB88320 ^ (c >> 1);
					} else {
						c >>= 1;
					}
				}
				table[i] = c;
			}
			//Initied
			inited = true;
		}

		crc = 0;
	}

	DWORD Update(const BYTE *data, DWORD size)
	{
		DWORD c = crc ^ 0xFFFFFFFF;
		for (DWORD i = 0; i < size; ++i) 
			c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
		crc =  c ^ 0xFFFFFFFF;
		return crc;
	}
private:
	static DWORD table[256];
	static bool inited;
private:
	DWORD crc = 0;
};


#endif	/* CRC32_H */
