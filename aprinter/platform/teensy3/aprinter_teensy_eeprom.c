#include <eeprom.c>

int eeprom_write_byte_nonblock (uint32_t offset, uint8_t value)
{
    if (FlexRAM[offset] != value) {
        FlexRAM[offset] = value;
        return 1;
    }
    return 0;
}
