#ifndef __AQBISS_SENSOR_H
#define __AQBISS_SENSOR_H

#include <stdint.h>

#define AQBISS_POSITION_BITS 15u
#define AQBISS_COUNTS_PER_REV 32768u
#define AQBISS_FRAME_BITS 25u

#ifndef BISS_CRC_INIT
#define BISS_CRC_INIT 0x00u
#endif

#define BISS_CRC_POLY 0x05u

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
int aqbiss_read(aqbiss_frame_t *frame);

#endif
