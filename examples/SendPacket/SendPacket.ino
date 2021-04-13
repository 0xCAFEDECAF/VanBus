/*
 * VanBus: SendPacket - send a packet on the VAN bus
 *
 * Written by Erik Tromp
 *
 * Version 0.2.1 - April, 2021
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * -----
 * Wiring
 *
 * You can usually find the VAN bus on pins 2 and 3 of ISO block "A" of your head unit (car radio). See
 * https://en.wikipedia.org/wiki/Connectors_for_car_audio and https://github.com/morcibacsi/esp32_rmt_van_rx .
 *
 * There are various possibilities to hook up a ESP8266 based board to your vehicle's VAN bus:
 *
 * 1. Use a MCP2551 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    As the MCP2551 has 5V logic, a 5V <-> 3.3V level converter is needed to. For this example, connect:
 *    - the CRX / RXD / R pin of the transceiver, via the level converter, to GPIO pin 2 (RX_PIN) of your ESP8266
 *      board, and
 *    - the CTX / TXD / D pin of the transceiver, via the level converter, to GPIO pin 3 (TX_PIN) of your ESP8266
 *      board.
 *
 * 2. Use a SN65HVD230 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R
 *    and the CTX / TXD / D pins of the transceiver to a GPIO pin of your ESP8266 board. For this example, connect:
 *    - the CRX / RXD / R pin of the transceiver to GPIO pin 2 (RX_PIN) of your ESP8266 board.
 *    - the CTX / TXD / D pin of the transceiver to GPIO pin 3 (TX_PIN) of your ESP8266 board, and
 *
 * Note: for transmitting packets, it is necessary to also connect the receiving pin. The receiving pin is used to
 *   sense media access from other devices on the bus, so that bus arbitration can be performed.
 */

#include <ESP8266WiFi.h>
#include <VanBus.h>

const int TX_PIN = D3; // Set to GPIO pin connected to VAN bus transceiver input
const int RX_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.println("Starting VAN bus transmitter");

    // Disable WIFI altogether to get rid of long and variable interrupt latency, causing packet CRC errors
    // From: https://esp8266hints.wordpress.com/2017/06/29/save-power-by-reliably-switching-the-esp-wifi-on-and-off/
    WiFi.disconnect(true);
    delay(1); 
    WiFi.mode(WIFI_OFF);
    delay(1);
    WiFi.forceSleepBegin();
    delay(1);

    VanBus.Setup(RX_PIN, TX_PIN);
} // setup

void loop()
{
    TVanPacketRxDesc pkt;
    if (VanBus.Receive(pkt))
    {
        bool crcOk = pkt.CheckCrcAndRepair();

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
        static uint8_t temperatureValue = 0x60;
        if (temperatureValue == 0x60) temperatureValue = 0x70; else temperatureValue = 0x60;
        
        uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, temperatureValue};
        VanBus.SyncSendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
    } // if

    // Print some boring statistics every minute or so
    static unsigned long lastDumped = 0;
    if (millis() - lastDumped >= 60000UL) // Arithmetic has safe roll-over
    {
        lastDumped = millis();
        VanBus.DumpStats(Serial);
    } // if
} // loop
