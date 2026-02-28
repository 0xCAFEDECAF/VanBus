/*
 * VanBus packet receiver for ESP8266
 *
 * Written by Erik Tromp
 *
 * Version 0.5.0 - March, 2026
 *
 * MIT license, all text above must be included in any redistribution.
 */

#ifndef VanBusVersion_h
#define VanBusVersion_h

#define VAN_BUS_VERSION "0.5.0"

#define VAN_BUS_VERSION_MAJOR 0
#define VAN_BUS_VERSION_MINOR 5
#define VAN_BUS_VERSION_PATCH 0

#define VAN_BUS_VERSION_INT 000005000

#define VAN_BUS_RX_VERSION VAN_BUS_VERSION_INT

// Special stuff

#ifdef ARDUINO_ARCH_ESP32
  #if ! defined ESP_ARDUINO_VERSION
    #define ESP_ARDUINO_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

    // Not really important which patch level we assume; use 6 which is the last in 1.0 .
    #define ESP_ARDUINO_VERSION_MAJOR 1
    #define ESP_ARDUINO_VERSION_MINOR 0
    #define ESP_ARDUINO_VERSION_PATCH 6

    #define ESP_ARDUINO_VERSION \
        ESP_ARDUINO_VERSION_VAL(ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH)

  #endif
#endif

#endif /* VanBusVersion_h */
