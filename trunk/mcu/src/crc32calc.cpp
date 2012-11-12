#include "crc32calc.h"

DWORD CRC32Calc::table[256];
bool CRC32Calc::inited = false;