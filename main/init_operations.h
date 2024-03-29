#pragma once

#include <stdint.h>

typedef enum
{
    BEGIN_WRITE,
    WRITE_COMMAND_8,
    WRITE_COMMAND_16,
    WRITE_COMMAND_BYTES,
    WRITE_DATA_8,
    WRITE_DATA_16,
    WRITE_BYTES,
    WRITE_C8_D8,
    WRITE_C8_D16,
    WRITE_C8_BYTES,
    WRITE_C16_D16,
    END_WRITE,
    DELAY,
} spi_operation_type_t;

// Init code for 480x480 round TL021WVC02 display
static const uint8_t TL021WVC02_init_operations[] = {BEGIN_WRITE,
                                                     WRITE_COMMAND_8,
                                                     0xFF,
                                                     WRITE_BYTES,
                                                     5,
                                                     0x77,
                                                     0x01,
                                                     0x00,
                                                     0x00,
                                                     0x10,
                                                     WRITE_C8_D16,
                                                     0xC0,
                                                     0x3B,
                                                     0x00,
                                                     WRITE_C8_D16,
                                                     0xC1,
                                                     0x0B,
                                                     0x02, // VBP
                                                     WRITE_C8_D16,
                                                     0xC2,
                                                     0x00,
                                                     0x02,
                                                     WRITE_C8_D8,
                                                     0xCC,
                                                     0x10,
                                                     WRITE_C8_D8,
                                                     0xCD,
                                                     0x08,
                                                     WRITE_COMMAND_8,
                                                     0xB0, // Positive Voltage Gamma Control
                                                     WRITE_BYTES,
                                                     16,
                                                     0x02,
                                                     0x13,
                                                     0x1B,
                                                     0x0D,
                                                     0x10,
                                                     0x05,
                                                     0x08,
                                                     0x07,
                                                     0x07,
                                                     0x24,
                                                     0x04,
                                                     0x11,
                                                     0x0E,
                                                     0x2C,
                                                     0x33,
                                                     0x1D,

                                                     WRITE_COMMAND_8,
                                                     0xB1, // Negative Voltage Gamma Control
                                                     WRITE_BYTES,
                                                     16,
                                                     0x05,
                                                     0x13,
                                                     0x1B,
                                                     0x0D,
                                                     0x11,
                                                     0x05,
                                                     0x08,
                                                     0x07,
                                                     0x07,
                                                     0x24,
                                                     0x04,
                                                     0x11,
                                                     0x0E,
                                                     0x2C,
                                                     0x33,
                                                     0x1D,

                                                     WRITE_COMMAND_8,
                                                     0xFF,
                                                     WRITE_BYTES,
                                                     5,
                                                     0x77,
                                                     0x01,
                                                     0x00,
                                                     0x00,
                                                     0x11,

                                                     WRITE_C8_D8,
                                                     0xB0,
                                                     0x5d, // 5d
                                                     WRITE_C8_D8,
                                                     0xB1,
                                                     0x43, // VCOM amplitude setting
                                                     WRITE_C8_D8,
                                                     0xB2,
                                                     0x81, // VGH Voltage setting, 12V
                                                     WRITE_C8_D8,
                                                     0xB3,
                                                     0x80,

                                                     WRITE_C8_D8,
                                                     0xB5,
                                                     0x43, // VGL Voltage setting, -8.3V

                                                     WRITE_C8_D8,
                                                     0xB7,
                                                     0x85,
                                                     WRITE_C8_D8,
                                                     0xB8,
                                                     0x20,

                                                     WRITE_C8_D8,
                                                     0xC1,
                                                     0x78,
                                                     WRITE_C8_D8,
                                                     0xC2,
                                                     0x78,

                                                     WRITE_C8_D8,
                                                     0xD0,
                                                     0x88,

                                                     WRITE_COMMAND_8,
                                                     0xE0,
                                                     WRITE_BYTES,
                                                     3,
                                                     0x00,
                                                     0x00,
                                                     0x02,

                                                     WRITE_COMMAND_8,
                                                     0xE1,
                                                     WRITE_BYTES,
                                                     11,
                                                     0x03,
                                                     0xA0,
                                                     0x00,
                                                     0x00,
                                                     0x04,
                                                     0xA0,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x20,
                                                     0x20,

                                                     WRITE_COMMAND_8,
                                                     0xE2,
                                                     WRITE_BYTES,
                                                     13,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xE3,
                                                     WRITE_BYTES,
                                                     4,
                                                     0x00,
                                                     0x00,
                                                     0x11,
                                                     0x00,

                                                     WRITE_C8_D16,
                                                     0xE4,
                                                     0x22,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xE5,
                                                     WRITE_BYTES,
                                                     16,
                                                     0x05,
                                                     0xEC,
                                                     0xA0,
                                                     0xA0,
                                                     0x07,
                                                     0xEE,
                                                     0xA0,
                                                     0xA0,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xE6,
                                                     WRITE_BYTES,
                                                     4,
                                                     0x00,
                                                     0x00,
                                                     0x11,
                                                     0x00,

                                                     WRITE_C8_D16,
                                                     0xE7,
                                                     0x22,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xE8,
                                                     WRITE_BYTES,
                                                     16,
                                                     0x06,
                                                     0xED,
                                                     0xA0,
                                                     0xA0,
                                                     0x08,
                                                     0xEF,
                                                     0xA0,
                                                     0xA0,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xEB,
                                                     WRITE_BYTES,
                                                     7,
                                                     0x00,
                                                     0x00,
                                                     0x40,
                                                     0x40,
                                                     0x00,
                                                     0x00,
                                                     0x00,

                                                     WRITE_COMMAND_8,
                                                     0xED,
                                                     WRITE_BYTES,
                                                     16,
                                                     0xFF,
                                                     0xFF,
                                                     0xFF,
                                                     0xBA,
                                                     0x0A,
                                                     0xBF,
                                                     0x45,
                                                     0xFF,
                                                     0xFF,
                                                     0x54,
                                                     0xFB,
                                                     0xA0,
                                                     0xAB,
                                                     0xFF,
                                                     0xFF,
                                                     0xFF,

                                                     WRITE_COMMAND_8,
                                                     0xEF,
                                                     WRITE_BYTES,
                                                     6,
                                                     0x10,
                                                     0x0D,
                                                     0x04,
                                                     0x08,
                                                     0x3F,
                                                     0x1F,

                                                     WRITE_COMMAND_8,
                                                     0xFF,
                                                     WRITE_BYTES,
                                                     5,
                                                     0x77,
                                                     0x01,
                                                     0x00,
                                                     0x00,
                                                     0x13,

                                                     WRITE_C8_D8,
                                                     0xEF,
                                                     0x08,

                                                     WRITE_COMMAND_8,
                                                     0xFF,
                                                     WRITE_BYTES,
                                                     5,
                                                     0x77,
                                                     0x01,
                                                     0x00,
                                                     0x00,
                                                     0x00,

                                                     WRITE_C8_D8,
                                                     0x36,
                                                     0x00,
                                                     WRITE_C8_D8,
                                                     0x3A,
                                                     0x60, // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565

                                                     WRITE_COMMAND_8,
                                                     0x11, // Sleep Out
                                                     END_WRITE,

                                                     DELAY,
                                                     100,

                                                     BEGIN_WRITE,
                                                     WRITE_COMMAND_8,
                                                     0x29, // Display On
                                                     END_WRITE,

                                                     DELAY,
                                                     50};