/*
 * VanBus: DisplayNotifications - send all multifunction display (MFD) notifications (information, warning), one by one
 *
 * Written by Erik Tromp
 *
 * Version 0.2.4 - November, 2021
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

// Send exterior temperature 8 deg C to the multifunction display (MFD)
void SendExteriorTemperatureMessage()
{
    uint8_t rmtTemperatureBytes[] = {0x0F, 0x07, 0x00, 0x00, 0x00, 0x00, 0x60};
    VanBus.SyncSendPacket(0x8A4, 0x08, rmtTemperatureBytes, sizeof(rmtTemperatureBytes));
} // SendExteriorTemperatureMessage

// Trigger a notification message on the MFD
void SendMfdNotificationMessage(uint8_t msgIdx)
{
    uint8_t rmtMessageBytes[] =
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, msgIdx, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    VanBus.SyncSendPacket(0x524, 0x08, rmtMessageBytes, sizeof(rmtMessageBytes));
} // SendMfdNotificationMessage

#define MSG_ID_START (0x00)
#define MSG_ID_END (0x7F)
#define MSG_ID_INVALID (0x80)

// Return the next notification message ID
uint8_t NextMfdNotificationMessageIndex(uint8_t msgIdx)
{
    ++msgIdx;
    if (msgIdx == 0x48) msgIdx = 0x50;  // Skip 0x48 ... 0x4F; those bit indexes never occur
    if (msgIdx > MSG_ID_END) return MSG_ID_START;
    return msgIdx;
} // NextMfdNotificationMessageIndex

// Return the previous notification message ID
uint8_t PreviousMfdNotificationMessageIndex(uint8_t msgIdx)
{
    if (msgIdx == MSG_ID_START) return MSG_ID_END;
    --msgIdx;
    if (msgIdx == 0x4F) msgIdx = 0x47;  // Skip 0x48 ... 0x4F; those bit indexes never occur
    return msgIdx;
} // PreviousMfdNotificationMessageIndex

void CycleMessage(bool restart = false)
{
    static unsigned long lastSentAt = 0;

    // Write packet every second
    if (millis() - lastSentAt >= 1000UL) // Arithmetic has safe roll-over
    {
        lastSentAt = millis();

        // One by one, trigger each message on the MFD

        static uint8_t msgIdx = MSG_ID_START;
        if (restart) msgIdx = MSG_ID_START;

        Serial.printf("Sending MFD notification ID: 0x%02X\n", msgIdx);
        SendMfdNotificationMessage(msgIdx);

        msgIdx = NextMfdNotificationMessageIndex(msgIdx);
    } // if
} // CycleMessage

void RepeatMessage(uint8_t msgIdx)
{
    static unsigned long lastSentAt = -7000;

    // To display the notification permanently, it must be re-triggered every 7 seconds
    if (millis() - lastSentAt >= 7000UL)  // Arithmetic has safe roll-over
    {
        lastSentAt = millis();

        // Re-trigger the display of the current notification message
        SendMfdNotificationMessage(MSG_ID_INVALID);
        SendMfdNotificationMessage(msgIdx);
    } // if
} // if

char RecvOneChar()
{
    if (Serial.available() > 0) return Serial.read();
    return 0;
} // RecvOneChar

void printUsage()
{
    Serial.println("Type one of the following letters, then hit <Enter> (or click the 'Send' button):");
    Serial.println("-> n = send Next message");
    Serial.println("-> p = send Previous message");
    Serial.println("-> c = Cycle through all messages");
    Serial.println("-> s = Stop cycling through all messages");
    Serial.println("-> h = show this Help text");
} // printUsage

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.println("Sketch to demonstrate the sending of MFD notification messages");
    Serial.println();

    // Disable Wi-Fi altogether to get rid of long and variable interrupt latency, causing packet CRC errors
    // From: https://esp8266hints.wordpress.com/2017/06/29/save-power-by-reliably-switching-the-esp-wifi-on-and-off/
    WiFi.disconnect(true);
    delay(1); 
    WiFi.mode(WIFI_OFF);
    delay(1);
    WiFi.forceSleepBegin();
    delay(1);

    VanBus.Setup(RX_PIN, TX_PIN);

    printUsage();
} // setup

void loop()
{
    static bool cycling = false;
    static uint8_t msgIdx = MSG_ID_END;

    char c = RecvOneChar();

    switch (c)
    {
        case 'n':
        {
            msgIdx = NextMfdNotificationMessageIndex(msgIdx);
            Serial.printf("Sending MFD notification ID: 0x%02X\n", msgIdx);
            SendMfdNotificationMessage(msgIdx);
        }
        break;

        case 'p':
        {
            msgIdx = PreviousMfdNotificationMessageIndex(msgIdx);
            Serial.printf("Sending MFD notification ID: 0x%02X\n", msgIdx);
            SendMfdNotificationMessage(msgIdx);
        }
        break;

        case 'c':
        {
            Serial.println("Starting to send all MFD notifications, one by one");
            cycling = true;
            CycleMessage(true);
        }
        break;

        case 's':
        {
            cycling = false;
            Serial.println("Stopped sending all MFD notifications");
        }
        break;

        case 'h':
        {
            printUsage();
        }
        break;
    } // switch

    if (cycling) CycleMessage(); else RepeatMessage(msgIdx);

    // Write exterior temperature packet every second (causes the MFD to light its backlight)
    static unsigned long lastSentAt = 0;
    if (millis() - lastSentAt >= 1000UL)  // Arithmetic has safe roll-over
    {
        lastSentAt = millis();

        SendExteriorTemperatureMessage();  // Send exterior temperature 8 deg C to the MFD
    } // if

    // Print some boring statistics every minute or so
    static unsigned long lastDumped = 0;
    if (millis() - lastDumped >= 60000UL)  // Arithmetic has safe roll-over
    {
        lastDumped = millis();
        VanBus.DumpStats(Serial);
    } // if
} // loop
