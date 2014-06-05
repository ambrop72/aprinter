#ifndef AMBROLIB_STM32F4DISCOVERY_PINS_H
#define AMBROLIB_STM32F4DISCOVERY_PINS_H

#include <aprinter/system/Stm32f4Pins.h>

#include <aprinter/BeginNamespace.h>

using DiscoveryPinLedGreen = Stm32f4Pin<Stm32f4PortD, 12>;
using DiscoveryPinLedOrange = Stm32f4Pin<Stm32f4PortD, 13>;
using DiscoveryPinLedRed = Stm32f4Pin<Stm32f4PortD, 14>;
using DiscoveryPinLedBlue = Stm32f4Pin<Stm32f4PortD, 15>;

#include <aprinter/EndNamespace.h>

#endif
