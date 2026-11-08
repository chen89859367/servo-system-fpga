#ifndef AQBISS_SENSOR_H
#define AQBISS_SENSOR_H

#include <stdint.h>

#define AQBISS_POSITION_BITS 15u
#define AQBISS_COUNTS_PER_REV 32768u
#define AQBISS_FRAME_BITS 25u

/* Confirm the CRC initialization with the exact Netzer UG206 revision. */
#ifndef BISS_CRC_INIT
#define BISS_CRC_INIT 0x00u
#endif
#define BISS_CRC_POLY 0x05u /* Lower 5 bits of x^5 + x^2 + 1. */

typedef struct {
    uint16_t position;
    uint8_t error_bits;
    uint8_t crc;
    uint8_t crc_expected;
    uint8_t m_bit;
    uint8_t start;
    uint8_t stop;
    uint8_t crc_ok;
} aqbiss_frame_t;

void aqbiss_init(void);
/* Captures 25 bits. Returns 0 on capture, -1 for invalid argument. */
int aqbiss_read(aqbiss_frame_t *frame);

#endif
