#include "crc32.h"

// This is a standard, bit-wise CRC-32-IEEE 802.3 implementation.
// It's not fast, but it's small and sufficient for the PS4 auth handshake.
uint32_t CRC32_calculate(const uint8_t* data, uint32_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        uint8_t ch = data[i];
        for (uint32_t j = 0; j < 8; j++) {
            uint32_t b = (ch ^ crc) & 1;
            crc >>= 1;
            if (b) crc = crc ^ 0xEDB88320;
            ch >>= 1;
        }
    }
    return ~crc;
}
