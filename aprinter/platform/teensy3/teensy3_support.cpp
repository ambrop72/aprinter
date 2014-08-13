#include <stdint.h>

extern "C" {
    // Dummy definitions to make Teensy startup code happy.
    uint32_t volatile systick_millis_count = 0;
    void _init_Teensyduino_internal_ (void) {}
    void rtc_set (unsigned long t) {}
    void yield (void) {}

    __attribute__((used)) void _init (void) {}
}
