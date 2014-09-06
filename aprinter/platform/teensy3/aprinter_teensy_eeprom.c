#include <stdint.h>

#include <eeprom.c>

void eeprom_write_byte_nonblock (uint32_t offset, uint8_t value)
{
    if (FlexRAM[offset] != value) {
        FlexRAM[offset] = value;
    }
}

void eeprom_write_word_nonblock (uint32_t offset, uint16_t value)
{
    if (*(uint16_t *)(&FlexRAM[offset]) != value) {
        *(uint16_t *)(&FlexRAM[offset]) = value;
    }
}

void eeprom_write_dword_nonblock (uint32_t offset, uint32_t value)
{
    if (*(uint32_t *)(&FlexRAM[offset]) != value) {
        *(uint32_t *)(&FlexRAM[offset]) = value;
    }
}
