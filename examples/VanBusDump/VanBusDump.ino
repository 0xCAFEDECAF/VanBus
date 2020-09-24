/*
 * VanBus: VanBusDump - dump all packets, received on a VAN bus, in raw format on the serial port.
 *
 * Written by Erik Tromp
 *
 * Version 0.1 - June, 2020
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
 *    As the MCP2551 has 5V logic, a 5V <-> 3.3V level converter is needed to connect the CRX / RXD / R pin of the
 *    transceiver to (in this example) GPIO pin 2 (RECV_PIN) of your ESP8266 board.
 *
 * 2. Use a SN65HVD230 transceiver, connected with its CANH and CANL pins to the vehicle's VAN bus.
 *    The SN65HVD230 transceiver already has 3.3V logic, so it is possible to directly connect the CRX / RXD / R pin of
 *    the transceiver to a GPIO pin of your ESP8266 board.
 *
 * 3. The simplest schematic is not to use a transceiver at all, but connect the VAN DATA line to GrouND using
 *    two 4.7 kOhm resistors. Connect the GPIO pin of your ESP8266 board to the 1:2 voltage divider that is thus
 *    formed by the two resistors. Results may vary.
 *    --> Note: I used this schematic during many long debugging hours, but I cannot guarantee that it won't ultimately
 *        cause your car to explode! (or anything less catastrofic)
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

#include <VanBus.h>

#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
// For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
#define D2 (2)
#endif
int RECV_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

void setup()
{
    Serial.begin(115200);
    Serial.println("Starting VAN bus receiver");
    VanBus.Setup(RECV_PIN);
} // setup

void loop()
{
    TVanPacketRxDesc pkt;
    if (VanBus.Receive(pkt))
    {
        // Fully dump bit timings for packets that have CRC ERROR, for further analysis
        if (! pkt.CheckCrcAndRepair()) pkt.getIsrDebugPacket().Dump(Serial);

        // Show byte content of packet
        pkt.DumpRaw(Serial);
    } // if

    // Print some boring statistics every minute or so
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 60000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBus.DumpStats(Serial);
    } // if
} // loop
