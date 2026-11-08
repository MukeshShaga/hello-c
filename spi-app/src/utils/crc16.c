/* crc16.c - CRC-16/CCITT-FALSE, bitwise implementation */
#include "crc16.h"

uint16_t crc16_ccitt(const void *data, size_t len)
{
    const uint8_t *p = data;
    uint16_t crc = 0xFFFF;

    while (len--) {
        crc ^= (uint16_t)(*p++) << 8;
        for (int bit = 0; bit < 8; bit++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
    }
    return crc;
}
