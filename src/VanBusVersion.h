/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.4.2 - September, 2025
 *
 * MIT license, all text above must be included in any redistribution.
 */

#ifndef VanBusVersion_h
#define VanBusVersion_h

#define VAN_BUS_VERSION "0.4.2"

#define VAN_BUS_VERSION_MAJOR 0
#define VAN_BUS_VERSION_MINOR 4
#define VAN_BUS_VERSION_PATCH 2

#define VAN_BUS_VERSION_INT 000004002

#define VAN_BUS_RX_VERSION VAN_BUS_VERSION_INT

// Special stuff

#ifdef ARDUINO_ARCH_ESP32
  #if ! defined ESP_ARDUINO_VERSION
    #define ESP_ARDUINO_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

    // Not really important which patch level we assume; use 6 which is the last in 1.0 .
    #define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(1, 0, 6)

  #endif
#endif

#endif /* VanBusVersion_h */
