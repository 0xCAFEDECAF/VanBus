/*
 * VanBus: PacketParser - try to parse the packets, received on the VAN comfort bus, and print the result on the
 *   serial port.
 *
 * Written by Erik Tromp
 *
 * Version 0.0.2 - September, 2020
 *
 * MIT license, all text above must be included in any redistribution.   
 *
 * -----
 * Description
 *
 * Parse the packets as received on the VAN comfort bus. These packets contain information about:
 * - Instrument panel
 * - Power train messages, sounds and warning lamps
 * - Lighting-signalling messages, sounds and warning lamps
 * - Driver information messages, sounds and warning lamps
 * - Wash/wipe messages, sounds and warning lamps
 * - Doors and protections messages, sounds and warning lamps
 * - Instruments and controls messages, sounds and warning lamps
 * - Driving assistance messages, sounds and warning lamps
 * - Comfort and convenience messages, sounds and warning lamps
 * - Accessory messages, sounds and indicator lamps
 * - Display systems  
 * - Audio / telematics / telephone
 * - Audio / multimedia
 * - Navigation
 * - Heating / air conditioning
 * - Additional heating
 * - Heating equipment
 * - and probably more
 *
 * Notes:
 * - All parsing done here is highly experimental, gathered together from various sources and from doing experiments
 *   on my own vehicle (Peugeot 406 Estate with DAM number 9586). Please be suspicious to any interpretation that is
 *   done inside this file. Your mileage may vary.
 *
 * -----
 * Wiring
 *
 * See paragraph 'Wiring' in the other example sketch, 'VanBusDump.ino'.
 *
 */

#include <VanBus.h>

#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
// For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
#define D2 (2)
#endif
int RECV_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

// Packet parsing
#define VAN_PACKET_DUPLICATE (1)
#define VAN_PACKET_PARSE_OK (0)
#define VAN_PACKET_PARSE_CRC_ERROR (-1)
#define VAN_PACKET_PARSE_UNEXPECTED_LENGTH (-2)
#define VAN_PACKET_PARSE_UNRECOGNIZED_IDEN (-3)
#define VAN_PACKET_PARSE_TO_BE_DECODED_IDEN (-4)

// VAN IDENtifiers
#define VIN_IDEN 0xE24
#define ENGINE_IDEN 0x8A4
#define HEAD_UNIT_STALK_IDEN 0x9C4
#define LIGHTS_STATUS_IDEN 0x4FC
#define DEVICE_REPORT 0x8C4
#define CAR_STATUS1_IDEN 0x564
#define CAR_STATUS2_IDEN 0x524
#define DASHBOARD_IDEN 0x824
#define DASHBOARD_BUTTONS_IDEN 0x664
#define HEAD_UNIT_IDEN 0x554
#define TIME_IDEN 0x984
#define AUDIO_SETTINGS_IDEN 0x4D4
#define MFD_STATUS_IDEN 0x5E4
#define AIRCON_IDEN 0x464
#define AIRCON2_IDEN 0x4DC
#define CDCHANGER_IDEN 0x4EC
#define SATNAV_STATUS_1_IDEN 0x54E
#define SATNAV_STATUS_2_IDEN 0x7CE
#define SATNAV_STATUS_3_IDEN 0x8CE
#define SATNAV_GUIDANCE_DATA_IDEN 0x9CE
#define SATNAV_GUIDANCE_IDEN 0x64E
#define SATNAV_REPORT_IDEN 0x6CE
#define MFD_TO_SATNAV_IDEN 0x94E
#define SATNAV_TO_MFD_IDEN 0x74E
#define SATNAV_DOWNLOADING_IDEN 0x6F4
#define SATNAV_DOWNLOADED1_IDEN 0xA44
#define SATNAV_DOWNLOADED2_IDEN 0xAC4
#define WHEEL_SPEED_IDEN 0x744
#define ODOMETER_IDEN 0x8FC
#define COM2000_IDEN 0x450
#define CDCHANGER_COMMAND_IDEN 0x8EC
#define MFD_TO_HEAD_UNIT_IDEN 0x8D4
#define AIR_CONDITIONER_DIAG_IDEN 0xADC
#define AIR_CONDITIONER_DIAG_COMMAND_IDEN 0xA5C
#define ECU_IDEN 0xB0E

#define SERIAL Serial

inline uint8_t GetBcd(uint8_t bcd)
{
    return (bcd >> 4 & 0x0F) * 10 + (bcd & 0x0F);
} // GetBcd

// Tuner band
enum TunerBand_t
{
    TB_NONE = 0,
    TB_FM1,
    TB_FM2,
    TB_FM3,  // Never seen, just guessing
    TB_FMAST,  // Auto-station
    TB_AM,
    TB_PTY_SELECT = 7
};

const char* TunerBandStr(uint8_t data)
{
    return
        data == TB_NONE ? "NONE" :
        data == TB_FM1 ? "FM1" :
        data == TB_FM2 ? "FM2" :
        data == TB_FM3 ? "FM3" :  // Never seen, just guessing
        data == TB_FMAST ? "FMAST" :
        data == TB_AM ? "AM" :
        data == TB_PTY_SELECT ? "PTY_SELECT" :  // When selecting PTY to scan for
        "??";
} // TunerBandStr

// Tuner scan mode
// Bits:
//  2  1  0
// ---+--+---
//  0  0  0 : Not searching
//  0  0  1 : Manual tuning
//  0  1  0 : Scanning by frequency
//  0  1  1 : 
//  1  0  0 : Scanning for station with matching PTY
//  1  0  1 : 
//  1  1  0 : 
//  1  1  1 : Auto-station search in the FMAST band (long-press "Radio Band" button)
enum TunerScanMode_t
{
    TS_NOT_SEARCHING = 0,
    TS_MANUAL = 1,
    TS_BY_FREQUENCY = 2,
    TS_BY_MATCHING_PTY = 4,
    TS_FM_AST = 7
};

const char* TunerScanModeStr(uint8_t data)
{
    return
        data == TS_NOT_SEARCHING ? "NOT_SEARCHING" :
        data == TS_MANUAL ? "MANUAL_TUNING" :
        data == TS_BY_FREQUENCY ? "SCANNING_BY_FREQUENCY" :
        data == TS_BY_MATCHING_PTY ? "SCANNING_MATCHING_PTY" : // Scanning for station with matching PTY
        data == TS_FM_AST ? "FM_AST_SEARCH" : // Auto-station search in the FMAST band (long-press Radio Band button)
        "??";
} // TunerScanModeStr

const char* PtyStr(uint8_t ptyCode)
{
    // See also:
    // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
    return
        ptyCode == 0 ? "Not defined" :
        ptyCode == 1 ? "News" :
        ptyCode == 2 ? "Current affairs" :
        ptyCode == 3 ? "Information" :
        ptyCode == 4 ? "Sport" :
        ptyCode == 5 ? "Education" :
        ptyCode == 6 ? "Drama" :
        ptyCode == 7 ? "Culture" :
        ptyCode == 8 ? "Science" :
        ptyCode == 9 ? "Varied" :
        ptyCode == 10 ? "Pop Music" :
        ptyCode == 11 ? "Rock Music" :
        ptyCode == 12 ? "Easy Listening" :  // also: "Middle of the road music"
        ptyCode == 13 ? "Light Classical" :
        ptyCode == 14 ? "Serious Classical" :
        ptyCode == 15 ? "Other Music" :
        ptyCode == 16 ? "Weather" :
        ptyCode == 17 ? "Finance" :
        ptyCode == 18 ? "Children's Programmes" :
        ptyCode == 19 ? "Social Affairs" :
        ptyCode == 20 ? "Religion" :
        ptyCode == 21 ? "Phone-in" :
        ptyCode == 22 ? "Travel" :
        ptyCode == 23 ? "Leisure" :
        ptyCode == 24 ? "Jazz Music" :
        ptyCode == 25 ? "Country Music" :
        ptyCode == 26 ? "National Music" :
        ptyCode == 27 ? "Oldies Music" :
        ptyCode == 28 ? "Folk Music" :
        ptyCode == 29 ? "Documentary" :
        ptyCode == 30 ? "Alarm Test" :
        ptyCode == 31 ? "Alarm" :
        "??";
} // PtyStr

// Seems to be used in bus packets with IDEN:
//
// - SATNAV_REPORT_IDEN (0x6CE): data[1]. Following values of data[1] seen:
//   0x02, 0x05, 0x08, 0x09, 0x0E, 0x0F, 0x10, 0x11, 0x13, 0x1B, 0x1D
// - SATNAV_TO_MFD_IDEN (0x74E): data[1]. Following values of data[1] seen:
//   0x02, 0x05, 0x08, 0x09, 0x1B, 0x1C
// - MFD_TO_SATNAV_IDEN (0x94E): data[0]. Following values of data[1] seen:
//   0x02, 0x05, 0x06, 0x08, 0x09, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x1B, 0x1C, 0x1D
//
const char* SatNavRequestStr(uint8_t data)
{
    char buffer[5];
    sprintf_P(buffer, PSTR("0x%02X"), data);

    return
        data == 0x00 ? "ENTER_COUNTRY" :  // Never seen, just guessing
        data == 0x01 ? "ENTER_PROVINCE" :  // Never seen, just guessing
        data == 0x02 ? "ENTER_CITY" :
        data == 0x03 ? "ENTER_DISTRICT" :  // Never seen, just guessing
        data == 0x04 ? "ENTER_NEIGHBORHOOD" :  // Never seen, just guessing
        data == 0x05 ? "ENTER_STREET" :
        data == 0x06 ? "ENTER_HOUSE_NUMBER" :
        data == 0x07 ? "ENTER_HOUSE_NUMBER_LETTER" :  // Never seen, just guessing
        data == 0x08 ? "PLACE_OF_INTEREST_CATEGORY_LIST" :
        data == 0x09 ? "PLACE_OF_INTEREST_CATEGORY" :
        data == 0x0E ? "GPS_FOR_PLACE_OF_INTEREST" :
        data == 0x0F ? "NEXT_STREET" : // Shown during navigation in the (solid line) top box
        data == 0x10 ? "CURRENT_STREET" : // Shown during navigation in the (dashed line) bottom box
        data == 0x11 ? "PRIVATE_ADDRESS" :
        data == 0x12 ? "BUSINESS_ADDRESS" :
        data == 0x13 ? "SOFTWARE_MODULE_VERSIONS" :
        data == 0x1B ? "PRIVATE_ADDRESS_LIST" :
        data == 0x1C ? "BUSINESS_ADDRESS_LIST" :
        data == 0x1D ? "GPS_CHOOSE_DESTINATION" :
        buffer;
} // SatNavRequestStr

// Attempt to show a detailed SatNnav guidance instruction in "Ascii art"
//
// A detailed SatNnav guidance instruction consists of 8 bytes:
// * 0   : turn angle in increments of 22.5 degrees, measured clockwise, starting with 0 at 6 o-clock.
//         E.g.: 0x4 == 90 deg left, 0x8 = 180 deg = straight ahead, 0xC = 270 deg = 90 deg right.
//         Turn angle is shown here as (vertical) (d|sl)ash ('\', '|', '/', or '-').
// * 1   : always 0x00 ??
// * 2, 3: bit pattern indicating which legs are present in the junction or roundabout. Each bit set is for one leg.
//         Lowest bit of byte 3 corresponds to the leg of 0 degrees (straight down, which is
//         always there, because that is where we are currently driving), running clockwise up to the
//         highest bit of byte 2, which corresponds to a leg of 337.5 degrees (very sharp right).
//         A leg is shown here as a '.'.
// * 4, 5: bit pattern indicating which legs in the junction are "no entry". The coding of the bits is the same
//         as for bytes 2 and 3.
//         A "no-entry" is shown here as "(-)".
// * 6   : always 0x00 ??
// * 7   : always 0x00 ??
//
void PrintGuidanceInstruction(const uint8_t data[8])
{
    SERIAL.printf("      %s%s%s%s%s\n",
        data[5] & 0x40 ? "(-)" : "   ",
        data[5] & 0x80 ? "(-)" : "   ",
        data[4] & 0x01 ? "(-)" : "   ",
        data[4] & 0x02 ? "(-)" : "   ",
        data[4] & 0x04 ? "(-)" : "   "
    );
    SERIAL.printf("       %s  %s  %s  %s  %s\n",
        data[0] == 6 ? "\\" : data[3] & 0x40 ? "." : " ",
        data[0] == 7 ? "\\" : data[3] & 0x80 ? "." : " ",
        data[0] == 8 ? "|" : data[2] & 0x01 ? "." : " ",
        data[0] == 9 ? "/" : data[2] & 0x02 ? "." : " ",
        data[0] == 10 ? "/" : data[2] & 0x04 ? "." : " "
    );
    SERIAL.printf("    %s%s           %s%s\n",
        data[5] & 0x20 ? "(-)" : "   ",
        data[0] == 5 ? "-" : data[3] & 0x20 ? "." : " ",
        data[0] == 11 ? "-" : data[2] & 0x08 ? "." : " ",
        data[4] & 0x08 ? "(-)" : "   "
    );
    SERIAL.printf("    %s%s     +     %s%s\n",
        data[5] & 0x10 ? "(-)" : "   ",
        data[0] == 4 ? "-" : data[3] & 0x10 ? "." : " ",
        data[0] == 12 ? "-" : data[2] & 0x10 ? "." : " ",
        data[4] & 0x10 ? "(-)" : "   "
    );
    SERIAL.printf("    %s%s     |     %s%s\n",
        data[5] & 0x08 ? "(-)" : "   ",
        data[0] == 3 ? "-" : data[3] & 0x08 ? "." : " ",
        data[0] == 13 ? "-" : data[2] & 0x20 ? "." : " ",
        data[4] & 0x20 ? "(-)" : "   "
    );
    SERIAL.printf("       %s  %s  |  %s  %s\n",
        data[0] == 2 ? "/" : data[3] & 0x04 ? "." : " ",
        data[0] == 1 ? "/" : data[3] & 0x02 ? "." : " ",
        data[0] == 14 ? "\\" : data[3] & 0x40 ? "." : " ",
        data[0] == 15 ? "\\" : data[3] & 0x80 ? "." : " "
    );
    SERIAL.printf("      %s%s%s%s%s\n",
        data[5] & 0x04 ? "(-)" : "   ",
        data[5] & 0x02 ? "(-)" : "   ",
        data[5] & 0x01 ? "(-)" : "   ",
        data[4] & 0x40 ? "(-)" : "   ",
        data[4] & 0x80 ? "(-)" : "   "
    );
} // PrintGuidanceInstruction

// Parse a VAN packet
// Result:
// VAN_PACKET_DUPLICATE (1): Packet was the same as the last with this IDEN field
// VAN_PACKET_PARSE_OK (0): Packet was parsed OK
// VAN_PACKET_PARSE_CRC_ERROR (-1): Packet had a CRC error
// VAN_PACKET_PARSE_UNEXPECTED_LENGTH (-2): Packet had unexpected length
// VAN_PACKET_PARSE_UNRECOGNIZED_IDEN (-3): Packet had unrecognized IDEN field
// VAN_PACKET_PARSE_TO_BE_DECODED_IDEN (-4): IDEN recognized but the correct parsing of this packet is not yet known
int ParseVanPacket(TVanPacketRxDesc* pkt)
{
    if (! pkt->CheckCrc()) return VAN_PACKET_PARSE_CRC_ERROR;

    int dataLen = pkt->DataLen();
    if (dataLen < 0 || dataLen > 28) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt->Data();
    uint16_t iden = pkt->Iden();

    switch (iden)
    {
        case VIN_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#E24
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#E24

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> VIN: ");

            if (dataLen != 17)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char vinTxt[18];
            memcpy(vinTxt, data, 17);
            vinTxt[17] = 0;

            SERIAL.printf("%s\n", vinTxt);
        }
        break;

        case ENGINE_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8A4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8A4

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Engine: ");

            if (dataLen != 7)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // TODO - Always "CLOSE", even if open. Actual status of "door open" icon on instrument cluster is found in
            // packet with IDEN 0x4FC (LIGHTS_STATUS_IDEN)
            //data[1] & 0x08 ? "door=OPEN" : "door=CLOSE",

            char floatBuf[3][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "dash_light=%s, dash_actual_brightness=%u; contact_key_position=%s; engine=%s;\n"
                "    economy_mode=%s; in_reverse=%s; trailer=%s; water_temp=%s; odometer=%s; exterior_temperature=%s\n",
                data[0] & 0x80 ? "FULL" : "DIMMED (LIGHTS ON)",
                data[0] & 0x0F,

                (data[1] & 0x03) == 0x00 ? "OFF" :
                    (data[1] & 0x03) == 0x01 ? "ACC" :
                    (data[1] & 0x03) == 0x03 ? "ON" :
                    (data[1] & 0x03) == 0x02 ? "START_ENGINE" :
                    "??",

                data[1] & 0x04 ? "RUNNING" : "OFF",
                data[1] & 0x10 ? "ON" : "OFF",
                data[1] & 0x20 ? "YES" : "NO",
                data[1] & 0x40 ? "PRESENT" : "NOT_PRESENT",
                data[2] == 0xFF ? "---" : FloatToStr(floatBuf[0], data[2] - 39, 0),  // TODO - or: data[2] / 2
                FloatToStr(floatBuf[1], ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) / 10.0, 1),
                FloatToStr(floatBuf[2], (data[6] - 0x50) / 2.0, 1)
            );
        }
        break;

        case HEAD_UNIT_STALK_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#9C4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9C4

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Head unit stalk: ");

            if (dataLen != 2)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.printf(
                "button=%s,%s,%s,%s,%s; wheel=%d, wheel_rollover=%u\n",
                data[0] & 0x80 ? "NEXT" : "",
                data[0] & 0x40 ? "PREV" : "",
                data[0] & 0x08 ? "VOL_UP" : "",
                data[0] & 0x04 ? "VOL_DOWN" : "",
                data[0] & 0x02 ? "SOURCE" : "",
                data[1] - 0x80,
                data[0] >> 4 & 0x03
            );
        }
        break;

        case LIGHTS_STATUS_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#4FC
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_1
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanInstrumentClusterV1Structs.h

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            // Examples:
            // Raw: #1987 (12/15) 16 0E 4FC WA0 90-00-01-AE-F0-00-FF-FF-22-48-FF-7A-56 ACK OK 7A56 CRC_OK

            SERIAL.print("--> Lights status: ");

            if (dataLen != 11 && dataLen != 14)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Cars made until 2002?

            SERIAL.printf("\n    - Instrument cluster: %sENABLED\n", data[0] & 0x80 ? "" : "NOT ");
            SERIAL.printf("    - Speed regulator wheel: %s\n", data[0] & 0x40 ? "ON" : "OFF");
            SERIAL.printf("%s", data[0] & 0x20 ? "    - Warning LED ON\n" : "");
            SERIAL.printf("%s", data[0] & 0x04 ? "    - Diesel glow plugs ON\n" : "");  // TODO
            SERIAL.printf("%s", data[1] & 0x01 ? "    - Door OPEN\n" : "");
            SERIAL.printf(
                "    - Remaing km to service: %u (dashboard shows: %u)\n",
                ((uint16_t)data[2] << 8 | data[3]) * 20,
                (((uint16_t)data[2] << 8 | data[3]) * 20) / 100 * 100
            );

            if (data[5] & 0x02)
            {
                SERIAL.printf("    - Automatic gearbox: %s%s%s%s\n",
                    (data[4] & 0x70) == 0x00 ? "P" :
                        (data[4] & 0x70) == 0x10 ? "R" :
                        (data[4] & 0x70) == 0x20 ? "N" :
                        (data[4] & 0x70) == 0x30 ? "D" :
                        (data[4] & 0x70) == 0x40 ? "4" :
                        (data[4] & 0x70) == 0x50 ? "3" :
                        (data[4] & 0x70) == 0x60 ? "2" :
                        (data[4] & 0x70) == 0x70 ? "1" :
                        "??",
                    data[4] & 0x08 ? " - Snow" : "",
                    data[4] & 0x04 ? " - Sport" : "",
                    data[4] & 0x80 ? " (blinking)" : ""
                );
            } // if

            SERIAL.printf(
                "    - Lights: %s%s%s%s%s%s\n",
                data[5] & 0x80 ? "DIPPED_BEAM " : "",
                data[5] & 0x40 ? "HIGH_BEAM " : "",
                data[5] & 0x20 ? "FOG_FRONT " : "",
                data[5] & 0x10 ? "FOG_REAR " : "",
                data[5] & 0x08 ? "INDICATOR_RIGHT " : "",
                data[5] & 0x04 ? "INDICATOR_LEFT " : ""
            );

            if (data[6] != 0xFF)
            {
                // If you see "29.2 Â°C", then set 'Remote character set' to 'UTF-8' in
                // PuTTY setting 'Window' --> 'Translation'
                SERIAL.printf("    - Oil temperature: %d °C\n", (int)data[6] - 40);  // Never seen this
            } // if
            //SERIAL.printf("    - Oil temperature (2): %d °C\n", (int)data[9] - 50);  // Other possibility?

            if (data[7] != 0xFF)
            {
                SERIAL.printf("Fuel level: %u %%\n", data[7]);  // Never seen this
            } // if

            SERIAL.printf("    - Oil level: %s\n",
                data[8] <= 0x0B ? "------" :
                data[8] <= 0x19 ? "O-----" :
                data[8] <= 0x27 ? "OO----" :
                data[8] <= 0x35 ? "OOO---" :
                data[8] <= 0x43 ? "OOOO--" :
                data[8] <= 0x51 ? "OOOOO-" :
                "OOOOOO"
            );

            if (data[10] != 0xFF)
            {
                // Never seen this; I don't have LPG
                SERIAL.printf("LPG fuel level: %s\n",
                    data[10] <= 0x08 ? "1" :
                    data[10] <= 0x11 ? "2" :
                    data[10] <= 0x21 ? "3" :
                    data[10] <= 0x32 ? "4" :
                    data[10] <= 0x43 ? "5" :
                    data[10] <= 0x53 ? "6" :
                    "7"
                );
            } // if

            if (dataLen == 14)
            {
                // Cars made in/after 2004?

                // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_2

                SERIAL.printf("Cruise control: %s\n",
                    data[11] == 0x41 ? "OFF" :
                    data[11] == 0x49 ? "Cruise" :
                    data[11] == 0x59 ? "Cruise - speed" :
                    data[11] == 0x81 ? "Limiter" :
                    data[11] == 0x89 ? "Limiter - speed" :
                    "?"
                );

                SERIAL.printf("Cruise control speed: %u\n", data[12]);
            } // if
        }
        break;

        case DEVICE_REPORT:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8C4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8C4

            // Examples:


            if (dataLen < 1 || dataLen > 3)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.print("--> Device report: ");

            if (data[0] == 0x8A)
            {
                // I'm sure that these messages are sent by the head unit: when no other device than the head unit is
                // on the bus, these packets are seen (unACKed; ACKs appear when the MFD is plugged back in to the bus).

                // Examples:
                // Raw: #xxxx (xx/15)  8 0E 8C4 WA0 8A-24-40-9B-32 ACK OK 9B32 CRC_OK
                // Raw: #7820 (10/15)  8 0E 8C4 WA0 8A-21-40-3D-54 ACK OK 3D54 CRC_OK
                // Raw: #0345 ( 1/15)  8 0E 8C4 WA0 8A-28-40-F9-96 ACK OK F996 CRC_OK

                SERIAL.print("Head unit: ");

                if (dataLen != 3)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // data[1] values seen:
                // 0x20: Tuner info - reply to 8D4 WA0 D1 (any source)
                // 0x21: Audio settings announcement (source = satnav, tuner or CD changer)
                // 0x22: Button press announcement (source = satnav, tuner or CD changer)
                // 0x24: Tuner info announcement (any source)
                // 0x28: Cassette tape presence announcement (any source)
                // 0x30: Internal CD presence announcement (any source)
                // 0x40: Tuner presets available - reply to 8D4 WA0 27-11
                // 0x60: Cassette tape data available - reply to 8D4 WA0 D2
                // 0x61: Audio settings announcement (source = cassette tape)
                // 0x62: Button press announcement (source = cassette tape)
                // 0x64: Start tape announcement (source = cassette tape)
                // 0x68: Info announcement (source = cassette tape)
                // 0xC0: Internal CD track data available - reply to 8D4 WA0 D6
                // 0xC1: Audio settings announcement (source = internal CD)
                // 0xC2: Button press announcement (source = internal CD)
                // 0xC4: Searching announcement (source = internal CD)
                // 0xD0: Track info announcement (source = internal CD)
                //
                SERIAL.printf(
                    "%s\n",
                    data[1] == 0x20 ? "TUNER - REPLY" :
                    data[1] == 0x21 ? "AUDIO_SETTINGS_ANNOUNCE" :
                    data[1] == 0x22 ? "BUTTON_PRESS_ANNOUNCE" :
                    data[1] == 0x24 ? "TUNER_ANNOUNCEMENT" :
                    data[1] == 0x28 ? "TAPE_PRESENCE_ANNOUNCEMENT" :
                    data[1] == 0x30 ? "CD_PRESENT" :
                    data[1] == 0x40 ? "TUNER_PRESETS - REPLY" :
                    data[1] == 0x60 ? "TAPE_INFO - REPLY" :
                    data[1] == 0x61 ? "TAPE_PLAYING - AUDIO_SETTINGS_ANNOUNCE" :
                    data[1] == 0x62 ? "TAPE_PLAYING - BUTTON_PRESS_ANNOUNCE" :
                    data[1] == 0x64 ? "TAPE_PLAYING - STARTING" :
                    data[1] == 0x68 ? "TAPE_PLAYING - INFO" :
                    data[1] == 0xC0 ? "INTERNAL_CD_TRACK_INFO - REPLY" :
                    data[1] == 0xC1 ? "INTERNAL_CD_PLAYING - AUDIO_SETTINGS_ANNOUNCE" :
                    data[1] == 0xC2 ? "INTERNAL_CD_PLAYING - BUTTON_PRESS_ANNOUNCE" :
                    data[1] == 0xC4 ? "INTERNAL_CD_PLAYING - SEARCHING" :
                    data[1] == 0xD0 ? "INTERNAL_CD_PLAYING - TRACK_INFO" :
                    "??"
                );

                // Possible bits in data[1]:
                // 0x01 - Audio settings announcement
                // 0x02 - Button press announcement
                // 0x04 - Status update (CD track or tuner info)
                // 0x08 - 
                // 0xF0 - 0x20 = Tuner (radio)
                //      - 0x30 = CD track found
                //      - 0x40 = Tuner preset
                //      - 0x60 = Cassette tape playing
                //      - 0xC0 = CD playing
                //      - 0xD0 = CD track info
                //
                // SERIAL.printf(
                    // "%s - %s\n",
                    // data[1] == 0x24 ? "TUNER" : // TUNER - STATUS_UPDATE_ANNOUNCE
                        // data[1] == 0x20 ? "TUNER" :  // TUNER - REPLY
                        // (data[1] & 0x0F) == 0x01 ? "HEAD_UNIT" :  // HEAD_UNIT - AUDIO_SETTINGS_ANNOUNCE
                        // (data[1] & 0x0F) == 0x02 ? "HEAD_UNIT" :  // HEAD_UNIT - BUTTON_PRESS_ANNOUNCE
                        // (data[1] & 0xF0) == 0x30 ? "CD_PLAYING" :
                        // (data[1] & 0xF0) == 0x40 ? "TUNER_PRESET" :
                        // (data[1] & 0xF0) == 0xC0 ? "CD_OR_TAPE" :
                        // (data[1] & 0xF0) == 0xD0 ? "CD_TRACK" :
                        // "??",
                    // data[1] & 0x0F) == 0x00 ? "REPLY" :

                        // // When the following messages are ACK'ed, a report will follow, e.g.
                        // // 0x4D4 (AUDIO_SETTINGS_IDEN) or 0x554 (HEAD_UNIT_IDEN). When not ACK'ed, will retry a few
                        // // times.
                        // (data[1] & 0x0F) == 0x01 ? "AUDIO_SETTINGS_ANNOUNCE" :  // max 3 retries
                        // (data[1] & 0x0F) == 0x02 ? "BUTTON_PRESS_ANNOUNCE" :  // max 6 retries
                        // (data[1] & 0x0F) == 0x04 ? "STATUS_UPDATE_ANNOUNCE" :  // max 3 retries
                        // "??"
                // );

                // Button-press announcement?
                if ((data[1] & 0x0F) == 0x02)
                {
                    char buffer[5];
                    sprintf_P(buffer, PSTR("0x%02X"), data[2]);

                    SERIAL.printf(
                        "    Head unit button pressed: %s%s%s\n",
                        (data[2] & 0x1F) == 0x01 ? "1" :
                            (data[2] & 0x1F) == 0x02 ? "'2'" :
                            (data[2] & 0x1F) == 0x03 ? "'3'" :
                            (data[2] & 0x1F) == 0x04 ? "'4'" :
                            (data[2] & 0x1F) == 0x05 ? "'5'" :
                            (data[2] & 0x1F) == 0x06 ? "'6'" :
                            (data[2] & 0x1F) == 0x11 ? "AUDIO_DOWN" :
                            (data[2] & 0x1F) == 0x12 ? "AUDIO_UP" :
                            (data[2] & 0x1F) == 0x13 ? "SEEK_BACKWARD" :
                            (data[2] & 0x1F) == 0x14 ? "SEEK_FORWARD" :
                            (data[2] & 0x1F) == 0x16 ? "AUDIO" :
                            (data[2] & 0x1F) == 0x17 ? "MAN" :
                            (data[2] & 0x1F) == 0x1B ? "TUNER" :
                            (data[2] & 0x1F) == 0x1C ? "TAPE" :
                            (data[2] & 0x1F) == 0x1D ? "INTERNAL_CD" :
                            (data[2] & 0x1F) == 0x1E ? "CD_CHANGER" :
                            buffer,
                        data[2] & 0x40 ? " (released)" : "",
                        data[2] & 0x80 ? " (repeat)" : ""
                    );
                } // if
            }
            else if (data[0] == 0x96)
            {
                // Examples:
                // Raw: #7819 ( 9/15)  6 0E 8C4 WA0 96-D8-48 ACK OK D848 CRC_OK

                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.print("CD-changer: STATUS_UPDATE_ANNOUNCE\n");
            }
            else if (data[0] == 0x07)
            {
                if (dataLen != 3)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // Unknown what this is. Surely not the head unit. Could be:
                // - MFD unit - this type of packet is showing often when the MFD is changing to another screen
                // - Aircon panel
                // - CD changer (seems unlikely)
                // - SatNav system - this type of packet is showing often when using the SatNav
                // - Instrument panel

                // Examples:
                // Raw: #0408 ( 3/15)  8 0E 8C4 WA0 07-40-00-E6-2C ACK OK E62C CRC_OK
                // Raw: #1857 (12/15)  8 0E 8C4 WA0 07-41-00-94-C0 ACK OK 94C0 CRC_OK
                // Raw: #2152 ( 7/15)  8 0E 8C4 WA0 07-10-00-47-E8 ACK OK 47E8 CRC_OK
                // Raw: #2155 (10/15)  8 0E 8C4 WA0 07-01-00-46-62 ACK OK 4662 CRC_OK
                // Raw: #3026 (11/15)  8 0E 8C4 WA0 07-20-00-D2-42 ACK OK D242 CRC_OK
                // Raw: #4199 (14/15)  8 0E 8C4 WA0 07-21-01-BF-94 ACK OK BF94 CRC_OK
                // Raw: #5039 (14/15)  8 0E 8C4 WA0 07-01-01-59-58 ACK OK 5958 CRC_OK
                // Raw: #5919 ( 9/15)  8 0E 8C4 WA0 07-00-01-2B-B4 ACK OK 2BB4 CRC_OK
                // Raw: #7644 ( 9/15)  8 0E 8C4 WA0 07-47-00-A5-92 ACK OK A592 CRC_OK
                // Raw: #7677 (12/15)  8 0E 8C4 WA0 07-60-00-00-E0 ACK OK 00E0 CRC_OK
                // Raw: #9471 ( 6/15)  8 0E 8C4 WA0 07-00-02-0A-FA ACK OK 0AFA CRC_OK
                // Raw: #7386 ( 1/15)  8 0E 8C4 WA0 07-40-02-D8-58 ACK OK D858 CRC_OK

                // data[1] seems a bit pattern. Bits seen:
                //  & 0x01 - MFD shows SatNav disclaimer screen, Entering new destination in SatNav
                //  & 0x02
                //  & 0x04
                //  & 0x10 - Entering new destination in SatNav
                //  & 0x20 - Entering new destination in SatNav, SatNav showing direction
                //  & 0x40 - Contact key in "ACC" position?, SatNav showing direction
                //
                // data[2] is usually 0x00, sometimes 0x01 or 0x02

                SERIAL.printf(
                    "0x%02X 0x%02X 0x%02X [to be decoded]\n",
                    data[0],
                    data[1],
                    data[2]
                );

                return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
            }
            else if (data[0] == 0x52)
            {
                if (dataLen != 2)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // Unknown what this is. Surely not the MFD and not the head unit.
                // Seems to be sent more often when engine is running. Could be:
                // - Aircon panel
                // - CD changer (seems unlikely)
                // - SatNav system
                // - Instrument panel

                // Examples:
                // Raw: #2641 ( 1/15)  7 0E 8C4 WA0 52-08-97-D0 ACK OK 97D0 CRC_OK
                // Raw: #7970 (10/15)  7 0E 8C4 WA0 52-20-A8-0E ACK OK A80E CRC_OK

                // data[1] is usually 0x08, sometimes 0x20
                //  & 0x08 - Contact key in "ON" position?
                //  & 0x20 - Economy mode ON?

                SERIAL.printf(
                    "0x%02X 0x%02X [to be decoded]\n",
                    data[0],
                    data[1]
                );

                return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
            }
            else
            {
                SERIAL.printf(
                    "0x%02X [to be decoded]\n",
                    data[0]);

                return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
            } // if
        }
        break;

        case CAR_STATUS1_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#564
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#564

            // Print only if not duplicate of previous packet; ignore different sequence numbers
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data + 1, packetData, dataLen - 2) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data + 1, dataLen - 2);

            SERIAL.print("--> Car status 1: ");

            if (dataLen != 27)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char floatBuf[3][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "seq=%u; doors=%s%s%s%s%s; right_stalk_button=%s; avg_speed_1=%u; avg_speed_2=%u; "
                "exp_moving_avg_speed=%u;\n"
                "    range_1=%u; avg_consumption_1=%s; range_2=%u; avg_consumption_2=%s; inst_consumption=%s; "
                "mileage=%u\n",
                data[0] & 0x07,
                data[7] & 0x80 ? "FRONT_RIGHT " : "",
                data[7] & 0x40 ? "FRONT_LEFT " : "",
                data[7] & 0x20 ? "REAR_RIGHT " : "",
                data[7] & 0x10 ? "REAR_LEFT " : "",
                data[7] & 0x08 ? "BOOT " : "",
                data[10] & 0x01 ? "PRESSED" : "RELEASED",
                data[11],
                data[12],

                // When engine running but stopped (actual vehicle speed is 0), this value counts down by 1 every
                // 10 - 20 seconds or so. When driving, this goes up and down slowly toward the current speed.
                // Looking at the time stamps when this value changes, it looks like this is an exponential moving
                // average (EMA) of the recent vehicle speed. When the actual speed is 0, the value is seen to decrease
                // about 12% per minute. If the actual vehicle speed is sampled every second, then, in the
                // following formula, K would be around 12% / 60 = 0.2% = 0.002 :
                //
                //   exp_moving_avg_speed := exp_moving_avg_speed * (1 − K) + actual_vehicle_speed * K
                //
                // Often used in EMA is the constant N, where K = 2 / (N + 1). That means N would be around 1000 (given
                // a sampling time of 1 second).
                //
                data[13],

                (uint16_t)data[14] << 8 | data[15],
                FloatToStr(floatBuf[0], ((uint16_t)data[16] << 8 | data[17]) / 10.0, 1),
                (uint16_t)data[18] << 8 | data[19],
                FloatToStr(floatBuf[1], ((uint16_t)data[20] << 8 | data[21]) / 10.0, 1),
                (uint16_t)data[22] << 8 | data[23] == 0xFFFF
                    ? "---"
                    : FloatToStr(floatBuf[2], ((uint16_t)data[22] << 8 | data[23]) / 10.0, 1),
                (uint16_t)data[24] << 8 | data[25]
            );
        }
        break;

        case CAR_STATUS2_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#524
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#524
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV1.h
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV2.h

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Car status 2: ");

            if (dataLen != 14 && dataLen != 16)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // PROGMEM array of PROGMEM strings
            // See also: https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html

            // TODO - translate into all languages

            // Byte 0
            static const char msg_0_0[] PROGMEM = "Tyre pressure low";
            static const char msg_0_1[] PROGMEM = "Door open";
            static const char msg_0_2[] PROGMEM = "Auto gearbox temperature too high";
            static const char msg_0_3[] PROGMEM = "Brake system fault"; // and exclamation mark on instrument cluster
            static const char msg_0_4[] PROGMEM = "Hydraulic suspension fault";
            static const char msg_0_5[] PROGMEM = "Suspension fault";
            static const char msg_0_6[] PROGMEM = "Engine oil temperature too high"; // and oil can on instrument cluster
            static const char msg_0_7[] PROGMEM = "Water temperature too high";

            // Byte 1
            static const char msg_1_0[] PROGMEM = "Unblock diesel filter"; // and mil icon on instrument cluster
            static const char msg_1_1[] PROGMEM = "Stop car icon";
            static const char msg_1_2[] PROGMEM = "Diesel additive too low";
            static const char msg_1_3[] PROGMEM = "Fuel cap open";
            static const char msg_1_4[] PROGMEM = "Tyres punctured";
            static const char msg_1_5[] PROGMEM = "Coolant level low"; // and icon on instrument cluster
            static const char msg_1_6[] PROGMEM = "Oil pressure too low";
            static const char msg_1_7[] PROGMEM = "Oil level too low";

            // Byte 2
            static const char msg_2_0[] PROGMEM = "Noooo.... the famous \"Antipollution fault\" :-(";
            static const char msg_2_1[] PROGMEM = "Brake pads worn";
            static const char msg_2_2[] PROGMEM = "Diagnosis ok";
            static const char msg_2_3[] PROGMEM = "Auto gearbox faulty";
            static const char msg_2_4[] PROGMEM = "ESP"; // and icon on instrument cluster
            static const char msg_2_5[] PROGMEM = "ABS";
            static const char msg_2_6[] PROGMEM = "Suspension or steering fault";
            static const char msg_2_7[] PROGMEM = "Braking system faulty";

            // Byte 3
            static const char msg_3_0[] PROGMEM = "Side airbag faulty";
            static const char msg_3_1[] PROGMEM = "Airbags faulty";
            static const char msg_3_2[] PROGMEM = "Cruise control faulty";
            static const char msg_3_3[] PROGMEM = "Engine temperature too high";
            static const char msg_3_4[] PROGMEM = "Fault: Load shedding in progress"; // RT3 mono
            static const char msg_3_5[] PROGMEM = "Ambient brightness sensor fault";
            static const char msg_3_6[] PROGMEM = "Rain sensor fault";
            static const char msg_3_7[] PROGMEM = "Water in diesel fuel filter"; // and icon on instrument cluster

            // Byte 4
            static const char msg_4_0[] PROGMEM = "Left rear sliding door faulty";
            static const char msg_4_1[] PROGMEM = "Headlight corrector fault";
            static const char msg_4_2[] PROGMEM = "Right rear sliding door faulty";
            static const char msg_4_3[] PROGMEM = "No broken lamp"; // RT3 mono
            static const char msg_4_4[] PROGMEM = "Battery low";
            static const char msg_4_5[] PROGMEM = "Battery charge fault"; // and battery icon on instrument cluster
            static const char msg_4_6[] PROGMEM = "Diesel particle filter faulty";
            static const char msg_4_7[] PROGMEM = "Catalytic converter fault"; // MIL icon flashing on instrument cluster

            // Byte 5
            static const char msg_5_0[] PROGMEM = "Handbrake on";
            static const char msg_5_1[] PROGMEM = "Seatbelt warning";
            static const char msg_5_2[] PROGMEM = "Passenger airbag deactivated"; // and icon on instrument cluster
            static const char msg_5_3[] PROGMEM = "Screen washer liquid level too low";
            static const char msg_5_4[] PROGMEM = "Current speed too high";
            static const char msg_5_5[] PROGMEM = "Ignition key left in";
            static const char msg_5_6[] PROGMEM = "Sidelights left on";
            static const char msg_5_7[] PROGMEM = "Hill holder active"; // On RT3 mono: Driver's seatbelt not fastened

            // Byte 6
            static const char msg_6_0[] PROGMEM = "Shock sensor faulty";
            static const char msg_6_1[] PROGMEM = "Seatbelt warning"; // not sure; there is no message with 0x31 so could not check
            static const char msg_6_2[] PROGMEM = "Check and re-init tyre pressure";
            static const char msg_6_3[] PROGMEM = "Remote control battery low";
            static const char msg_6_4[] PROGMEM = "Left stick button pressed";
            static const char msg_6_5[] PROGMEM = "Put automatic gearbox in P position";
            static const char msg_6_6[] PROGMEM = "Stop lights test: brake gently";
            static const char msg_6_7[] PROGMEM = "Fuel level low";

            // Byte 7
            static const char msg_7_0[] PROGMEM = "Automatic headlamp lighting deactivated";
            static const char msg_7_1[] PROGMEM = "Rear LH passenger seatbelt not fastened";
            static const char msg_7_2[] PROGMEM = "Rear RH passenger seatbelt not fastened";
            static const char msg_7_3[] PROGMEM = "Front passenger seatbelt not fastened";
            static const char msg_7_4[] PROGMEM = "Driving school pedals indication";
            static const char msg_7_5[] PROGMEM = "Tyre pressure monitor sensors X missing";
            static const char msg_7_6[] PROGMEM = "Tyre pressure monitor sensors Y missing";
            static const char msg_7_7[] PROGMEM = "Tyre pressure monitor sensors Z missing";

            // Byte 8
            static const char msg_8_0[] PROGMEM = "Doors locked";
            static const char msg_8_1[] PROGMEM = "ESP/ASR deactivated";
            static const char msg_8_2[] PROGMEM = "Child safety activated";
            static const char msg_8_3[] PROGMEM = "Deadlocking active";
            static const char msg_8_4[] PROGMEM = "Automatic lighting active";
            static const char msg_8_5[] PROGMEM = "Automatic wiping active";
            static const char msg_8_6[] PROGMEM = "Engine immobiliser fault";
            static const char msg_8_7[] PROGMEM = "Sport suspension mode active";

            static const char *const msgTable[] PROGMEM =
            {
                msg_0_0, msg_0_1, msg_0_2, msg_0_3, msg_0_4, msg_0_5, msg_0_6, msg_0_7,
                msg_1_0, msg_1_1, msg_1_2, msg_1_3, msg_1_4, msg_1_5, msg_1_6, msg_1_7,
                msg_2_0, msg_2_1, msg_2_2, msg_2_3, msg_2_4, msg_2_5, msg_2_6, msg_2_7,
                msg_3_0, msg_3_1, msg_3_2, msg_3_3, msg_3_4, msg_3_5, msg_3_6, msg_3_7,
                msg_4_0, msg_4_1, msg_4_2, msg_4_3, msg_4_4, msg_4_5, msg_4_6, msg_4_7,
                msg_5_0, msg_5_1, msg_5_2, msg_5_3, msg_5_4, msg_5_5, msg_5_6, msg_5_7,
                msg_6_0, msg_6_1, msg_6_2, msg_6_3, msg_6_4, msg_6_5, msg_6_6, msg_6_7,
                msg_7_0, msg_7_1, msg_7_2, msg_7_3, msg_7_4, msg_7_5, msg_7_6, msg_7_7,
                msg_8_0, msg_8_1, msg_8_2, msg_8_3, msg_8_4, msg_8_5, msg_8_6, msg_8_7
            };

            SERIAL.print("\n");
            for (int byte = 0; byte < 9; byte++)
            {
                for (int bit = 0; bit < 8; bit++)
                {
                    if (data[byte] >> bit & 0x01)
                    {
                        char buffer[80];  // Make sure this is large enough for the largest string it must hold
                        strcpy_P(buffer, (char *)pgm_read_dword(&(msgTable[byte * 8 + bit])));
                        SERIAL.print("    - ");
                        SERIAL.println(buffer);
                    } // if
                } // for
            } // for
        }
        break;

        case DASHBOARD_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#824
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#824

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Dashboard: ");

            if (dataLen != 7)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char floatBuf[2][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "rpm=%s /min; speed=%s km/h; seq=%lu\n",
                data[0] == 0xFF && data[1] == 0xFF ?
                    "---.-" :
                    FloatToStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 8.0, 1),
                data[2] == 0xFF && data[3] == 0xFF ?
                    "---.--" :
                    FloatToStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2),
                (uint32_t)data[4] << 16 | (uint32_t)data[5] << 8 | data[6]
            );
        }
        break;

        case DASHBOARD_BUTTONS_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#664
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#664

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Dashboard buttons: ");

            if (dataLen != 11 && dataLen != 12)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // data[6..10] - always 00-FF-00-FF-00

            char floatBuf[2][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "hazard_lights=%s; door=%s; dashboard_programmed_brightness=%u, esp=%s,\n"
                "    fuel_level_filtered=%s litre, fuel_level_raw=%s litre\n",
                data[0] & 0x02 ? "ON" : "OFF",
                data[2] & 0x40 ? "LOCKED" : "UNLOCKED",
                data[2] & 0x0F,
                data[3] & 0x02 ? "ON" : "OFF",

                // Surely fuel level. Test with tank full shows definitely level is in litres.
                FloatToStr(floatBuf[0], data[4] / 2.0, 1),
                FloatToStr(floatBuf[1], data[5] / 2.0, 1)
            );
        }
        break;

        case HEAD_UNIT_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#554

            // These packets are sent by the head unit

            uint8_t seq = data[0];
            uint8_t infoType = data[1];

            // Head Unit info types
            enum HeatUnitInfoType_t
            {
                INFO_TYPE_TUNER = 0xD1,
                INFO_TYPE_TAPE,
                INFO_TYPE_PRESET,
                INFO_TYPE_CDCHANGER = 0xD5, // TODO - Not sure
                INFO_TYPE_CD,
            };

            switch (infoType)
            {
                case INFO_TYPE_TUNER:
                {
                    // Message when the HeadUnit is in "tuner" (radio) mode

                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_1

                    // Examples:
                    // 0E554E 80D1019206030F60FFFFA10000000000000000000080 9368
                    // 0E554E 82D1011242040F60FFFFA10000000000000000000082 3680
                    // 0E554E 87D10110CA030F60FFFFA10000000000000000000080 62E6

                    // Print only if not duplicate of previous packet
                    static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
                    if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
                    memcpy(packetData, data, dataLen);

                    SERIAL.print("--> Tuner info: ");

                    // TODO - some web pages show 22 bytes data, some 23
                    if (dataLen != 22)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    // data[2]: radio band and preset position
                    uint8_t band = data[2] & 0x07;
                    uint8_t presetPos = data[2] >> 3 & 0x0F;

                    // data[3]: scanning bits
                    bool scanDx = data[3] & 0x02;  // Scan mode: distant (Dx) or local (Lo)
                    bool ptyStandbyMode = data[3] & 0x04;

                    uint8_t scanMode = data[3] >> 3 & 0x07;

                    bool manualScan = data[3] & 0x08;
                    bool scanningByFreq = data[3] & 0x10;
                    bool fmastSearch = data[3] & 0x20;  // Auto-station search (long-press "Radio Band" button)

                    bool scanDirectionUp = data[3] & 0x80;

                    // data[4] and data[5]: frequency tuned in to
                    uint16_t frequency = (uint16_t)data[5] << 8 | data[4];

                    // data[6] - Reception status? Like: receiving station? Stereo? RDS bits like MS, TP, TA, AF?
                    // & 0xF0:
                    //   - Usually 0x00 when tuned in to a "normal" station.
                    //   - One or more bits stay '1' when tuned in to a "crappy" station (e.g. pirate).
                    //   - During the process of tuning in to another station, switches to e.g. 0x20, 0x60, but
                    //     (usually) ends up 0x00.
                    //   Just guessing for the possible meaning of the bits:
                    //   - Mono (not stereo)
                    //   - Music/Speech (MS)
                    //   - No RDS available / Searching for RDS
                    //   - No TA available / Searching for TA
                    //   - No AF (Alternative Frequencies) available
                    //   - No PTY (Program TYpe) available
                    //   - No PI (Program Identification) available
                    //   - Number (0..15) indicating the quality of the RDS stream
                    //   & 0x10:
                    //   & 0x20:
                    //   & 0x40:
                    //   & 0x80:
                    //
                    // & 0x0F = signal strength: increases with antenna plugged in and decreases with antenna plugged
                    //          out. Updated when a station is being tuned in to, or when the MAN button is pressed.
                    uint8_t signalStrength = data[6] & 0x0F;
                    char signalStrengthBuffer[3];
                    sprintf_P(signalStrengthBuffer, PSTR("%u"), signalStrength);

                    // data[7]: TA, RDS and REG (regional) bits
                    bool rdsSelected = data[7] & 0x01;
                    bool taSelected = data[7] & 0x02;
                    bool regional = data[7] & 0x04;  // Long-press "RDS" button
                    bool rdsAvailable = data[7] & 0x20;
                    bool taAvailable = data[7] & 0x40;
                    bool taAnnounce = data[7] & 0x80;

                    // data[8] and data[9]: PI code
                    // See also:
                    // - https://en.wikipedia.org/wiki/Radio_Data_System#Program_Identification_Code_(PI_Code),
                    // - https://radio-tv-nederland.nl/rds/rds.html
                    // - https://people.uta.fi/~jk54415/dx/pi-codes.html
                    // - http://poupa.cz/rds/countrycodes.htm
                    // - https://www.pira.cz/rds/p232man.pdf
                    uint16_t piCode = (uint16_t)data[8] << 8 | data[9];
                    uint8_t countryCode = data[8] >> 4 & 0x0F;
                    uint8_t coverageCode = data[8] & 0x0F;

                    // data[10]: for PTY-based scan mode
                    // & 0x1F: PTY code to scan
                    // & 0x20: 0 = PTY of station matches selected; 1 = no match
                    // & 0x40: 1 = "Select PTY" dialog visible (long-press "TA" button; press "<<" or ">>" to change)
                    uint8_t selectedPty = data[10] & 0x1F;
                    bool ptyMatch = (data[10] & 0x20) == 0;  // PTY of station matches selected PTY
                    bool ptySelectionMenu = data[10] & 0x40; 

                    // data[11]: PTY code of current station
                    uint8_t currPty = data[11] & 0x1F;

                    // data[12]...data[20]: RDS text
                    char rdsTxt[9];
                    strncpy(rdsTxt, (const char*) data + 12, 8);
                    rdsTxt[8] = 0;

                    char piBuffer[40];
                    sprintf_P(
                        piBuffer,
                        PSTR("%04X(%s, %s)"),
                        piCode,

                        // https://radio-tv-nederland.nl/rds/PI%20codes%20Europe.jpg
                        // More than one country is assigned to the same code, just listing the most likely.
                        countryCode == 0x01 || countryCode == 0x0D ? "Germany" :
                            countryCode == 0x02 ? "Ireland" :
                            countryCode == 0x03 ? "Poland" :
                            countryCode == 0x04 ? "Switzerland" :
                            countryCode == 0x05 ? "Italy" :
                            countryCode == 0x06 ? "Belgium" :
                            countryCode == 0x07 ? "Luxemburg" :
                            countryCode == 0x08 ? "Netherlands" :
                            countryCode == 0x09 ? "Denmark" :
                            countryCode == 0x0A ? "Austria" :
                            countryCode == 0x0B ? "Hungary" :
                            countryCode == 0x0C ? "UK" :
                            countryCode == 0x0E ? "Spain" :
                            countryCode == 0x0F ? "France" :
                            "???",
                        coverageCode == 0x00 ? "local" :
                            coverageCode == 0x01 ? "international" :
                            coverageCode == 0x02 ? "national" :
                            coverageCode == 0x03 ? "supra-regional" :
                            "regional"
                    );

                    char currPtyBuffer[40];
                    sprintf_P(currPtyBuffer, PSTR("%s(%u)"), PtyStr(currPty), currPty);

                    char selectedPtyBuffer[40];
                    sprintf_P(selectedPtyBuffer, PSTR("%s(%u)"), PtyStr(selectedPty), selectedPty);

                    char presetPosBuffer[20];
                    sprintf_P(presetPosBuffer, PSTR(", memory=%u"), presetPos);

                    bool anyScanBusy = (scanMode != TS_NOT_SEARCHING);

                    char floatBuf[MAX_FLOAT_SIZE];
                    SERIAL.printf(
                        "band=%s%s, %s %s, strength=%s,\n"
                        "    scan_mode=%s%s%s,\n",
                        TunerBandStr(band),
                        presetPos == 0 ? "" : presetPosBuffer,
                        frequency == 0x07FF ? "---" :
                            band == TB_AM
                                ? FloatToStr(floatBuf, frequency, 0)  // AM and LW bands
                                : FloatToStr(floatBuf, frequency / 20.0 + 50.0, 2),  // FM bands
                        band == TB_AM ? "KHz" : "MHz",

                        // TODO - check:
                        // - not sure if applicable in AM mode
                        // - signalStrength == 15 always means "not applicable" or "no signal"? Not just while scanning?
                        //   In other words: maybe 14 is the highest possible signal strength, and 15 just means: no
                        //   signal.
                        signalStrength == 15 && (scanMode == TS_BY_FREQUENCY || scanMode == TS_BY_MATCHING_PTY)
                            ? "--"
                            : signalStrengthBuffer,

                        TunerScanModeStr(scanMode),

                        // Scan sensitivity: distant (Dx) or local (Lo)
                        // TODO - not sure if this bit is applicable for the various values of 'scanMode'
                        ! anyScanBusy ? "" :
                            scanDx ? ", sensitivity=Dx" : ", sensitivity=Lo",

                        ! anyScanBusy ? "" :
                            scanDirectionUp ? ", scan_direction=UP" : ", scan_direction=DOWN"
                    );

                    if (band != TB_AM)
                    {
                        SERIAL.printf(
                            "    pty_selection_menu=%s, selected_pty=%s, pty_standby_mode=%s, pty_match=%s, pty=%s,\n"
                            "    pi=%s, regional=%s, ta=%s %s, rds=%s %s, rds_text=\"%s\"%s\n",

                            ptySelectionMenu ? "ON" : "OFF",
                            selectedPtyBuffer,
                            ptyStandbyMode ? "YES" : "NO",
                            ptyMatch ? "YES" : "NO",
                            currPty == 0x00 ? "---" : currPtyBuffer,

                            piCode == 0xFFFF ? "---" : piBuffer,
                            regional ? "ON" : "OFF",
                            taSelected ? "ON" : "OFF",
                            taAvailable ? "(AVAILABLE)" : "(NOT_AVAILABLE)",
                            rdsSelected ? "ON" : "OFF",
                            rdsAvailable ? "(AVAILABLE)" : "(NOT_AVAILABLE)",
                            rdsTxt,

                            taAnnounce ? "\n    --> Info Trafic!" : ""
                        );
                    } // if
                }
                break;
                
                case INFO_TYPE_TAPE:
                {
                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_2

                    SERIAL.print("--> Cassette tape info: ");

                    if (dataLen != 5)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    uint8_t status = data[2] & 0x3C;
                    char buffer[5];
                    sprintf_P(buffer, PSTR("0x%02X"), status);

                    SERIAL.printf("%s, side=%s\n",
                        status == 0x00 ? "STOPPED" :
                            status == 0x04 ? "LOADING" :
                            status == 0x0C ? "PLAYING" :
                            status == 0x10 ? "FAST_FORWARD" :
                            status == 0x14 ? "NEXT_TRACK" :
                            status == 0x30 ? "REWIND" :
                            status == 0x34 ? "PREVIOUS_TRACK" :
                            buffer,
                        data[2] & 0x01 ? "2" : "1"
                    );
                }
                break;

                case INFO_TYPE_PRESET:
                {
                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_3

                    SERIAL.print("--> Tuner preset info: ");

                    if (dataLen != 12)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    char rdsOrFreqTxt[9];
                    strncpy(rdsOrFreqTxt, (const char*) data + 3, 8);
                    rdsOrFreqTxt[8] = 0;

                    SERIAL.printf("band=%s, memory=%u, %s=\"%s\"\n",
                        TunerBandStr(data[2] >> 4 & 0x07),
                        data[2] & 0x0F,
                        data[2] & 0x80 ? "RDS_TEXT" : "FREQUECY",
                        rdsOrFreqTxt
                    );
                }
                break;

                case INFO_TYPE_CD:
                {
                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_6

                    SERIAL.print("--> CD track info: ");

                    if (dataLen < 10)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    SERIAL.printf("%s",
                        data[3] == 0x11 ? "INSERTED" :
                            data[3] == 0x12 ? "PAUSE-SEARCHING" :
                            data[3] == 0x13 ? "PLAY-SEARCHING" :
                            data[3] == 0x02 ? "PAUSE" :
                            data[3] == 0x03 ? "PLAY" :
                            data[3] == 0x04 ? "FAST_FORWARD" :
                            data[3] == 0x05 ? "REWIND" :
                            "??"
                    );

                    SERIAL.printf(" - %um:%us in track %u",
                        GetBcd(data[5]),
                        GetBcd(data[6]),
                        GetBcd(data[7])
                    );

                    if (data[8] != 0xFF)
                    {
                        SERIAL.printf("/%u", GetBcd(data[8]));

                        if (dataLen >= 12 && data[9] != 0xFF)
                        {
                            SERIAL.printf(" (total: %um:%us)",
                                GetBcd(data[9]),
                                GetBcd(data[10])
                            );
                        } // if
                    } // if

                    SERIAL.println();
                }
                break;

                case INFO_TYPE_CDCHANGER:
                {
                    SERIAL.print("--> CD changer info: ");

                    SERIAL.println("[to be decoded]");

                    return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
                }
                break;
            }
        }
        break;

        case TIME_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#984
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#984

            // TODO - seems to have nothing to do with time. Mine is always the same:
            //   Raw: #2692 ( 7/15) 10 0E 984 W-0 00-00-00-06-08-D0-C8 NO_ACK OK D0C8 CRC_OK
            // regardless of the time.

            SERIAL.print("--> Time: ");

            if (dataLen != 5)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.printf("uptime battery=%u days, time=%uh%um\n",
                (uint16_t)data[0] << 8 | data[1],
                data[3],
                data[4]
            );

        }
        break;

        case AUDIO_SETTINGS_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#4D4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4D4
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanRadioInfoStructs.h

            // These packets are sent by the head unit

            // Examples:
            // 0E4D4E 800C010011003F3F3F3F80 0118
            // 0E4D4E 850D000010003F3F3F3F85 6678
            // 0E4D4E 840C010011823F3F3F3F84 AD50
            // 0E4D4E 863C010011013E3E404086 70C4
            // 0E 4D4 RA0 84-0C-01-02-11-11-3F-3F-3F-3F-84-1E-2E ACK OK 1E2E CRC_OK

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Audio settings: ");

            if (dataLen != 11)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // data[0] & 0x07: sequence number
            // data[4] & 0x10: TODO

            SERIAL.printf(
                "power=%s, tape=%s, cd=%s, source=%s, ext_mute=%s, mute=%s,\n"
                "    volume=%u%s, audio_menu=%s, bass=%d%s, treble=%d%s, loudness=%s, fader=%d%s, balance=%d%s, "
                "auto_volume=%s\n",
                data[2] & 0x01 ? "ON" : "OFF",  // power
                data[4] & 0x20 ? "PRESENT" : "NOT_PRESENT",  // tape
                data[4] & 0x40 ? "PRESENT" : "NOT_PRESENT",  // cd

                (data[4] & 0x0F) == 0x00 ? "NONE" :  // source
                    (data[4] & 0x0F) == 0x01 ? "TUNER" :
                    (data[4] & 0x0F) == 0x02 ?
                        data[4] & 0x20 ? "TAPE" : 
                        data[4] & 0x40 ? "INTERNAL_CD" : 
                        "INTERNAL_CD_OR_TAPE" :
                    (data[4] & 0x0F) == 0x03 ? "CD_CHANGER" :

                    // This is the "default" mode for the head unit, to sit there and listen to the navigation
                    // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                    // whenever this source is chosen.
                    (data[4] & 0x0F) == 0x05 ? "NAVIGATION_AUDIO" :

                    "???",

                data[1] & 0x02 ? "ON" : "OFF",  // ext_mute. Activated when head unit ISO connector A pin 1 ("Phone mute") is pulled down.
                data[1] & 0x01 ? "ON" : "OFF",  // mute. To activate: press both VOL_UP and VOL_DOWN buttons on stalk.

                data[5] & 0x7F,  // volume
                data[5] & 0x80 ? "<UPD>" : "",

                // audio_menu. Bug: if CD changer is playing, this one is always "OPEN" (even if it isn't).
                data[1] & 0x20 ? "OPEN" : "CLOSED",

                (sint8_t)(data[8] & 0x7F) - 0x3F,  // bass
                data[8] & 0x80 ? "<UPD>" : "",
                (sint8_t)(data[9] & 0x7F) - 0x3F,  // treble
                data[9] & 0x80 ? "<UPD>" : "",
                data[1] & 0x10 ? "ON" : "OFF",  // loudness
                (sint8_t)(0x3F) - (data[7] & 0x7F),  // fader
                data[7] & 0x80 ? "<UPD>" : "",
                (sint8_t)(0x3F) - (data[6] & 0x7F),  // balance
                data[6] & 0x80 ? "<UPD>" : "",
                data[1] & 0x04 ? "ON" : "OFF"  // auto_volume
            );
        }
        break;

        case MFD_STATUS_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#5E4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#5E4

            // Example: 0E5E4C00FF1FF8

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> MFD status: ");

            if (dataLen != 2)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            uint16_t mfdStatus = (uint16_t)data[0] << 8 | data[1];

            char buffer[7];
            sprintf_P(buffer, PSTR("0x%04X"), mfdStatus);

            // TODO - seems like data[0] & 0x20 is some kind of status bit, showing MFD connectivity status? There
            // must also be a specific packet that triggers this bit to be set to '0', because this happens e.g. when
            // the contact key is removed.

            SERIAL.printf(
                "%s\n",

                // hmmm... MFD can also be ON if this is reported; this happens e.g. in the "minimal VAN network" test
                // setup with only the head unit (radio) and MFD. Maybe this is a status report: the MFD shows if has
                // received any packets that show connectivity to e.g. the BSI?
                data[0] == 0x00 && data[1] == 0xFF ? "MFD_SCREEN_OFF" :

                    data[0] == 0x20 && data[1] == 0xFF ? "MFD_SCREEN_ON" :
                    buffer
            );
        }
        break;

        case AIRCON_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#464
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#464
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner1Structs.h

            // Examples:
            // Raw: #3147 ( 7/15) 10 0E 464 WA0 00-00-00-00-07-D1-7C ACK OK D17C CRC_OK
            // Raw: #3510 (10/15) 10 0E 464 WA0 10-00-00-00-07-AE-8A ACK OK AE8A CRC_OK
            // Raw: #3542 (12/15) 10 0E 464 WA0 10-00-00-00-09-15-C6 ACK OK 15C6 CRC_OK
            // Raw: #1978 ( 3/15) 10 0E 464 WA0 11-00-00-00-15-81-DC ACK OK 81DC CRC_OK
            // Raw: #2780 (10/15) 10 0E 464 WA0 10-00-00-00-09-15-C6 ACK OK 15C6 CRC_OK
            // Raw: #3994 ( 9/15) 10 0E 464 WA0 10-00-00-00-08-0A-FC ACK OK 0AFC CRC_OK
            // Raw: #5630 (10/15) 10 0E 464 WA0 10-00-00-00-06-B1-B0 ACK OK B1B0 CRC_OK
            // Raw: #7808 (13/15) 10 0E 464 WA0 00-00-00-00-00-8C-DA ACK OK 8CDA CRC_OK

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Aircon: ");

            if (dataLen != 5)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // data[0] - bits:
            // - & 0x01: Rear window heater ON
            // - & 0x04: Air recirculation ON
            // - & 0x10: A/C icon?
            // - & 0x20: A/C icon? "YES" : "NO",  // TODO - what is this? This bit is always 0
            // - & 0x40: mode ? "AUTO" : "MANUAL",  // TODO - what is this? This bit is always 0
            // - & 0x0A: demist ? "ON" : "OFF",  // TODO - what is this? These bits are always 0

            // data[4] - reported fan speed
            //
            // Real-world tests: reported fan_speed values with various settings of the fan speed icon (NO icon
            // visible ... all blades full, 0...7), under varying conditions:
            //
            // 1.) Recirculation = OFF, rear window heater = OFF, A/C = OFF:
            //     Fan icon not visible at all = 0
            //     Fan icon with all empty blades = 4 
            //     One blade visible = 4 (same as previous!)
            //     Two blades - low = 6
            //     Two blades - high = 7
            //     Three blades - low = 9
            //     Three blades - high = 10
            //     Four blades = 19
            //     
            // 2.) Recirculation = ON, rear window heater = OFF, A/C = OFF:
            //     Fan icon not visible at all = 0
            //     Fan icon with all empty blades = 4
            //     One blade visible = 4 (same as previous!)
            //     Two blades - low = 6
            //     Two blades - high = 7
            //     Three blades - low = 9
            //     Three blades - high = 10
            //     Four blades = 19
            //
            // 3.) Recirculation = OFF, rear window heater = OFF, A/C = ON (compressor running):
            //     Fan icon not visible at all = 0 (A/C icon will turn off)
            //     Fan icon with all empty blades = 4
            //     One blade visible = 4 (same as previous!)
            //     Two blades - low = 8
            //     Two blades - high = 9
            //     Three blades - low = 11
            //     Three blades - high = 12
            //     Four blades = 21
            //
            // 4.) Recirculation = OFF, rear window heater = ON, A/C = OFF:
            //     Fan icon not visible at all = 12
            //     Fan icon with all empty blades = 15
            //     One blade visible = 16
            //     Two blades - low = 17
            //     Two blades - high = 19
            //     Three blades - low = 20
            //     Three blades - high = 22
            //     Four blades = 30
            //
            // 5.) Recirculation = OFF, rear window heater = ON, = A/C ON:
            //     Fan icon not visible at all = 12 (A/C icon will turn off)
            //     Fan icon with all empty blades = 17
            //     One blade visible = 18
            //     Two blades - low = 19
            //     Two blades - high = 21
            //     Three blades - low = 22
            //     Three blades - high = 24
            //     Four blades = 32
            //
            // All above with demist ON --> makes no difference
            //
            // In "AUTO" mode, the fan speed varies gradually over time in increments or decrements of 1.

            bool rear_heat = data[0] & 0x01;
            bool ac_icon = data[0] & 0x10;
            uint8_t setFanSpeed = data[4];
            if (rear_heat) setFanSpeed -= 12;
            if (ac_icon) setFanSpeed -= 2;
            setFanSpeed =
                setFanSpeed >= 18 ? 7 : // Four blades
                setFanSpeed >= 10 ? 6 : // Three blades - high
                setFanSpeed >= 8 ? 5 : // Three blades - low
                setFanSpeed == 7 ? 4 : // Two blades - high
                setFanSpeed >= 5 ? 3 : // Two blades - low
                setFanSpeed == 4 ? 2 : // One blade (2) or all empty blades (1)
                setFanSpeed == 3 ? 1 : // All empty blades (1)
                0; // Fan icon not visible at all

            SERIAL.printf(
                "ac_icon=%s; recirc=%s, rear_heat=%s, reported_fan_speed=%u, set_fan_speed=%u\n",
                ac_icon ? "ON" : "OFF",
                data[0] & 0x04 ? "ON" : "OFF",
                rear_heat ? "YES" : "NO",
                data[4],
                setFanSpeed
            );
        }
        break;

        case AIRCON2_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#4DC
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4DC
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner2Structs.h

            // Evaporator temperature is constantly toggling between 2 values, while the rest of the data is the same.
            // So print only if not duplicate of previous 2 packets.
            static uint8_t packetData[2][MAX_DATA_BYTES];  // Previous packet data

            if (memcmp(data, packetData[0], dataLen) == 0) return VAN_PACKET_DUPLICATE;
            if (memcmp(data, packetData[1], dataLen) == 0) return VAN_PACKET_DUPLICATE;

            memcpy(packetData[0], packetData[1], dataLen);
            memcpy(packetData[1], data, dataLen);

            SERIAL.print("--> Aircon 2: ");

            if (dataLen != 7)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char floatBuf[2][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "contact_key_on=%s; enabled=%s; rear_window=%s; aircon_compressor=%s; contact_key_position=%s;\n"
                "    condenserTemperature=%s, evaporatorTemperature=%s\n",
                data[0] & 0x80 ? "YES" : "NO",
                data[0] & 0x40 ? "YES" : "NO",
                data[0] & 0x20 ? "ON" : "OFF",
                data[0] & 0x01 ? "ON" : "OFF",
                data[1] == 0x1C ? "ACC_OR_OFF" :
                    data[1] == 0x18 ? "ACC-->OFF" :
                    data[1] == 0x04 ? "ON-->ACC" :
                    data[1] == 0x00 ? "ON" :
                    "??",

                // This is not interior temperature. This rises quite rapidly if the aircon compressor is
                // running, and drops again when the aircon compressor is off. So I think this is the condenser
                // temperature.
                data[2] == 0xFF ? "--" : FloatToStr(floatBuf[0], data[2], 0),

                FloatToStr(floatBuf[1], ((uint16_t)data[3] << 8 | data[4]) / 10.0 - 40.0, 1)
            );
        }
        break;

        case CDCHANGER_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#4EC
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4EC
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanCdChangerStructs.h

            // Example: 0E4ECF9768

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> CD Changer: ");

            switch (dataLen)
            {
                case 0:
                {
                    SERIAL.print("request\n");

                    break;
                }
                case 12:
                {
                    char floatBuf[3][MAX_FLOAT_SIZE];

                    SERIAL.printf(
                        "random=%s; state=%s; cartridge=%s; %sm:%ss in track %u/%u on CD %u; "
                        "presence=%s-%s-%s-%s-%s-%s\n",
                        data[1] == 0x01 ? "ON" : "OFF",

                        data[2] == 0x41 ? "OFF" :
                            data[2] == 0xC1 ? "PAUSE" :
                            data[2] == 0xD3 ? "SEARCHING" :
                            data[2] == 0xC3 ? "PLAYING" :
                            data[2] == 0xC4 ? "FAST_FORWARD" :
                            data[2] == 0xC5 ? "REWIND" :
                            "UNKNOWN",

                        data[3] == 0x16 ? "IN" :
                            data[3] == 0x06 ? "OUT" :
                            "UNKNOWN",

                        data[4] == 0xFF ? "--" : FloatToStr(floatBuf[0], GetBcd(data[4]), 0),
                        data[5] == 0xFF ? "--" : FloatToStr(floatBuf[1], GetBcd(data[5]), 0),
                        GetBcd(data[6]),
                        data[8] == 0xFF ? "--" : FloatToStr(floatBuf[2], GetBcd(data[8]), 0),
                        GetBcd(data[7]),

                        data[10] & 0x01 ? "1" : " ",
                        data[10] & 0x02 ? "2" : " ",
                        data[10] & 0x04 ? "3" : " ",
                        data[10] & 0x08 ? "4" : " ",
                        data[10] & 0x10 ? "5" : " ",
                        data[10] & 0x20 ? "6" : " "
                    );
                    break;
                }
                default:
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                }
            } // switch
        }
        break;

        case SATNAV_STATUS_1_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#54E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#54E

            // Examples:

            // Raw: #0974 (14/15) 11 0E 54E RA0 80-00-80-00-00-80-95-06 ACK OK 9506 CRC_OK
            // Raw: #1058 ( 8/15) 11 0E 54E RA0 81-02-00-00-00-81-B2-6C ACK OK B26C CRC_OK

            SERIAL.print("--> SatNav status 1: ");

            if (dataLen != 6)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data
            //
            // data[0] & 0xF0, data[dataLen - 1] & 0xF0: always 0x80
            // data[0] & 0x07, data[dataLen - 1] & 0x07: sequence number
            //
            // data[1...2]: status ?? See below
            //
            // data[3]: usually 0x00, sometimes 0x02
            //
            // data[4]: usually 0x00, sometimes 0x0B, 0x0C, 0x0E, 0x7A
            //
            uint16_t status = (uint16_t)data[1] << 8 | data[2];

            char buffer[10];
            sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data[1], data[2]);

            // TODO - check; total guess
            SERIAL.printf(
                "status=%s%s\n",
                status == 0x0000 ? "NOT_OPERATING" :
                    status == 0x0001 ? "0x0001" :
                    status == 0x0020 ? "0x0020" : // Nearly at destination ??
                    status == 0x0080 ? "READY" :
                    status == 0x0101 ? "0x0101" :
                    //status == 0x0200 ? "INITIALISING" : // No, definitely not this
                    status == 0x0200 ? "READING_DISC_1" :
                    status == 0x0220 ? "0x0220" :
                    status == 0x0300 ? "IN_GUIDANCE_MODE_1" :
                    status == 0x0301 ? "IN_GUIDANCE_MODE_2" :
                    status == 0x0320 ? "STOPPING_GUIDANCE" :
                    //status == 0x0400 ? "TERMS_AND_CONDITIONS_ACCEPTED" : // No, definitely not this
                    status == 0x0400 ? "START_OF_AUDIO_MESSAGE" :
                    status == 0x0410 ? "ARRIVED_AT_DESTINATION_1" :
                    status == 0x0600 ? "0x0600" :
                    status == 0x0700 ? "INSTRUCTION_AUDIO_MESSAGE_START_1" :
                    status == 0x0701 ? "INSTRUCTION_AUDIO_MESSAGE_START_2" :
                    status == 0x0800 ? "END_OF_AUDIO_MESSAGE" :  // Follows 0x0400, 0x0700, 0x0701
                    status == 0x4000 ? "GUIDANCE_STOPPED" :
                    status == 0x4001 ? "0x4001" :
                    status == 0x4200 ? "ARRIVED_AT_DESTINATION_2" :
                    status == 0x9000 ? "READING_DISC_2" :
                    status == 0x9080 ? "0x9080" :
                    buffer,
                data[4] == 0x0B ? " reason=0x0B" :  // Seen with status == 0x4001
                    data[4] == 0x0C ? " reason=NO_DISC" :
                    data[4] == 0x0E ? " reason=NO_DISC" :
                    ""
            );

            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        case SATNAV_STATUS_2_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#7CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#7CE

            // Examples:

            // 0E7CEF2114

            // Raw: #0517 ( 7/15) 25 0E 7CE RA0 80-20-38-00-07-06-01-00-00-00-00-00-00-00-00-00-00-00-00-80-C4-18 ACK OK C418 CRC_OK
            // Raw: #0973 (13/15) 25 0E 7CE RA0 81-20-38-00-07-06-01-00-00-00-00-00-00-00-00-00-00-20-00-81-3D-68 ACK OK 3D68 CRC_OK
            // Raw: #1057 ( 7/15) 25 0E 7CE RA0 82-20-3C-00-07-06-01-00-00-00-00-00-00-00-00-00-00-28-00-82-1E-A0 ACK OK 1EA0 CRC_OK
            // Raw: #1635 ( 0/15) 25 0E 7CE RA0 87-21-3C-00-07-06-01-00-00-00-00-00-00-00-00-00-00-28-00-87-1A-78 ACK OK 1A78 CRC_OK

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> SatNav status 2: ");

            if (dataLen != 20)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:

            // data[0] & 0x07 - sequence number
            //
            // data[1] values:
            // - 0x11: Stopping guidance??
            // - 0x15: In guidance mode
            // - 0x20: Idle, not ready??
            // - 0x21: Idle, ready??
            // - 0x25: Busy calculating route??
            // - 0x41: Finished downloading?? Audio volume dialog??
            // - 0xC1: Finished downloading??
            // Or bits:
            // - & 0x01: ??
            // - & 0x04: ??
            // - & 0x10: ??
            // - & 0x20: ??
            // - & 0x40: ??
            // - & 0x80: ??
            //
            // data[2] values: 0x38, 0x39, 0x3A, 0x3C, 0x78, 0x79, 0x7C
            // Bits:
            // - & 0x01: GPS fix
            // - & 0x02: GPS signal lost
            // - & 0x04: Scanning for GPS signal (at startup, or when driving under bridge on in tunnel)
            // - & 0x08: Always 1
            // - & 0x70:
            //   0x30 = Disc recognized / valid
            //   0x70 = No Disc present
            //
            // data[3] - either 0x00 or 0x02
            // 
            // data[4] - always 0x07
            //
            // data[5] - either 0x01 or 0x06
            //
            // data[6] - either 0x01, 0x02, or 0x04
            //
            // data[7...8] - always 0x00
            //
            // data[9] << 8 | data[10] - always either 0x0000 or 0x01F4 (500)
            //
            // data[11...15] - always 0x00
            //
            // data[16] - vehicle speed (as measured by GPS?) in km/h. Can be negative (e.g. 0xFC) when reversing.
            //
            // data[17] values: 0x00, 0x20, 0x21, 0x22, 0x28, 0x29, 0x2A, 0x2C, 0x2D, 0x30, 0x38, 0xA1
            // Bits:
            // - & 0x01 - Loading audio fragment
            // - & 0x02 - Audio output
            // - & 0x04 - New guidance instruction??
            // - & 0x08 - Reading disc??
            // - & 0x10 - Calculating route??
            // - & 0x20 - Disc present??
            // - & 0x80 - Reached destination (sure!)
            //
            // data[18] - always 0x00
            //
            // data[19] - same as data[0]: sequence number

            // uint16_t status = (uint16_t)data[1] << 8 | data[2];

            // char buffer[12];
            // sprintf_P(buffer, PSTR("0x%04X-0x%02X"), status, data[17]);

            // SERIAL.printf(
                // "status=%s",
                // status == 0x2038 ? "CD_ROM_FOUND" :
                    // status == 0x203C ? "INITIALIZING" :  // data[17] == 0x28
                    // status == 0x213C ? "READING_CDROM" :
                    // status == 0x2139 && data[17] == 0x20 ? "DISC_PRESENT" :
                    // status == 0x2139 && data[17] == 0x28 ? "DISC_IDENTIFIED" :
                    // status == 0x2139 && data[17] == 0x29 ? "SHOWING_TERMS_AND_CONDITIONS" : 
                    // status == 0x2139 && data[17] == 0x2A ? "READ_WELCOME_MESSAGE" : 
                    // status == 0x2539 ? "CALCULATING_ROUTE" :
                    // status == 0x1539 ? "GUIDANCE_ACTIVE" :
                    // status == 0x1139 ? "GUIDANCE_STOPPED" :
                    // status == 0x2039 ? "POWER_OFF" :
                    // buffer
            // );

            SERIAL.printf("status=%s",
                data[1] == 0x11 ? "STOPPING_GUIDANCE" :
                    data[1] == 0x15 ? "IN_GUIDANCE_MODE" :
                    data[1] == 0x20 ? "IDLE_NOT_READY" :
                    data[1] == 0x21 ? "IDLE_READY" :
                    data[1] == 0x25 ? "CALCULATING_ROUTE" :
                    data[1] == 0x41 ? "???" :
                    data[1] == 0xC1 ? "FINISHED_DOWNLOADING" :
                    "??"
            );

            SERIAL.printf(", disc=%s, gps_fix=%s, gps_fix_lost=%s, gps_scanning=%s",
                (data[2] & 0x70) == 0x70 ? "NONE_PRESENT" :
                    (data[2] & 0x70) == 0x30 ? "RECOGNIZED" :
                    "??",
                data[2] & 0x01 ? "YES" : "NO",
                data[2] & 0x02 ? "YES" : "NO",
                data[2] & 0x04 ? "YES" : "NO"
            );

            if (data[17] != 0x00)
            {
                SERIAL.printf(", disc_status=%s%s%s%s%s%s%s",
                    data[17] & 0x01 ? "LOADING_AUDIO_FRAGMENT " : "",
                    data[17] & 0x02 ? "AUDIO_OUTPUT " : "",
                    data[17] & 0x04 ? "NEW_GUIDANCE_INSTRUCTION " : "",
                    data[17] & 0x08 ? "READING_DISC " : "",
                    data[17] & 0x10 ? "CALCULATING_ROUTE " : "",
                    data[17] & 0x20 ? "DISC_PRESENT" : "",
                    data[17] & 0x80 ? "REACHED_DESTINATION" : ""
                );
            } // if

            uint16_t zzz = (uint16_t)data[9] << 8 | data[10];
            if (zzz != 0x00) SERIAL.printf(", zzz=%u", zzz);

            SERIAL.printf(", gps_speed=%u km/h%s",

                // 0xE0 as boundary for "reverse": just guessing. Do we ever drive faster than 224 km/h?
                data[16] < 0xE0 ? data[16] : 0xFF - data[16] + 1,

                data[16] >= 0xE0 ? " (reverse)" : ""
            );

            SERIAL.println();
        }
        break;

        case SATNAV_STATUS_3_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8CE

            // Examples:
            // Raw: #2733 ( 8/15)  7 0E 8CE WA0 00-00-62-98 ACK OK 6298 CRC_OK
            // Raw: #2820 ( 5/15)  7 0E 8CE WA0 00-01-7D-A2 ACK OK 7DA2 CRC_OK
            // Raw: #5252 ( 2/15)  7 0E 8CE WA0 0C-02-3E-48 ACK OK 3E48 CRC_OK
            // Raw: #2041 (11/15)  7 0E 8CE WA0 0C-01-1F-06 ACK OK 1F06 CRC_OK
            // Raw: #2103 (13/15) 22 0E 8CE WA0 20-50-41-34-42-32-35-30-30-53-42-20-00-30-30-31-41-14-E6 ACK OK 14E6 CRC_OK
            // Raw: #2108 ( 3/15)  7 0E 8CE WA0 01-40-83-52 ACK OK 8352 CRC_OK
            // Raw: #5129 ( 9/15)  8 0E 8CE WA0 04-04-00-B2-B8 ACK OK B2B8 CRC_OK
            // Raw: #1803 ( 8/15)  8 0E 8CE WA0 04-02-00-83-EA ACK OK 83EA CRC_OK

            // Possible meanings of data:

            SERIAL.print("--> SatNav status 3: ");

            if (dataLen != 2 && dataLen != 3 && dataLen != 17)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            if (dataLen == 2)
            {
                uint16_t status = (uint16_t)data[0] << 8 | data[1];

                char buffer[7];
                sprintf_P(buffer, PSTR("0x%04X"), status);

                // TODO - check; total guess
                SERIAL.printf(
                    "status=%s\n",
                    status == 0x0000 ? "CALCULATING_ROUTE" :
                        status == 0x0001 ? "STOPPING_NAVIGATION" :
                        status == 0x0C01 ? "CD_ROM_FOUND" :
                        status == 0x0C02 ? "POWERING_OFF" :
                        status == 0x0140 ? "GPS_POS_FOUND" :
                        status == 0x0120 ? "ACCEPTED_TERMS_AND_CONDITIONS" :
                        status == 0x0108 ? "NAVIGATION_MENU_ENTERED" :
                        buffer
                );
            }
            else if (dataLen == 17 && data[0] == 0x20)
            {
                // Some set of ID strings. Stays the same even when the navigation CD is changed.
                SERIAL.print("system_id=");

                int at = 1;

                // Max 28 data bytes, minus header (1), plus terminating '\0'
                char txt[MAX_DATA_BYTES - 1 + 1];

                while (at < dataLen)
                {
                    strncpy(txt, (const char*) data + at, dataLen - at);
                    txt[dataLen - at] = 0;
                    SERIAL.printf("'%s' - ", txt);
                    at += strlen(txt) + 1;
                } // while

                SERIAL.println();
            } // if
        }
        break;

        case SATNAV_GUIDANCE_DATA_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#9CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9CE

            SERIAL.print("--> SatNav guidance data: ");

            if (dataLen != 16)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // This packet precedes a packet with IDEN value 0x64E (SATNAV_GUIDANCE_IDEN).
            // This packet contains direction data during guidance.
            //
            // Meanings of data:
            //
            // - data[0] & 0x0F , data[15] & 0x0F: sequence number
            //
            // - data[1], [2]: current heading in compass degrees (0...359)
            //
            // - data[3], [4]: heading to destination in compass degrees (0...359)
            //
            // - (data[5] & 0x7F) << 8 | data[6]: remaining distance to destination
            //   Unit (kilometers or meters) is encoded in most significant bit:
            //   & 0x80: 1 = in kilometers (>= 10 km), 0 = in meters (< 10 km)
            //
            // - (data[7] & 0x7F) << 8 | data[8]: distance to destination in straight line
            //   Unit (kilometers or meters) is encoded in most significant bit:
            //   & 0x80: 1 = in kilometers (>= 10 km), 0 = in meters (< 10 km)
            //
            // - (data[9] & 0x7F) << 8 | data[10]: distance to next guidance instruction
            //   Unit (kilometers or meters) is encoded in most significant bit:
            //   & 0x80: 1 = in kilometers (>= 10 km), 0 = in meters (< 10 km)
            //
            // - data [11] << 8 & data[12]: Usually 0x7FFF. If not, low values (maximum value seen is 0x0167). Some
            //   kind of heading indication? Seen only when driving on a roundabout. For indicating when to take the
            //   exit of a roundabout?
            //
            // - data [13] << 8 & data[14]: Some distance value?? If so, always in km. Or: remaining time in minutes
            //   (TTG)? Or: the number of remaining guidance instructions until destination?
            //   Decreases steadily while driving, but can jump up after a route has been recalculated. When
            //   decreasing, sometimes just skips a value.
            //
            uint16_t currHeading = (uint16_t)data[1] << 8 | data[2];
            uint16_t headingToDestination = (uint16_t)data[3] << 8 | data[4];
            uint16_t distanceToDestination = (uint16_t)(data[5] & 0x7F) << 8 | data[6];
            uint16_t gpsDistanceToDestination = (uint16_t)(data[7] & 0x7F) << 8 | data[8];
            uint16_t distanceToNextTurn = (uint16_t)(data[9] & 0x7F) << 8 | data[10];
            uint16_t headingOnRoundabout = (uint16_t)data[11] << 8 | data[12];
            uint16_t minutesToTravel = (uint16_t)data[13] << 8 | data[14];

            char floatBuf[MAX_FLOAT_SIZE];
            SERIAL.printf(
                "curr_heading=%u deg, heading_to_dest=%u deg, distance_to_dest=%u %s,"
                " distance_to_dest_straight_line=%u %s, turn_at=%u %s,\n"
                " heading_on_roundabout=%s deg, minutes_to_travel=%u\n",
                currHeading,
                headingToDestination,
                distanceToDestination,
                data[5] & 0x80 ? "Km" : "m" ,
                gpsDistanceToDestination,
                data[7] & 0x80 ? "Km" : "m" ,
                distanceToNextTurn,
                data[9] & 0x80 ? "Km" : "m",
                headingOnRoundabout == 0x7FFF ? "---" : FloatToStr(floatBuf, headingOnRoundabout, 0),
                minutesToTravel
            );
        }
        break;

        case SATNAV_GUIDANCE_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#64E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#64E

            SERIAL.print("--> SatNav guidance: ");

            if (dataLen != 3 && dataLen != 4 && dataLen != 6 && dataLen != 13 && dataLen != 23)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Packet appears as soon as the first guidance instruction is given.
            // Just before this packet there is always a packet with IDEN value 0x9CE (SATNAV_GUIDANCE_DATA_IDEN).
            //
            // Meanings of data:
            //
            // - data[0] & 0x0F , data[dataLen - 1] & 0x0F: sequence number
            //
            // - data[1]: current instruction (large icon in left of MFD)
            //   0x01: Single turn instruction ("Turn left",  dataLen = 6 or 13)
            //   0x03: Double turn instruction ("Turn left, then turn right", dataLen = 23)
            //   0x04: Turn around if possible (dataLen = 3)
            //   0x05: Follow current road until next instruction (dataLen = 4)
            //   0x06: Not on map; follow heading (dataLen = 4)
            //
            // If data[1] == 0x01 or data[1] == 0x03, then the remaining bytes (data[4], data[4...11] or
            // data[6...21]) describe the detailed shape of the large navigation icon as shown in the left of the MFD.
            // Note: the basic shape (junction or roundabout) is determined by the data[2] value as found in the
            // last received packet with data[1] == 0x05 ("Follow current road until next instruction").
            //
            // - If data[1] == 0x01 and data[2] == 0x02: fork or exit instruction; dataLen = 6
            //   * data[4]:
            //     0x41: keep left on fork
            //     0x14: keep right on fork
            //     0x12: take right exit
            //
            // - If data[1] == 0x01 and (data[2] == 0x00 || data[2] == 0x01): one "detailed instruction"; dataLen = 13
            //   * data[4...11]: current instruction ("turn left")
            //
            // - If data[1] == 0x03: two "detailed instructions"; dataLen = 23
            //   * data[6...13]: current instruction ("turn left ...")
            //   * data[14...21]: next instruction ("... then turn right")
            //
            //   A "detailed instruction" consists of 8 bytes:
            //   * 0   : turn angle in increments of 22.5 degrees, measured clockwise, starting with 0 at 6 o-clock.
            //           E.g.: 0x4 == 90 deg left, 0x8 = 180 deg = straight ahead, 0xC = 270 deg = 90 deg right.
            //   * 1   : always 0x00 ??
            //   * 2, 3: bit pattern indicating which legs are present in the junction or roundabout. Each bit set is
            //           for one leg.
            //           Lowest bit of byte 3 corresponds to the leg of 0 degrees (straight down, which is
            //           always there, because that is where we are currently driving), running clockwise up to the
            //           highest bit of byte 2, which corresponds to a leg of 337.5 degrees (very sharp right).
            //   * 4, 5: bit pattern indicating which legs in the junction are "no entry". The coding of the bits is
            //           the same as for bytes 2 and 3.
            //   * 6   : always 0x00 ??
            //   * 7   : always 0x00 ??
            //
            // - If data[1] == 0x05: wait for next instruction; follow current road (dataLen = 4)
            //   data[2]: small icon (between current street and next street, indicating next instruction)
            //     Note: the icon indicated here is shown in the large icon as soon as the "detailed instruction"
            //       (see above) is received.
            //     0x00: No icon
            //     0x01: Junction: turn right
            //     0x02: Junction: turn left
            //     0x04: Roundabout
            //     0x08: Go straight
            //     0x10: Retrieving next instruction
            //
            // - If data[1] == 0x06: not on map; follow heading (dataLen = 4)
            //   data[2]: angle of heading to maintain, in increments of 22.5 degrees, measured clockwise, starting
            //      with 0 at 6 o-clock.
            //      E.g.: 0x4 is 90 deg left, 0x8 is 180 deg (straight ahead), 0xC is 270 deg (90 deg right).
            //
            // - data[3]: ?? Seen values: 0x00, 0x01, 0x20, 0x42, 0x53
            //
            // =====
            // Just some examples:
            //
            //     01-02-53-41 is shown as:
            //           ^  |
            //           || |
            //           \\ /
            //            ++
            //            ||
            //            ||
            //       (keep left on fork)
            //
            //     01-02-53-14 is shown as:
            //           |  ^
            //           | ||
            //           \ //
            //            ++
            //            ||
            //            ||
            //       (keep right on fork)
            //
            //     01-02-53-12 is shown as:
            //            |  ^
            //            | ||
            //            |//
            //            ++
            //            ||
            //            ||
            //       (take right exit)
            //
            //     01-00-53-04-00-12-11-00-00-00-00 is shown as:
            //               /
            //              /
            //         <===+---
            //            ||
            //            ||
            //       (turn left 90 deg; ahead = +202.5 deg; right = 270 deg)
            //
            //     01-00-53-04-00-01-11-00-00-00-00 is shown as:
            //             |
            //             |
            //         <===+
            //            ||
            //            ||
            //       (turn left 90 deg; ahead = 180 deg; no right)
            //
            //     01-00-53-03-00-11-09-10-00-00-00 is:
            //                |
            //                |
            //             /==+--- (-)
            //         <==/  ||
            //               ||
            //       (turn left -67.5 deg; ahead = 180 deg; right = 270 deg: no entry)
            //
            //     01-00-53-05-00-02-21-00-00-00-00 is:
            //                  /
            //         <==\    /
            //             \==+
            //               ||
            //               ||
            //       (turn left 112.5 deg; ahead = +202.5 deg; no right)
            //
            //     01-00-53-0C-00-10-21-00-00-00-00
            //
            //         --\
            //            \--+===>
            //              ||
            //              ||
            //       (turn right 270 deg; no ahead; left = 112.5 deg)
            //
            //     01-01-53-09-00-22-21-00-00-00-00 is:
            //                /
            //               /
            //             --+
            //            /   \\
            //     --\   /     \\
            //        \--+     ++--\
            //           \    //    \--
            //            \++//
            //             ||
            //             ||
            //
            //       (take second exit on roundabout @ 202.5 degrees, exits on 112.5, 202.5 and 290.5 degrees)
            //
            //     01-00-01-0C-00-11-11-00-10-00-00 is:
            //             |
            //             |
            //      (-) ---+===>
            //            ||
            //            ||
            //       (turn right 270 deg; ahead = 180 deg; left = 90: no entry)
            //
            //     01-00-01-04-00-10-13-00-03-00-00 is:
            //
            //         <===+---
            //            /||
            //           / ||
            //          (-)
            //       (turn left 90 deg; no ahead; right = 270 deg, sharp left = 22.5 deg: no entry)
            //
            //     03-00-53-00-41-0C-00-11-01-00-00-00-00-0B-00-08-11-00-00-00-00 is:
            //             |
            //             |
            //             +===>
            //            ||
            //            ||
            //       (current instruction: turn right 270 deg; ahead = 180 deg; no left)
            //
            //     03-00-53-00-41-0C-00-10-11-00-10-00-00-04-00-12-11-00-00-00-00 is:
            //
            //      (-) ---+===>
            //            ||
            //            ||
            //       (current instruction: turn right 270 deg; no ahead; left = 90 deg: no entry)
            //
            //     03-00-53-00-63-04-00-01-11-00-00-00-00-03-00-08-09-00-00-00-00 and
            //     03-00-53-00-32-04-00-01-11-00-00-00-00-05-00-02-21-00-00-00-00 is:
            //             |
            //             |
            //         <===+
            //            ||
            //            ||
            //       (current instruction: turn left 90 deg; ahead = 180 deg; no right)
            //
            //     03-00-53-00-3A-0B-00-08-11-00-00-00-00-04-00-10-11-00-00-00-00 is:
            //
            //                 /==>
            //      (-) ---+==/
            //            ||
            //            ||
            //       (current instruction: turn right 247.2 deg; no ahead; left = 90 deg)
            //       (next instruction: turn left 90 deg; no ahead; right = 270 deg)
            //

            char buffer[20];
            sprintf_P(buffer, PSTR("unknown(0x%02X-0x%02X)"), data[1], data[2]);

            SERIAL.printf("guidance_instruction=%s\n",
                data[1] == 0x01 ? "SINGLE_TURN" :
                    data[1] == 0x03 ? "DOUBLE_TURN" :
                    data[1] == 0x04 ? "TURN_AROUND_IF_POSSIBLE" :
                    data[1] == 0x05 ? "FOLLOW_ROAD" :
                    data[1] == 0x06 ? "NOT_ON_MAP" :
                    buffer
            );

            if (data[1] == 0x01)
            {
                if (data[2] == 0x00 || data[2] == 0x01)
                {
                    if (dataLen != 13)
                    {
                        SERIAL.println("    [unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    // One "detailed instruction" in data[4...11]
                    SERIAL.print("    current_instruction=\n");
                    PrintGuidanceInstruction(data + 4);
                }
                else if (data[2] == 0x02)
                {
                    if (dataLen != 6)
                    {
                        SERIAL.println("    [unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    // Fork or exit instruction
                    SERIAL.printf("    current_instruction=%s\n",
                        data[4] == 0x41 ? "KEEP_LEFT_ON_FORK" :
                            data[4] == 0x14 ? "KEEP_RIGHT_ON_FORK" :
                            data[4] == 0x12 ? "TAKE_RIGHT_EXIT" :
                            "??"
                    );
                }
                else
                {
                    SERIAL.printf("%s\n", buffer);
                } // if
            }
            else if (data[1] == 0x03)
            {
                if (dataLen != 23)
                {
                    SERIAL.println("    [unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // Two "detailed instructions": current in data[6...13], next in data[14...21]
                SERIAL.print("    current_instruction=\n");
                PrintGuidanceInstruction(data + 6);
                SERIAL.print("    next_instruction=\n");
                PrintGuidanceInstruction(data + 14);
            }
            else if (data[1] == 0x04)
            {
                if (dataLen != 3)
                {
                    SERIAL.println("    [unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

            } // if
            else if (data[1] == 0x05)
            {
                if (dataLen != 4)
                {
                    SERIAL.println("    [unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("    follow_road_next_instruction=%s\n",
                    data[2] == 0x00 ? "NONE" :
                        data[2] == 0x01 ? "TURN_RIGHT" :
                        data[2] == 0x02 ? "TURN_LEFT" :
                        data[2] == 0x04 ? "ROUNDABOUT" :
                        data[2] == 0x08 ? "GO_STRAIGHT_AHEAD" :
                        data[2] == 0x10 ? "RETRIEVING_NEXT_INSTRUCTION" :
                        "??"
                );
            }
            else if (data[1] == 0x06)
            {
                if (dataLen != 4)
                {
                    SERIAL.println("    [unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("    not_on_map_follow_heading=%u\n", data[2]);
            } // if
        }
        break;

        case SATNAV_REPORT_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#6CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#6CE

            SERIAL.print("--> SatNav report: ");

            if (dataLen < 3)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:
            //
            // data[0]: starts low (0x02, 0x05), then increments every time with either 9 or 1.
            //   & 0x80: last packet in sequence
            //   --> Last data byte (data[dataLen - 1]) is always the same as the first (data[0]).
            //
            // data[1]: Code of report; see below
            //
            // Lists are formatted as follows:
            // - Record: terminated with '\1', containing a set of:
            //   - Strings: terminated with '\0'
            //     - Special characters:
            //       * 0x81 = '+' in solid circle, means: this destination cannot be selected with the current
            //         navigation disc. Something from UTF-8? See also U+2A01 (UTF-8 0xe2 0xa8 0x81) at:
            //         https://www.utf8-chartable.de/unicode-utf8-table.pl?start=10752&number=1024&utf8=0x&htmlent=1
            //
            // Character set is "Extended ASCII/Windows-1252"; e.g. ë is 0xEB.
            // See also: https://bytetool.web.app/en/ascii/
            //
            // Address format:
            // - Starts with "V" (Ville? Vers?) or "C" (Country? Courant? Chosen?)
            // - Country
            // - Province
            // - City
            // - District (or empty)
            // - String "G" (iGnore?) which can be followed by text that is not important for searching on, e.g.
            //   "GRue de"
            // - Street name
            // - Either:
            //   - House number (or "0" if unknown or not applicable), or
            //   - GPS coordinates (e.g. "+495456", "+060405"). Note that these coordinates are in degrees, NOT in
            //     decimal notation. So the just given example would translate to: 49°54'56"N 6°04'05"E. There
            //     seems to be however some offset, because the shown GPS coordinates do not exactly match the
            //     destination.

            #define MAX_SATNAV_STRING_SIZE 128
            static char buffer[128];
            static int offsetInBuffer = 0;

            int offsetInPacket = 1;

            if ((data[0] & 0x7F) <= 7)
            {
                // First packet of report sequence
                offsetInPacket = 2;
                offsetInBuffer = 0;

                SERIAL.printf("report=%s:\n    ", SatNavRequestStr(data[1]));
            }
            else
            {
                // TODO - check if data[0] & 0x7F has incremented by either 1 or 9 w.r.t. the last received packet.
                // If it has incremented by e.g. 10 (9+1) or 18 (9+9), then we have obviously missed a packet, so
                // appending the text of the current packet to that of the previous packet would be incorrect.

                SERIAL.print("\n    ");
            } // if

            while (offsetInPacket < dataLen - 1)
            {
                // New record?
                if (data[offsetInPacket] == 0x01)
                {
                    offsetInPacket++;
                    offsetInBuffer = 0;
                    SERIAL.print("\n    ");
                } // if

                int maxLen = dataLen - 1 - offsetInPacket;
                if (offsetInBuffer + maxLen >= MAX_SATNAV_STRING_SIZE)
                {
                    maxLen = MAX_SATNAV_STRING_SIZE - offsetInBuffer - 1;
                } // if

                strncpy(buffer + offsetInBuffer, (const char*) data + offsetInPacket, maxLen);
                buffer[offsetInBuffer + maxLen] = 0;

                offsetInPacket += strlen(buffer) + 1 - offsetInBuffer;
                if (offsetInPacket <= dataLen - 1)
                {
                    SERIAL.printf("'%s' - ", buffer);
                    offsetInBuffer = 0;
                }
                else
                {
                    offsetInBuffer = strlen(buffer);
                } // if
            } // while

            // Last packet in report sequence?
            if (data[0] & 0x80) SERIAL.print("--LAST--");
            SERIAL.println();
        }
        break;

        case MFD_TO_SATNAV_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#94E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#94E

            SERIAL.print("--> MFD to SatNav: ");

            if (dataLen != 4 && dataLen != 9 && dataLen != 11)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:
            //
            // data[0]: request code
            //
            // data[1]:
            // - 0x0D: user is selecting an option
            // - 0x0E: user is selecting an option
            // - 0x1D: user is entering data (city, street, house number, ...)
            // - 0xFF: user or MFD is requesting a list or data item
            //
            // data[2]: Type
            // - 0x00: request length of list
            // - 0x01: request list, starting at data[5] << 8 | data[6], length in data[7] << 8 | data[8]
            // - 0x02: select item as indicated in data[5] << 8 | data[6]
            //
            // data[3]: Selected letter, digit or character (A..Z, 0..9, '). 0 if not applicable.
            //
            // data[4]:
            //
            // data[5] << 8 | data[6]: selected item or offset, or 0 if not applicable.
            //
            // data[7] << 8 | data[8]: number of items or length, or 0 if not applicable.
            //
            uint16_t request = (uint16_t)data[0] << 8 | data[1];

            char buffer[7];
            sprintf_P(buffer, PSTR("0x%04X"), request);

            SERIAL.printf(
                "request=%s (%s), type=%d(%s)",
                SatNavRequestStr(data[0]),
                request == 0x021D ? "ENTER_CITY" : // 4, 9 or 11 bytes
                    request == 0x051D ? "ENTER_STREET" : // 4, 9 or 11 bytes
                    request == 0x061D ? "ENTER_HOUSE_NUMBER" : // 9 or 11 bytes
                    request == 0x080D && dataLen == 4 ? "REQUEST_LIST_SIZE_OF_CATEGORIES" :
                    request == 0x080D && dataLen == 9 ? "CHOOSE_CATEGORY" :

                    // Strange: when starting a SatNav session, the MFD always starts off by asking the number of
                    // items in the list of categories. It gets the correct answer (38) but just ignores that.
                    request == 0x08FF && dataLen == 4 ? "START_SATNAV" :

                    request == 0x08FF && dataLen == 9 ? "REQUEST_LIST_OF_CATEGORIES" :
                    request == 0x090D ? "CHOOSE_CATEGORY":
                    request == 0x0E0D ? "CHOOSE_ADDRESS_FOR_PLACES_OF_INTEREST" :
                    request == 0x0EFF ? "REQUEST_ADDRESS_FOR_PLACES_OF_INTEREST" :
                    request == 0x0FFF ? "REQUEST_NEXT_STREET" :
                    request == 0x100D ? "CHOOSE_CURRENT_ADDRESS" :
                    request == 0x10FF ? "REQUEST_CURRENT_STREET" :
                    request == 0x110E ? "CHOOSE_PRIVATE_ADDRESS" :
                    request == 0x11FF ? "REQUEST_PRIVATE_ADDRESS" :
                    request == 0x120E ? "CHOOSE_BUSINESS_ADDRESS" :
                    request == 0x12FF ? "REQUEST_BUSINESS_ADDRESS" :
                    request == 0x13FF ? "REQUEST_SOFTWARE_MODULE_VERSIONS" :
                    request == 0x1BFF && dataLen == 4 ? "REQUEST_LIST_SIZE_OF_PRIVATE_ADDRESSES" :
                    request == 0x1BFF && dataLen == 9 ? "REQUEST_PRIVATE_ADDRESSES" :
                    request == 0x1CFF && dataLen == 4 ? "REQUEST_LIST_SIZE_OF_BUSINESS_ADDRESSES" :
                    request == 0x1CFF && dataLen == 9 ? "REQUEST_BUSINESS_ADDRESSES" :
                    request == 0x1D0E ? "SELECT_FASTEST_ROUTE?" :
                    request == 0x1DFF ? "CHOOSE_DESTINATION_SHOW_CURRENT_ADDRESS" :
                    buffer,

                // Possible meanings:
                // * request == 0x021D:
                //   - type = 1: request list starting at data[5] << 8 | data[6], length in data[7] << 8 | data[8]
                //   - type = 2: choose line in data[5] << 8 | data[6]
                // * request == 0x080D:
                //   - type = 2: choose line in data[5] << 8 | data[6]
                // * request == 0x08FF:
                //   - type = 1: request list starting at data[5] << 8 | data[6], length in data[7] << 8 | data[8]

                data[2],
                data[2] == 0x00 ? "REQ_LIST_LENGTH" :
                    data[2] == 0x01 ? "REQ_LIST" :
                    data[2] == 0x02 ? "CHOOSE" :
                    "??"
            );

            if (data[3] != 0x00)
            {
                char buffer[2];
                sprintf_P(buffer, PSTR("%c"), data[3]);
                SERIAL.printf(
                    ", letter=%s",
                    (data[3] >= 'A' && data[3] <= 'Z') || (data[3] >= '0' && data[3] <= '9') || data[3] == '\'' ? buffer :
                        data[3] == ' ' ? "_" : // Space
                        data[3] == 0x01 ? "Esc" :
                        "??"
                );
            } // if

            if (dataLen >= 9)
            {
                uint16_t selectionOrOffset = (uint16_t)data[5] << 8 | data[6];
                uint16_t length = (uint16_t)data[7] << 8 | data[8];

                if (selectionOrOffset > 0 && length > 0) SERIAL.printf(", offset=%u, length=%u", selectionOrOffset, length);
                else if (selectionOrOffset > 0) SERIAL.printf(", selection=%u", selectionOrOffset);
            } // if

            SERIAL.println();
        }
        break;

        case SATNAV_TO_MFD_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#74E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#74E

            SERIAL.print("--> SatNav to MFD: ");

            if (dataLen != 27)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:

            // data[0] & 0x07: sequence number
            // data[1]: request code
            // data[4] << 8 | data[5]: number of items
            // data[17...22]: bits indicating available letters, numbers, single quote (') or space

            SERIAL.printf(
                "response=%s; list_size=%u, ",
                SatNavRequestStr(data[1]),
                (uint16_t)data[4] << 8 | data[5]
            );

            // Available letters are bit-coded in bytes 17...20
            for (int byte = 0; byte <= 3; byte++)
            {
                for (int bit = 0; bit < (byte == 3 ? 2 : 8); bit++)
                {
                    SERIAL.printf("%c", data[byte + 17] >> bit & 0x01 ? 65 + 8 * byte + bit : '.');
                } // for
            } // for

            // Special character '
            SERIAL.printf("%c", data[21] >> 6 & 0x01 ? '\'' : '.');

            // Available numbers are bit-coded in bytes 20...21, starting with '0' at bit 2 of byte 20, ending
            // with '9' at bit 3 of byte 21
            for (int byte = 0; byte <= 1; byte++)
            {
                for (int bit = (byte == 0 ? 2 : 0); bit < (byte == 1 ? 3 : 8); bit++)
                {
                    SERIAL.printf("%c", data[byte + 20] >> bit & 0x01 ? 48 + 8 * byte + bit - 2 : '.');
                } // for
            } // for

            // <Space>, printed here as '_'
            SERIAL.printf("%c", data[22] >> 1 & 0x01 ? '_' : '.');

            SERIAL.println();
        }
        break;

        case SATNAV_DOWNLOADING_IDEN:
        {
            // I think this is just a message from the SatNav system that it is processing a new navigation disc.
            // MFD shows "DOWNLOADING".

            // Examples:
            // Raw: #1593 ( 3/15)  5 0E 6F4 RA1 3A-3E NO_ACK OK 3A3E CRC_OK

            SERIAL.print("--> SatNav is DOWNLOADING: ");

            if (dataLen != 0)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.println();
        }
        break;

        case SATNAV_DOWNLOADED1_IDEN:
        {
            // SatNav system somehow indicating that it is finished "DOWNLOADING".
            // Sequence of messages is the same for different discs.

            // Examples:
            // Raw: #2894 (14/15)  7 0E A44 WA0 21-80-74-A4 ACK OK 74A4 CRC_OK
            // Raw: #2932 ( 7/15)  6 0E A44 WA0 82-D5-86 ACK OK D586 CRC_OK

            SERIAL.print("--> SatNav DOWNLOADING finished 1: ");

            if (dataLen != 1 && dataLen != 2)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.println();
        }
        break;

        case SATNAV_DOWNLOADED2_IDEN:
        {
            // SatNav system somehow indicating that it is finished "DOWNLOADING".
            // Sequence of messages is the same for different discs.

            // Examples:
            // Raw: #2896 ( 1/15)  5 0E AC4 RA1 C5-D6 NO_ACK OK C5D6 CRC_OK
            // Raw: #2897 ( 2/15)  5 0E AC4 RA1 C5-D6 NO_ACK OK C5D6 CRC_OK
            // Raw: #2898 ( 3/15) 31 0E AC4 RA0 89-61-80-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-00-89-ED-C0 ACK OK EDC0 CRC_OK
            // Raw: #2933 ( 8/15)  8 0E AC4 RA0 8A-C2-8A-21-04 ACK OK 2104 CRC_OK

            SERIAL.print("--> SatNav DOWNLOADING finished 2: ");

            if (dataLen != 0 && dataLen != 3 && dataLen != 26)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.println();
        }
        break;

        case WHEEL_SPEED_IDEN:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#744

            SERIAL.print("--> Wheel speed: ");

            if (dataLen != 5)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char floatBuf[2][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "rear right=%s km/h, rear left=%s km/h, rear right pulses=%u, rear left pulses=%u\n",
                FloatToStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 100.0, 2),
                FloatToStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2),
                (uint16_t)data[4] << 8 | data[5],
                (uint16_t)data[6] << 8 | data[7]
            );
        }
        break;

        case ODOMETER_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8FC
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8FC

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> Odometer: ");

            if (dataLen != 5)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            char floatBuf[MAX_FLOAT_SIZE];
            SERIAL.printf(
                "kms=%s\n",
                FloatToStr(floatBuf, ((uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3]) / 10.0, 1)
            );
        }
        break;

        case COM2000_IDEN:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#450

            SERIAL.print("--> COM2000: ");

            if (dataLen != 10)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.printf("\nLight switch: %s%s%s%s%s%s%s%s\n",
                data[1] & 0x01 ? "Auto light button pressed, " : "",
                data[1] & 0x02 ? "Fog light switch turned FORWARD, " : "",
                data[1] & 0x04 ? "Fog light switch turned BACKWARD, " : "",
                data[1] & 0x08 ? "Main beam handle gently ON, " : "",
                data[1] & 0x10 ? "Main beam handle fully ON, " : "",
                data[1] & 0x20 ? "All OFF, " : "",
                data[1] & 0x40 ? "Sidelights ON, " : "",
                data[1] & 0x80 ? "Low beam ON, " : ""
            );

            SERIAL.printf("Right stalk: %s%s%s%s%s%s%s%s\n",
                data[2] & 0x01 ? "Trip computer button pressed, " : "",
                data[2] & 0x02 ? "Rear wiper switched turned to screen wash position, " : "",
                data[2] & 0x04 ? "Rear wiper switched turned to position 1, " : "",
                data[2] & 0x08 ? "Screen wash, " : "",
                data[2] & 0x10 ? "Single screen wipe, " : "",
                data[2] & 0x20 ? "Screen wipe speed 1, " : "",
                data[2] & 0x40 ? "Screen wipe speed 2, " : "",
                data[2] & 0x80 ? "Screen wipe speed 3, " : ""
            );

            SERIAL.printf("Turn signal indicator: %s%s\n",
                data[3] & 0x40 ? "Left signal ON, " : "",
                data[3] & 0x80 ? "Right signal ON, " : ""
            );

            SERIAL.printf("Head unit stalk: %s%s%s%s%s\n",
                data[5] & 0x02 ? "SRC button pressed, " : "",
                data[5] & 0x03 ? "Volume down button pressed, " : "",
                data[5] & 0x08 ? "Volume up button pressed, " : "",
                data[5] & 0x40 ? "Seek backward button pressed, " : "",
                data[5] & 0x80 ? "Seek forward button pressed, " : ""
            );

            SERIAL.printf("Head unit stalk wheel position: %d\n",
                (sint8_t)data[6]);
        }
        break;

        case CDCHANGER_COMMAND_IDEN:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8EC

            // Examples:
            // 0E8ECC 1181 30F0

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> CD changer command: ");

            if (dataLen != 2)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            uint16_t cdcCommand = (uint16_t)data[0] << 8 | data[1];

            char buffer[7];
            sprintf_P(buffer, PSTR("0x%04X"), cdcCommand);

            SERIAL.printf("%s\n",
                cdcCommand == 0x1101 ? "POWER_OFF" :
                cdcCommand == 0x2101 ? "POWER_OFF" :
                cdcCommand == 0x1181 ? "PAUSE" :
                cdcCommand == 0x1183 ? "PLAY" :
                cdcCommand == 0x31FE ? "PREVIOUS_TRACK" :
                cdcCommand == 0x31FF ? "NEXT_TRACK" :
                cdcCommand == 0x4101 ? "CD 1" :
                cdcCommand == 0x4102 ? "CD 2" :
                cdcCommand == 0x4103 ? "CD 3" :
                cdcCommand == 0x4104 ? "CD 4" :
                cdcCommand == 0x4105 ? "CD 5" :
                cdcCommand == 0x4106 ? "CD 6" :
                cdcCommand == 0x41FE ? "PREVIOUS_CD" :
                cdcCommand == 0x41FF ? "NEXT_CD" :
                buffer
            );
        }
        break;

        case MFD_TO_HEAD_UNIT_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8D4
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8D4

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            SERIAL.print("--> MFD to head unit: ");

            char buffer[5];
            sprintf_P(buffer, PSTR("0x%02X"), data[1]);

            if (data[0] == 0x11)
            {
                if (dataLen != 2)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf(
                    "command=HEAD_UNIT_UPDATE_AUDIO_BITS, mute=%s, auto_volume=%s, loudness=%s, audio_menu=%s,\n"
                    "    power=%s, contact_key=%s\n",
                    data[1] & 0x01 ? "ON" : "OFF",
                    data[1] & 0x02 ? "ON" : "OFF",
                    data[1] & 0x10 ? "ON" : "OFF",
                    data[1] & 0x20 ? "OPEN" : "CLOSED",  // Bug: if CD changer is playing, this one is always "OPEN"...
                    data[1] & 0x40 ? "ON" : "OFF",
                    data[1] & 0x80 ? "ON" : "OFF"
                );
            }
            else if (data[0] == 0x12)
            {
                if (dataLen != 2 && dataLen != 11)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("command=HEAD_UNIT_SWITCH_TO, param=%s\n",
                    data[1] == 0x01 ? "TUNER" :
                    data[1] == 0x02 ? "INTERNAL_CD_OR_TAPE" :
                    data[1] == 0x03 ? "CD_CHANGER" :

                    // This is the "default" mode for the head unit, to sit there and listen to the navigation
                    // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                    // whenever this source is chosen.
                    data[1] == 0x05 ? "NAVIGATION_AUDIO" :

                    buffer
                );

                if (dataLen == 11)
                {
                    SERIAL.printf(
                        "    power=%s, source=%s,%s,%s,\n"
                        "    volume=%u%s, balance=%d%s, fader=%d%s, bass=%d%s, treble=%d%s\n",
                        data[2] & 0x01 ? "ON" : "OFF",
                        data[4] & 0x04 ? "CD_CHANGER" : "",
                        data[4] & 0x02 ? "INTERNAL_CD_OR_TAPE" : "",
                        data[4] & 0x01 ? "TUNER" : "",
                        data[5] & 0x7F,
                        data[5] & 0x80 ? "<UPD>" : "",
                        (sint8_t)(0x3F) - (data[6] & 0x7F),
                        data[6] & 0x80 ? "<UPD>" : "",
                        (sint8_t)(0x3F) - (data[7] & 0x7F),
                        data[7] & 0x80 ? "<UPD>" : "",
                        (sint8_t)(data[8] & 0x7F) - 0x3F,
                        data[8] & 0x80 ? "<UPD>" : "",
                        (sint8_t)(data[9] & 0x7F) - 0x3F,
                        data[9] & 0x80 ? "<UPD>" : ""
                    );
                } // if
            }
            else if (data[0] == 0x13)
            {
                if (dataLen != 2)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                char buffer[3];
                sprintf_P(buffer, PSTR("%u"), data[1] & 0x1F);

                SERIAL.printf("command=HEAD_UNIT_UPDATE_VOLUME, param=%s(%s%s)\n",
                    buffer,
                    data[1] & 0x40 ? "relative: " : "absolute",
                    data[1] & 0x40 ?
                        data[1] & 0x20 ? "decrease" : "increase" :
                        ""
                );
            }
            else if (data[0] == 0x14)
            {
                // Seen when the audio popup is on the MFD and a level is changed, and when the audio popup on
                // the MFD disappears.

                // Examples:
                // Raw: #5848 ( 3/15) 10 0E 8D4 WA0 14-BF-3F-43-43-51-D6 ACK OK 51D6 CRC_OK
                // Raw: #6031 (11/15) 10 0E 8D4 WA0 14-BF-3F-45-43-60-84 ACK OK 6084 CRC_OK
                // Raw: #8926 (11/15) 10 0E 8D4 WA0 14-BF-3F-46-43-F7-B0 ACK OK F7B0 CRC_OK

                if (dataLen != 5)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // TODO - bit 7 of data[1] is always 1 ?

                SERIAL.printf("command=HEAD_UNIT_UPDATE_AUDIO_LEVELS, balance=%d, fader=%d, bass=%d, treble=%d\n",
                    (sint8_t)(0x3F) - (data[1] & 0x7F),
                    (sint8_t)(0x3F) - data[2],
                    (sint8_t)data[3] - 0x3F,
                    (sint8_t)data[4] - 0x3F
                );
            }
            else if (data[0] == 0x27)
            {
                SERIAL.printf("preset_request band=%s, preset=%u\n",
                    TunerBandStr(data[1] >> 4 & 0x07),
                    data[1] & 0x0F
                );
            }
            else if (data[0] == 0x61)
            {
                if (dataLen != 4)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("command=REQUEST_CD, param=%s\n",
                    data[1] == 0x02 ? "PAUSE" :
                        data[1] == 0x03 ? "PLAY" :
                        data[3] == 0xFF ? "NEXT" :
                        data[3] == 0xFE ? "PREVIOUS" :
                        "??"
                );
            }
            else if (data[0] == 0xD1)
            {
                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("command=REQUEST_TUNER_INFO\n");
            }
            else if (data[0] == 0xD2)
            {
                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("command=REQUEST_TAPE_INFO\n");
            }
            else if (data[0] == 0xD6)
            {
                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf("command=REQUEST_CD_TRACK_INFO\n");
            }
            else
            {
                SERIAL.printf("0x%02X [to be decoded]\n", data[0]);

                return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
            } // if
        }
        break;

        case AIR_CONDITIONER_DIAG_IDEN:
        {
            SERIAL.print("--> Aircon diag: ");
            SERIAL.println("[to be decoded]");
            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        case AIR_CONDITIONER_DIAG_COMMAND_IDEN:
        {
            SERIAL.print("--> Aircon diag command: ");
            SERIAL.println("[to be decoded]");
            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        case ECU_IDEN:
        {
            SERIAL.print("--> ECU status(?): ");

            if (dataLen != 15)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // No idea who is sending this packet (engine ECU?) and what this packet means.

            // Examples:
            //
            // Raw: #3857 (12/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1D-F7-00-00-00-F6-BE NO_ACK OK F6BE CRC_OK
            // Raw: #3918 (13/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1D-FA-00-00-02-D1-5A NO_ACK OK D15A CRC_OK
            // Raw: #3977 (12/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1D-FA-00-00-00-EF-2E NO_ACK OK EF2E CRC_OK
            // Raw: #4038 (13/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1D-FB-00-00-02-9A-EC NO_ACK OK 9AEC CRC_OK
            // Raw: #4099 (14/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1E-01-00-00-01-40-E8 NO_ACK OK 40E8 CRC_OK
            // Raw: #4160 ( 0/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1E-02-00-00-01-9C-32 NO_ACK OK 9C32 CRC_OK
            // Raw: #4221 ( 1/15) 20 0E B0E W-0 01-1D-6E-B0-0B-35-5A-3C-79-E3-1E-02-00-00-00-83-08 NO_ACK OK 8308 CRC_OK
            //
            // Raw: #8351 ( 1/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-1E-FA-00-00-01-28-DE NO_ACK OK 28DE CRC_OK
            // Raw: #8470 ( 0/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-1E-FB-00-00-01-63-68 NO_ACK OK 6368 CRC_OK
            // Raw: #8527 (12/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-20-01-00-00-01-82-48 NO_ACK OK 8248 CRC_OK --> Skipping 1F ??
            // Raw: #8586 (11/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-20-02-00-00-01-5E-92 NO_ACK OK 5E92 CRC_OK
            // Raw: #8645 (10/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-20-03-00-00-01-15-24 NO_ACK OK 1524 CRC_OK
            // Raw: #8763 ( 8/15) 20 0E B0E W-0 01-1D-6E-C2-0B-35-5A-3C-79-E3-20-05-00-00-01-B3-AA NO_ACK OK B3AA CRC_OK
            //
            // Raw: #1741 (11/15) 20 0E B0E W-0 01-48-E5-02-0A-B3-E3-3B-79-FD-35-F7-00-00-01-5A-1C NO_ACK OK 5A1C CRC_OK
            // Raw: #1876 (11/15) 20 0E B0E W-0 01-48-E5-02-0A-B3-E3-3B-79-FD-35-F9-00-00-01-9F-56 NO_ACK OK 9F56 CRC_OK
            // Raw: #2010 (10/15) 20 0E B0E W-0 01-48-E5-02-0A-B3-E3-3B-79-FD-35-FA-00-00-01-43-8C NO_ACK OK 438C CRC_OK
            // Raw: #2079 ( 4/15) 20 0E B0E W-0 01-48-E5-02-0A-B3-E3-3B-79-FD-36-00-00-00-02-99-88 NO_ACK OK 9988 CRC_OK
            // Raw: #2150 ( 0/15) 20 0E B0E W-0 01-48-E5-02-0A-B3-E3-3B-79-FD-36-02-00-00-01-2F-AA NO_ACK OK 2FAA CRC_OK

            // data[9] << 16 | data[10] << 8 | data[11]: Counter incrementing by 1 per second?
            //     Counting is regardless of engine running or not.

            SERIAL.printf("counter=%lu\n",
                (uint32_t)data[9] << 16 | (uint32_t)data[10] << 8 | data[11]
            );

            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        default:
        {
            return VAN_PACKET_PARSE_UNRECOGNIZED_IDEN;
        }
        break;
    } // switch

    return VAN_PACKET_PARSE_OK;
}

void setup()
{
    SERIAL.begin(115200);
    SERIAL.println("Starting VAN bus packet parser");
    VanBus.Setup(RECV_PIN);
} // setup

void loop()
{
    int n = 0;
    while (VanBus.Available())
    {
        TVanPacketRxDesc pkt;
        bool isQueueOverrun;
        VanBus.Receive(pkt, &isQueueOverrun);

        if (isQueueOverrun) SERIAL.print("QUEUE OVERRUN!\n");

        pkt.CheckCrcAndRepair();

        // Filter on specific IDENs

        uint16_t iden = pkt.Iden();

        // Choose either of the if-statements below (not both)

        // Show all packets, but discard the following:
        if (
            false
            // || iden == VIN_IDEN
            // || iden == ENGINE_IDEN
            // || iden == HEAD_UNIT_STALK_IDEN
            // || iden == LIGHTS_STATUS_IDEN
            // || iden == DEVICE_REPORT
            // || iden == CAR_STATUS1_IDEN
            // || iden == CAR_STATUS2_IDEN
            // || iden == DASHBOARD_IDEN
            // || iden == DASHBOARD_BUTTONS_IDEN
            // || iden == HEAD_UNIT_IDEN
            // || iden == TIME_IDEN
            // || iden == AUDIO_SETTINGS_IDEN
            // || iden == MFD_STATUS_IDEN
            // || iden == AIRCON_IDEN
            // || iden == AIRCON2_IDEN
            // || iden == CDCHANGER_IDEN
            // || iden == SATNAV_STATUS_1_IDEN
            // || iden == SATNAV_STATUS_2_IDEN
            // || iden == SATNAV_STATUS_3_IDEN
            // || iden == SATNAV_GUIDANCE_DATA_IDEN
            // || iden == SATNAV_GUIDANCE_IDEN
            // || iden == SATNAV_REPORT_IDEN
            // || iden == MFD_TO_SATNAV_IDEN
            // || iden == SATNAV_TO_MFD_IDEN
            // || iden == SATNAV_DOWNLOADING_IDEN
            // || iden == SATNAV_DOWNLOADED1_IDEN
            // || iden == SATNAV_DOWNLOADED2_IDEN
            // || iden == WHEEL_SPEED_IDEN
            // || iden == ODOMETER_IDEN
            // || iden == COM2000_IDEN
            // || iden == CDCHANGER_COMMAND_IDEN
            // || iden == MFD_TO_HEAD_UNIT_IDEN
            // || iden == AIR_CONDITIONER_DIAG_IDEN
            // || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            // || iden == ECU_IDEN
           )
        {
            break;
        } // if

        // Show no packets, except the following:
        // if (
            // true
            // && iden != VIN_IDEN
            // && iden != ENGINE_IDEN
            // && iden != HEAD_UNIT_STALK_IDEN
            // && iden != LIGHTS_STATUS_IDEN
            // && iden != DEVICE_REPORT
            // && iden != CAR_STATUS1_IDEN
            // && iden != CAR_STATUS2_IDEN
            // && iden != DASHBOARD_IDEN
            // && iden != DASHBOARD_BUTTONS_IDEN
            // && iden != HEAD_UNIT_IDEN
            // && iden != TIME_IDEN
            // && iden != AUDIO_SETTINGS_IDEN
            // && iden != MFD_STATUS_IDEN
            // && iden != AIRCON_IDEN
            // && iden != AIRCON2_IDEN
            // && iden != CDCHANGER_IDEN
            // && iden != SATNAV_STATUS_1_IDEN
            // && iden != SATNAV_STATUS_2_IDEN
            // && iden != SATNAV_STATUS_3_IDEN
            // && iden != SATNAV_GUIDANCE_DATA_IDEN
            // && iden != SATNAV_GUIDANCE_IDEN
            // && iden != SATNAV_REPORT_IDEN
            // && iden != MFD_TO_SATNAV_IDEN
            // && iden != SATNAV_TO_MFD_IDEN
            // && iden != SATNAV_DOWNLOADING_IDEN
            // && iden != SATNAV_DOWNLOADED1_IDEN
            // && iden != SATNAV_DOWNLOADED2_IDEN
            // && iden != WHEEL_SPEED_IDEN
            // && iden != ODOMETER_IDEN
            // && iden != COM2000_IDEN
            // && iden != CDCHANGER_COMMAND_IDEN
            // && iden != MFD_TO_HEAD_UNIT_IDEN
            // && iden != AIR_CONDITIONER_DIAG_IDEN
            // && iden != AIR_CONDITIONER_DIAG_COMMAND_IDEN
            // && iden != ECU_IDEN
           // )
        // {
            // break;
        // } // if

        // Show packet as parsed by ISR
        int parseResult = ParseVanPacket(&pkt);

        // Show byte content only for packets that are not a duplicate of a previously received packet
        if (parseResult != VAN_PACKET_DUPLICATE) pkt.DumpRaw(SERIAL);

        // Process at most 30 packets at a time
        if (++n >= 30) break;
    } // while

    // Print statistics every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBus.DumpStats(SERIAL);
    } // if

    delay(1); // Give some time to system to process other things?
} // loop
