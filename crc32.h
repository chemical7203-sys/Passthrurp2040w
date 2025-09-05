#ifndef _CRC32_H_
#define _CRC32_H_

#include <stdint.h>

uint32_t CRC32_calculate(const uint8_t* data, uint32_t size);

#endif // _CRC32_H_
