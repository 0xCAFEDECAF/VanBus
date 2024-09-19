/*
 * VanBus: VanBusDump - dump all packets, received on a VAN bus, in raw format on the serial port.
 *
 * Written by Erik Tromp
 *
 * Version 0.4.1 - September, 2024
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * -----
 * Wiring
 *
 * You can usually find the VAN bus on pins 2 and 3 of ISO block "A" of your head unit (car radio). See
 * https://en.wikipedia.org/wiki/Connectors_for_car_audio and https://github.com/morcibacsi/esp32_rmt_van_rx .
 *
 * There are various possibilities to hook up a ESP8266/ESP32 based board to your vehicle's VAN bus. For example,
 * when using a "WEMOS D1 mini" board:
 *
 * 1. Use a MCP2551 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    As the MCP2551 has 5V logic, a 5V <-> 3.3V level converter is needed to connect the CRX / RXD / R pin of the
 *    transceiver to (in this example) pin D2 (GPIO 4) of your board.
 *
 * 2. Use a SN65HVD230 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R pin of
 *    the transceiver to (in this example) pin D2 (GPIO 4) of your board.
 *
 * 3. The simplest schematic is not to use a transceiver at all, but connect the VAN DATA line to GrouND using
 *    two 4.7 kOhm resistors. Connect the GPIO pin of your board to the 1:2 voltage divider that is thus
 *    formed by the two resistors. Results may vary.
 *    --> Note: I used this schematic during many long debugging hours, but I cannot guarantee that it won't ultimately
 *        cause your car to explode! (or anything less catastrophic)
 *
 * -----
 * Output
 *
 * Raw packets will be printed line by line on the serial port, e.g. like this:
 *
 * Starting VAN bus receiver
 * Raw: #0000 ( 0/15)  0( 5) 0E 7CE RA1 21-14 NO_ACK OK 2114 CRC_OK
 * Raw: #0001 ( 1/15)  0( 5) 0E 4EC RA1 97-68 NO_ACK OK 9768 CRC_OK
 * Raw: #0002 ( 2/15) 11(16) 0E 4D4 RA0 82-0C-01-00-11-00-3F-3F-3F-3F-82:7B-A4 ACK OK 7BA4 CRC_OK
 * Raw: #0003 ( 3/15)  2( 7) 0E 5E4 WA0 00-FF:1F-F8 NO_ACK OK 1FF8 CRC_OK
 *
 * Legend:
 *
 * Raw: #0002 ( 2/15) 11(16) 0E 4D4 RA0 82-0C-01-00-11-00-3F-3F-3F-3F-82:7B-A4 ACK OK 7BA4 CRC_OK
 *         |    |  |   |  |   |  |   |   |                             |   |    |   |   |    |
 *         |    |  |   |  |   |  |   |   |                             |   |    |   |   |    +-- CRC value is correct
 *         |    |  |   |  |   |  |   |   |                             |   |    |   |   +-- Calculated CRC value
 *         |    |  |   |  |   |  |   |   |                             |   |    |   +-- Packet read result is OK
 *         |    |  |   |  |   |  |   |   |                             |   |    +-- Packet was ACKnowledged by receiver
 *         |    |  |   |  |   |  |   |   |                             |   +--------------- CRC value in packet
 *         |    |  |   |  |   |  |   |   +<-------- Packet data ------>+
 *         |    |  |   |  |   |  |   +--- "Command" FLAGS field; "R" = Read; "A" = requesting Ack; "0" = no RTR
 *         |    |  |   |  |   |  +--- IDEN field
 *         |    |  |   |  |   +--- SOF field (always 0x0E)
 *         |    |  |   |  +--- Total number of bytes in packet
 *         |    |  |   +--- Number of packet "data" bytes
 *         |    |  +--- Number of slots in circular RX queue
 *         |    +--- Occupied slot in circular RX queue
 *         +--- Packet sequence number (modulo 10000)
 */

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif // ARDUINO_ARCH_ESP32

#include <VanBusRx.h>  // https://github.com/0xCAFEDECAF/VanBus

// GPIO pin connected to VAN bus transceiver output
#ifdef ARDUINO_ARCH_ESP32
  const int RX_PIN = GPIO_NUM_22;
#else // ! ARDUINO_ARCH_ESP32

  #if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
    // For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
    #define D2 (2)
  #endif // defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01

  // For WEMOS D1 mini board we use D2 (GPIO 4)
  const int RX_PIN = D2;
#endif // ARDUINO_ARCH_ESP32

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.print("Starting VAN bus receiver\n");

    // Disable Wi-Fi altogether to get rid of long and variable interrupt latency, causing packet CRC errors
    // From: https://esp8266hints.wordpress.com/2017/06/29/save-power-by-reliably-switching-the-esp-wifi-on-and-off/
    WiFi.disconnect(true);
    delay(1);
    WiFi.mode(WIFI_OFF);
    delay(1);
  #ifdef ARDUINO_ARCH_ESP8266
    WiFi.forceSleepBegin();
    delay(1);
  #endif // ARDUINO_ARCH_ESP8266

    VanBusRx.Setup(RX_PIN);
    Serial.printf_P(PSTR("VanBusRx queue of size %d is set up\n"), VanBusRx.QueueSize());
} // setup

void loop()
{
    TVanPacketRxDesc pkt;
    if (VanBusRx.Receive(pkt))
    {
      #ifdef VAN_RX_ISR_DEBUGGING
        bool crcOk =
      #endif // VAN_RX_ISR_DEBUGGING
        pkt.CheckCrcAndRepair();

        // Show byte content of packet
        pkt.DumpRaw(Serial);

      #ifdef VAN_RX_ISR_DEBUGGING
        // Fully dump bit timings for packets that have CRC ERROR, for further analysis
        if (! crcOk) pkt.getIsrDebugPacket().Dump(Serial);
      #endif // VAN_RX_ISR_DEBUGGING
    } // if

    // Print some boring statistics
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 10000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBusRx.DumpStats(Serial);
    } // if
} // loop
