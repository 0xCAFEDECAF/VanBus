/*
 * VanBus: SendPacket - send a packet on the VAN bus
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
 *    As the MCP2551 has 5V logic, a 5V <-> 3.3V level converter is needed to. For this example, connect:
 *    - the CRX / RXD / R pin of the transceiver, via the level converter, to pin D2 (GPIO 4) of your board, and
 *    - the CTX / TXD / D pin of the transceiver, via the level converter, to pin D3 (GPIO 0) of your board.
 *
 * 2. Use a SN65HVD230 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R
 *    and the CTX / TXD / D pins of the transceiver to a GPIO pin of your board. For this example, connect:
 *    - the CRX / RXD / R pin of the transceiver to pin D2 (GPIO 4) of your board, and
 *    - the CTX / TXD / D pin of the transceiver to pin D3 (GPIO 0) of your board.
 *
 * Note: for transmitting packets, it is necessary to also connect the receiving pin. The receiving pin is used to
 * sense media access from other devices on the bus, so that bus arbitration can be performed.
 */

#include <assert.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif // ARDUINO_ARCH_ESP32

#include <VanBus.h>  // https://github.com/0xCAFEDECAF/VanBus

// Define GPIO pin connected to VAN bus transceiver input and output.
// Use #defines, not const int, so that the Serial.printf_P in setup() shows the correct pin name on the console.
#ifdef ARDUINO_ARCH_ESP32
 #ifdef CONFIG_IDF_TARGET_ESP32S2
  #define TX_PIN GPIO_NUM_18
  #define RX_PIN GPIO_NUM_33
 #else
  #define TX_PIN GPIO_NUM_21
  #define RX_PIN GPIO_NUM_22
 #endif
#else // ! ARDUINO_ARCH_ESP32
  #define TX_PIN D3
  #define RX_PIN D2
#endif // ARDUINO_ARCH_ESP32

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.print("Starting VAN bus transmitter\n");

    // Disable WIFI altogether to get rid of long and variable interrupt latency, causing packet CRC errors
    // From: https://esp8266hints.wordpress.com/2017/06/29/save-power-by-reliably-switching-the-esp-wifi-on-and-off/
    WiFi.disconnect(true);
    delay(1); 
    WiFi.mode(WIFI_OFF);
    delay(1);
  #ifdef  ARDUINO_ARCH_ESP8266 
    WiFi.forceSleepBegin();
    delay(1);
  #endif // ARDUINO_ARCH_ESP8266

  #define XSTR(x) STR(x)
  #define STR(x) #x
    Serial.printf_P(
        PSTR("Setting up VAN bus receiver, RX pin %s (GPIO%u), TX pin %s (GPIO%u)\n"),
        XSTR(RX_PIN), RX_PIN,
        XSTR(TX_PIN), TX_PIN
    );
    VanBus.Setup(RX_PIN, TX_PIN);
    Serial.printf_P(PSTR("VanBus is set up, rx queue size is %d\n"), VanBus.QueueSize());
} // setup

void loop()
{
    TVanPacketRxDesc pkt;
    if (VanBus.Receive(pkt))
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

    static unsigned long lastSentAt = 0;

    // Write packet every 0.1 seconds
    if (millis() - lastSentAt >= 100UL) // Arithmetic has safe roll-over
    {
        lastSentAt = millis();

        // Alternately send exterior temperature 8 and 16 deg C to the multifunction display (MFD).
        // Note: the MFD will average out the received values, ending up showing 12 deg C.
        int temperatureValue = 8;
        if (temperatureValue == 8) temperatureValue = 16; else temperatureValue = 8;

        const int byteToSend = temperatureValue * 2 + 0x50;
        assert(byteToSend >= 0 && byteToSend <= 255);

        const uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, (uint8_t)byteToSend};
        VanBus.SyncSendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
    } // if

    // Print some boring statistics
    static unsigned long lastDumped = 0;
    if (millis() - lastDumped >= 10000UL) // Arithmetic has safe roll-over
    {
        lastDumped = millis();
        VanBus.DumpStats(Serial);
    } // if
} // loop
