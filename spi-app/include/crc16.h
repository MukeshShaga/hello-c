/* crc16.h - CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) */
#ifndef CRC16_H
#define CRC16_H

#include <stddef.h>
#include <stdint.h>

uint16_t crc16_ccitt(const void *data, size_t len);

#endif /* CRC16_H */
