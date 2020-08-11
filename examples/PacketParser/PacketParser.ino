/*
 * VanBus: PacketParser - try to parse the packets, received on the VAN comfort bus, and print the result on the
 *   serial port.
 *
 * Written by Erik Tromp
 *
 * Version 0.0.1 - July, 2020
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
 * Note:
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
#define SATNAV_GUIDANCE_IDEN 0x9CE
#define AIRCON_IDEN 0x464
#define AIRCON2_IDEN 0x4DC
#define CDCHANGER_IDEN 0x4EC
#define SATNAV1_IDEN 0x54E
#define SATNAV2_IDEN 0x64E
#define SATNAV3_IDEN 0x6CE
#define SATNAV4_IDEN 0x74E
#define SATNAV5_IDEN 0x7CE
#define SATNAV6_IDEN 0x8CE
#define SATNAV7_IDEN 0x94E
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

const char* RadioBandStr(uint8_t data)
{
    return
        (data & 0x07) == 0 ? "NONE" :
        (data & 0x07) == 1 ? "FM1" :
        (data & 0x07) == 2 ? "FM2" :
        (data & 0x07) == 4 ? "FMAST" :
        (data & 0x07) == 5 ? "AM" :
        "??";
} // RadioBandStr

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
            data == 0x08 ? "PLACE_OF_INTEREST_CATEGORIES" :
            data == 0x09 ? "PLACE_OF_INTEREST_CATEGORY" :
            data == 0x0E ? "GPS_FOR_PLACE_OF_INTEREST" :
            data == 0x0F ? "CURRENT_DESTINATION" :
            data == 0x10 ? "CURRENT_ADDRESS" :
            data == 0x11 ? "PRIVATE_ADDRESS" :
            data == 0x12 ? "BUSINESS_ADDRESS" :
            data == 0x13 ? "SOFTWARE_MODULE_VERSIONS" :
            data == 0x1B ? "PRIVATE_ADDRESS_LIST" :
            data == 0x1C ? "BUSINESS_ADDRESS_LIST" :
            data == 0x1D ? "GPS_CHOOSE_DESTINATION" :
            buffer;
} // SatNavRequestStr

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

            // TODO - Always "CLOSE", even if open. Actual status of LED on dash is in packet with IDEN 0x4FC
            // (LIGHTS_STATUS_IDEN)
            //data[1] & 0x08 ? "door=OPEN" : "door=CLOSE",

            char floatBuf[3][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "dashboard_light=%s, dashboard_actual_brightness=%u; contact=%s; ignition=%s; engine=%s;\n"
                "    economy_mode=%s; in_reverse=%s; trailer=%s; water_temp=%s; odometer=%s; ext_temp=%s\n",
                data[0] & 0x80 ? "FULL" : "DIMMED (LIGHTS ON)",
                data[0] & 0x0F,
                data[1] & 0x01 ? "ACC" : "OFF",
                data[1] & 0x02 ? "ON" : "OFF",
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
                data[0] >> 4 & 0x03);
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

            SERIAL.printf("%s", data[5] & 0x02 ? "    - Automatic gearbox ENABLED\n" : "");

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
                data[5] & 0x04 ? "INDICATOR_LEFT " : "");

            if (data[6] != 0xFF)
            {
                // If you see "29.2 Â°C", then set 'Remote character set' to 'UTF-8' in
                // PuTTY setting 'Window' --> 'Translation'
                SERIAL.printf("    - Oil temperature: %d °C\n", (int)data[6] - 40);
            } // if
            //SERIAL.printf("    - Oil temperature (2): %d °C\n", (int)data[9] - 50);  // Other possibility?

            if (data[7] != 0xFF)
            {
                SERIAL.printf("Fuel level: %u %%\n", data[7]);
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
            // 0E 8C4 WA0 8AE1403D54 ACK
            // 0E 8C4 WA0 8A21403D54 ACK
            // 0E 8C4 WA0 8A24409B32 ACK

            // Raw: #7797 ( 2/15) 8 0E 8C4 WA0 07-40-00-E6-2C ACK OK E62C CRC_OK
            // Raw: #7819 ( 9/15) 6 0E 8C4 WA0 96-D8-48 ACK OK D848 CRC_OK
            // Raw: #7820 (10/15) 8 0E 8C4 WA0 8A-21-40-3D-54 ACK OK 3D54 CRC_OK
            // Raw: #7970 (10/15) 7 0E 8C4 WA0 52-20-A8-0E ACK OK A80E CRC_OK

            // Most of these packets are the same. So print only if not duplicate of previous packet.
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            if (dataLen < 1 || dataLen > 3)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            SERIAL.print("--> Device report: ");

            if (data[0] == 0x8A)
            {
                SERIAL.print("Head unit: ");

                if (dataLen != 3)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // Possible bits in data[1]:
                // 0x01 - Audio settings info
                // 0x02 - Button press info
                // 0x04 - Searching (CD track or radio station)
                // 0x08 - 
                // 0xF0 - 0x20 = Radio
                //      - 0x30 = CD track found
                //      - 0x40 = Radio preset
                //      - 0xC0 = Internal CD info
                //      - 0xD0 = CD track info

                SERIAL.printf(
                    "%s\n",
                    data[1] == 0x20 ? "RADIO_INFO" :
                    data[1] == 0x21 ? "AUDIO_SETTINGS" :
                    data[1] == 0x22 ? "BUTTON_PRESS_INFO" :
                    data[1] == 0x24 ? "RADIO_SEARCHING" :
                    data[1] == 0x30 ? "CD_TRACK_FOUND" :
                    data[1] == 0x40 ? "RADIO_PRESET_INFO" :
                    data[1] == 0xC0 ? "CD_INFO" :
                    data[1] == 0xC1 ? "CD_TAPE_AUDIO_SETTINGS" :
                    data[1] == 0xC4 ? "CD_SEARCHING" :
                    data[1] == 0xD0 ? "CD_TRACK_INFO" :
                    "??"
                );

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
                            (data[2] & 0x1F) == 0x1B ? "RADIO" :
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
                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.print("CD-changer info done\n");
            }
            else if (data[0] == 0x07)
            {
                if (dataLen != 3)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // Unknown what this is. MFD reporting what it is showing??
                // 
                // data[1] seems a bit pattern. Bits seen:
                //  & 0x01
                //  & 0x02
                //  & 0x04
                //  & 0x10
                //  & 0x20
                //  & 0x40
                //
                // data[2] is usually 0x00, sometimes 0x01

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

                // Unknown what this is
                //
                // data[1] is usually 0x08, sometimes 0x20

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
                "seq=%u; doors=%s,%s,%s,%s,%s; right_stalk_button=%s; avg_speed_1=%u; avg_speed_2=%u; "
                "fuel_level=%u litre;\n    range_1=%u; avg_consumption_1=%s; range_2=%u; avg_consumption_2=%s; "
                "inst_consumption=%s; mileage=%u\n",
                data[0] & 0x07,
                data[7] & 0x80 ? "FR" : "",
                data[7] & 0x40 ? "FL" : "",
                data[7] & 0x20 ? "RR" : "",
                data[7] & 0x10 ? "RL" : "",
                data[7] & 0x08 ? "BT" : "",
                data[10] & 0x01 ? "PRESSED" : "RELEASED",
                data[11],
                data[12],
                data[13], // TODO - total guess
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

            char floatBuf[2][MAX_FLOAT_SIZE];
            SERIAL.printf(
                "hazard_lights=%s; door=%s; dashboard_programmed_brightness=%u, esp=%s,\n"
                "    fuel_level_filtered=%s litre, fuel_level_raw=%s litre\n", // TODO - I think fuel level here is in % ?
                data[0] & 0x02 ? "ON" : "OFF",
                data[2] & 0x40 ? "LOCKED" : "UNLOCKED",
                data[2] & 0x0F,
                data[3] & 0x02 ? "OFF" : "ON",
                FloatToStr(floatBuf[0], data[4] / 2.0, 1),
                FloatToStr(floatBuf[1], data[5] / 2.0, 1)
            );
        }
        break;

        case HEAD_UNIT_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#554

            uint8_t seq = data[0];
            uint8_t infoType = data[1];

            // Head Unit info types
            enum HeatUnitInfoType_t
            {
                INFO_TYPE_RADIO = 0xD1,
                INFO_TYPE_TAPE,
                INFO_TYPE_PRESET,
                INFO_TYPE_CDCHANGER = 0xD5, // TODO - Not sure
                INFO_TYPE_CD,
            };

            switch (infoType)
            {
                case INFO_TYPE_RADIO:
                {
                    // Message when the HeadUnit is in "radio" (tuner) mode

                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_1

                    // Examples:
                    // 0E554E 80D1019206030F60FFFFA10000000000000000000080 9368
                    // 0E554E 82D1011242040F60FFFFA10000000000000000000082 3680
                    // 0E554E 87D10110CA030F60FFFFA10000000000000000000080 62E6

                    // Print only if not duplicate of previous packet
                    static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
                    if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
                    memcpy(packetData, data, dataLen);

                    SERIAL.print("--> Radio info: ");

                    // TODO - some web pages show 22 bytes data, some 23
                    if (dataLen != 22)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    char rdsTxt[9];
                    strncpy(rdsTxt, (const char*) data + 12, 8);
                    rdsTxt[8] = 0;

                    uint16_t frequency = (uint16_t)data[5] << 8 | data[4];

                    // data[11] - seems to contain PTY code
                    uint8_t ptyCode = data[11] & 0x1F;

                    // data[8] and data[9] contain PI code
                    // See also:
                    // - https://en.wikipedia.org/wiki/Radio_Data_System#Program_Identification_Code_(PI_Code),
                    // - https://radio-tv-nederland.nl/rds/rds.html
                    // - https://people.uta.fi/~jk54415/dx/pi-codes.html
                    uint16_t piCode = (uint16_t)data[8] << 8 | data[9];
                    uint8_t countryCode = data[8] >> 4 & 0x0F;

                    // data[10] - Always 0xA1 ?

                    char piBuffer[40];
                    sprintf_P(piBuffer, PSTR("%04X (country=%s)"),

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
                            "???"
                        );

                    char ptyBuffer[40];
                    sprintf_P(ptyBuffer, PSTR("%u(%s)"),

                        ptyCode,

                        // See also:
                        // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
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
                            ptyCode == 10 ? "Popular Music (Pop)" :
                            ptyCode == 11 ? "Rock Music" :
                            ptyCode == 12 ? "Easy Listening" :
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
                            "??"
                    );

                    char floatBuf[MAX_FLOAT_SIZE];
                    SERIAL.printf("position=%u, band=%s, scanning=%s%s, %smanual_scan=%s, %s %s, PTY=%s, PI=%s,\n"
                        "    %s, %s, %s, %s, \"%s\"%s\n",
                        data[2] >> 3 & 0x0F,
                        RadioBandStr(data[2]),
                        data[3] & 0x10 ? "YES" : "NO",  // scanning

                        // Distant (Dx) or Local (Lo) for AM.
                        // TODO - there seems to be also a bit for Lo/Dx reception for FM. Which one?
                        (data[2] & 0x07) != 5 ? "" : data[3] & 0x02 ? " (Dx)" : " (Lo)",

                        (data[3] & 0x10) == 0 ? "" : data[3] & 0x80 ? "scan_direction=UP, " : "scan_direction=DOWN, ",
                        data[3] & 0x08 ? "YES" : "NO",  // manual_scan
                        frequency == 0x07FF ? "---" :
                            (data[2] & 0x07) == 5
                                ? FloatToStr(floatBuf, frequency, 0)  // AM, LW
                                : FloatToStr(floatBuf, frequency / 20.0 + 50.0, 2),  // FM
                        (data[2] & 0x07) == 5 ? "KHz" : "MHz",
                        piCode == 0xFFFF ? "---" : ptyBuffer,
                        piCode == 0xFFFF ? "---" : piBuffer,
                        (data[2] & 0x07) == 5 ? "TA N/A" : data[7] & 0x40 ? "TA avaliable" : "NO TA availabe",
                        (data[2] & 0x07) == 5 ? "RDS N/A" : data[7] & 0x20 ? "RDS available" : "NO RDS available",
                        (data[2] & 0x07) == 5 ? "TA N/A" : data[7] & 0x02 ? "TA ON" : "TA OFF",
                        (data[2] & 0x07) == 5 ? "RDS N/A" : data[7] & 0x01 ? "RDS ON" : "RDS OFF",
                        rdsTxt,
                        data[7] & 0x80 ? "\n    --> Info Trafic!" : ""
                    );

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

                    SERIAL.printf("%s, %s, %s, %s\n",
                        data[2] & 0x10 ? "FAST FORWARD" : "",
                        data[2] & 0x30 ? "FAST REWIND" : "",
                        data[2] & 0x0C == 0x0C ? "PLAYING" : data[2] & 0x0C == 0x00 ? "STOPPED"  : "??",
                        data[2] & 0x01 ? "SIDE 1" : "SIDE 2"
                    );
                }
                break;

                case INFO_TYPE_PRESET:
                {
                    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_3

                    SERIAL.print("--> Radio preset info: ");

                    if (dataLen != 12)
                    {
                        SERIAL.println("[unexpected packet length]");
                        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                    } // if

                    char rdsTxt[9];
                    strncpy(rdsTxt, (const char*) data + 3, 8);
                    rdsTxt[8] = 0;

                    SERIAL.printf("band=%s, memory=%u, type=%s, \"%s\"\n",
                        RadioBandStr(data[2] >> 4 & 0x07),
                        data[2] & 0x0F,
                        data[2] & 0x80 ? "RDS_NAME" : "FREQ_NAME",
                        rdsTxt
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

                    if (data[3] == 0x11)
                    {
                        SERIAL.print("INSERTED");
                    }
                    else if (data[3] == 0x13)
                    {
                        SERIAL.print("SEARCHING");
                    }
                    else if (data[3] == 0x03)
                    {
                        SERIAL.print("PLAYING");
                    }

                    if (data[8] != 0xFF)
                    {
                        SERIAL.printf(" - %um:%us in track %u/%u",
                            GetBcd(data[5]),
                            GetBcd(data[6]),
                            GetBcd(data[7]),
                            GetBcd(data[8])
                        );

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

            SERIAL.printf(
                "seq=%u, audio_menu=%s, loudness=%s, auto_volume=%s, ext_mute=%s, mute=%s, power=%s,\n"
                "    tape=%s, cd=%s, source=%s, volume=%u%s, balance=%d%s, fader=%d%s, bass=%d%s, treble=%d%s\n",
                data[0] & 0x07,
                data[1] & 0x20 ? "OPEN" : "CLOSED",
                data[1] & 0x10 ? "ON" : "OFF",
                data[1] & 0x04 ? "ON" : "OFF",
                data[1] & 0x02 ? "ON" : "OFF",
                data[1] & 0x01 ? "ON" : "OFF",
                data[2] & 0x01 ? "ON" : "OFF",
                data[4] & 0x20 ? "PRESENT" : "NOT_PRESENT",
                data[4] & 0x40 ? "PRESENT" : "NOT_PRESENT",
                (data[4] & 0x0F) == 0x01 ? "RADIO" :
                    (data[4] & 0x0F) == 0x02 ? "INTERNAL_CD_OR_TAPE" :
                    (data[4] & 0x0F) == 0x03 ? "CD_CHANGER" :

                    // This is the "default" mode for the head unit, to sit there and listen to the navigation
                    // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                    // whenever this source is chosen.
                    (data[4] & 0x0F) == 0x05 ? "NAVIGATION_AUDIO" :

                    "???",
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

            SERIAL.printf(
                "%s\n",
                data[0] == 0x00 && data[1] == 0xFF ? "MFD_SCREEN_OFF" :
                    data[0] == 0x20 && data[1] == 0xFF ? "MFD_SCREEN_ON" :
                    buffer
            );
        }
        break;

        case SATNAV_GUIDANCE_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#9CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9CE

            SERIAL.print("--> SatNav guidance instruction: ");

            if (dataLen != 16)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // This packet precedes a packet with IDEN value 0x64E (SATNAV2_IDEN).
            // I think this packet contains direction instructions during guidance.
            //
            // Possible meanings of data:
            // - data[0] & 0x0F , data[15] & 0x0F = sequence number
            // - data[2] = number of icon to be shown on MFD ??
            //   - 0x81 : turn right
            // - data[9] << 8 | data[10] = number of meters before turn

            uint16_t metersBeforeTurn = (uint16_t)data[9] << 8 | data[10];

            SERIAL.printf("meters=%u\n", metersBeforeTurn);
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

            SERIAL.printf(
                "auto=%s; enabled=%s; on_if_necessary=%s; recirc=%s; demist=%s, rear_heat=%s, fanSpeed=%u\n",
                data[0] & 0x40 ? "AUTO" : "MANUAL",
                data[0] & 0x20 ? "YES" : "NO",
                data[0] & 0x10 ? "YES" : "NO",
                data[0] & 0x0A ? "ON" : "OFF",
                data[0] & 0x04 ? "ON" : "OFF",
                data[0] & 0x01 ? "YES" : "NO",

                // TODO - unravel byte value. Maybe this is effective (not the set) fan speed.
                // There is an offset that depends on various conditions, e.g. airco or recirc. I seem to notice fan
                // speed in fact goes up when recirc is on. Note: aircon on usually triggers recirc when interior
                // temperature is high (to speed up cooling).
                data[4]
            );
        }
        break;

        case AIRCON2_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#4DC
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4DC
            // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner2Structs.h

            // Evaporator temperature is contantly toggling between 2 values, while the rest of the data is the same.
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
                "auto=%s; enabled=%s; rear_window=%s; aircon_compressor=%s; air_con_key=%s;\n"
                "    interiorTemperature=%s, evaporatorTemperature=%s\n",
                data[0] & 0x80 ? "AUTO" : "MANUAL",
                data[0] & 0x40 ? "YES" : "NO",
                data[0] & 0x20 ? "ON" : "OFF",
                data[0] & 0x01 ? "ON" : "OFF",
                data[1] == 0x1C ? "OFF" :
                    data[1] == 0x04 ? "ACC" :
                    data[1] == 0x00 ? "ON" :
                    "INVALID",
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

            switch (dataLen)
            {
                case 0:
                {
                    //#if 0
                    SERIAL.print("--> CD Changer: ");
                    SERIAL.print("request\n");
                    //#endif
                    break;
                }
                case 12:
                {
                    SERIAL.print("--> CD Changer: ");

                    char floatBuf[2][MAX_FLOAT_SIZE];

                    SERIAL.printf(
                        "random=%s; state=%s; cartridge=%s; %s min:%s sec in track %u/%u on CD %u; "
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
                        GetBcd(data[8]),
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

        case SATNAV1_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#54E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#54E

            // Examples:

            // Raw: #0974 (14/15) 11 0E 54E RA0 80-00-80-00-00-80-95-06 ACK OK 9506 CRC_OK
            // Raw: #1058 ( 8/15) 11 0E 54E RA0 81-02-00-00-00-81-B2-6C ACK OK B26C CRC_OK

            SERIAL.print("--> SatNav 1 - SatNav status: ");

            if (dataLen != 6)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data
            // data[0] & 0x07 - sequence number

            uint16_t status = (uint16_t)data[1] << 8 | data[2];

            char buffer[10];
            sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data[1], data[2]);

            // TODO - check; total guess
            SERIAL.printf(
                "status=%s\n",
                status == 0x0080 ? "READY" :
                    status == 0x0200 ? "INITIALISING" :
                    status == 0x0301 ? "GUIDANCE_STARTED" :
                    status == 0x4000 ? "GUIDANCE_STOPPED" :
                    status == 0x0400 ? "TERMS_AND_CONDITIONS_ACCEPTED" :
                    status == 0x0800 ? "PLAYING_AUDIO_MESSAGE" :
                    status == 0x9000 ? "READING_DISC" :
                    buffer
            );

            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        case SATNAV2_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#64E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#64E

            SERIAL.print("--> SatNav 2: ");

            if (dataLen != 4)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Packet appears as soon as the first guidance instruction is given.
            // Just before this packet there is always a packet with IDEN value 0x9CE (SATNAV_GUIDANCE_IDEN).
            //
            // Possible meanings of data:
            // - data[0] & 0x0F , data[3] & 0x0F = sequence number
            // - data[1] = always 0x05
            // - data[2] = 0x01 or 0x02 - I think this is the direction icon to be shown in the MFD
            //   Possible meanings:
            //   0x01 = turn right

            char buffer[10];
            sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data[1], data[2]);

            SERIAL.printf("%s\n", buffer);

            return VAN_PACKET_PARSE_TO_BE_DECODED_IDEN;
        }
        break;

        case SATNAV3_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#6CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#6CE

            SERIAL.print("--> SatNav 3: ");

            if (dataLen < 3)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:
            //
            // data[0] - starts low (0x02, 0x05), then increments every time with either 9 or 1.
            //   & 0x80 = last packet in sequence
            //   --> Last data byte (data[dataLen - 1]) is always the same as the first (data[0]).
            //
            // data[1] - Code of report; see below
            //
            // Lists are formatted as follows:
            // - Record: terminated with '\1', containing a set of:
            //   - Strings: terminated with '\0'
            //     - Special characters:
            //       * 0x81 = '+' in solid circle, means: this destination cannot be selected with the current
            //         navigation disc. Something from UTF-8 ?
            //         See also U+2A01 (UTF-8 0xe2 0xa8 0x81) at:
            //         https://www.utf8-chartable.de/unicode-utf8-table.pl?start=10752&number=1024&utf8=0x&htmlent=1
            //
            // Character set is "Extended ASCII/Windows-1252"; e.g. ë is 0xEB.
            // See also: https://bytetool.web.app/en/ascii/
            //
            // Address format:
            // - Atarts with "V" (Ville?) or "C" (Country?)
            // - Country
            // - Province
            // - City
            // - District (or empty)
            // - String "G" which can be followed by text that is not important for searching on, e.g. "GRue de"
            // - Street name
            // - Either:
            //   - House number (or "0" if unknown), or
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
                // Start of report sequence
                offsetInPacket = 2;
                offsetInBuffer = 0;

                SERIAL.printf("report=%s:\n    ", SatNavRequestStr(data[1]));
            }
            else
            {
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

        case SATNAV4_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#74E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#74E

            SERIAL.print("--> SatNav 4 - SatNav to MFD: ");

            if (dataLen != 27)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:

            // data[0] & 0x07 - sequence number
            // data[1] - Request code

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

            // <Space>, printed as '_'
            SERIAL.printf("%c", data[22] >> 1 & 0x01 ? '_' : '.');

            SERIAL.println();
        }
        break;

        case SATNAV5_IDEN:
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

            SERIAL.print("--> SatNav 5 - SatNav status: ");

            if (dataLen != 20)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            // Possible meanings of data:

            // data[0] & 0x07 - sequence number
            //
            // data[1] values:
            // - 0x11 - Stopping guidance ??
            // - 0x15 - In guidance mode ??
            // - 0x20 - Idle, not ready ??
            // - 0x21 - Idle, ready ??
            // - 0x25 - Busy calculating route ??
            // Or bits:
            // - & 0x01 - ??
            // - & 0x04 - ??
            // - & 0x10 - ??
            // - & 0x20 - ??
            //
            // data[2] values: 0x38, 0x39 or 0x3C
            // Bits:
            // - & 0x01 - ??
            // - & 0x04 - ??
            // - & 0x08 - ??
            //
            // data[9] << 8 | data[10] - some kind of distance value ?? Seen 0x1F4 (500) for destination 100 km away
            //
            // data[17] values: 0x00, 0x20, 0x21, 0x22, 0x28, 0x2A, 0x2C, 0x2D, 0x30 or 0x38
            // Bits:
            // - & 0x01 - Showing terms and conditions (disclaimer) ??
            // - & 0x02 - Audio output ??
            // - & 0x04 - New instruction ??
            // - & 0x08 - Reading disc ??
            // - & 0x10 - Calculating route ??
            // - & 0x20 - Disc present ??

            uint16_t status = (uint16_t)data[1] << 8 | data[2];

            char buffer[12];
            sprintf_P(buffer, PSTR("0x%04X-0x%02X"), status, data[17]);

            // TODO - check; total guess
            SERIAL.printf(
                "status=%s; disc_status=%s%s%s%s%s%s, distance=%u\n",
                status == 0x2038 ? "CD_ROM_FOUND" :
                    status == 0x203C ? "INITIALIZING" :  // data[17] == 0x28
                    status == 0x213C ? "READING_CDROM" :
                    status == 0x2139 && data[17] == 0x28 ? "DISC_IDENTIFIED" :
                    status == 0x2139 && data[17] == 0x29 ? "SHOWING_TERMS_AND_CONDITIONS" : 
                    status == 0x2139 && data[17] == 0x2A ? "READ_WELCOME_MESSAGE" : 
                    status == 0x2539 ? "CALCULATING_ROUTE" :
                    status == 0x1539 ? "GUIDANCE_ACTIVE" :
                    status == 0x1139 ? "GUIDANCE_STOPPED" :
                    status == 0x2039 ? "POWER_OFF" :
                    buffer,
                data[17] & 0x01 ? "SHOWING_TERMS_AND_CONDITIONS " : "",
                data[17] & 0x02 ? "AUDIO_OUTPUT " : "",
                data[17] & 0x04 ? "NEW_GUIDANCE_INSTRUCTION " : "",
                data[17] & 0x08 ? "READING_DISC " : "",
                data[17] & 0x10 ? "CALCULATING_ROUTE " : "",
                data[17] & 0x20 ? "DISC_PRESENT" : "",
                (uint16_t)data[9] << 8 | data[10]
            );
        }
        break;

        case SATNAV6_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#8CE
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8CE

            // Examples:

            // Raw: #2733 ( 8/15) 7 0E 8CE WA0 00-00-62-98 ACK OK 6298 CRC_OK
            // Raw: #2820 ( 5/15) 7 0E 8CE WA0 00-01-7D-A2 ACK OK 7DA2 CRC_OK
            // Raw: #5252 ( 2/15) 7 0E 8CE WA0 0C-02-3E-48 ACK OK 3E48 CRC_OK

            // Raw: #2041 (11/15) 7 0E 8CE WA0 0C-01-1F-06 ACK OK 1F06 CRC_OK
            // Raw: #2103 (13/15) 22 0E 8CE WA0 20-50-41-34-42-32-35-30-30-53-42-20-00-30-30-31-41-14-E6 ACK OK 14E6 CRC_OK
            // Raw: #2108 ( 3/15) 7 0E 8CE WA0 01-40-83-52 ACK OK 8352 CRC_OK

            // Possible meanings of data:

            SERIAL.print("--> SatNav 6 - SatNav status: ");

            if (dataLen != 2 && dataLen != 17)
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
                SERIAL.print("disc_id=");

                int at = 1;

                // Max 28 data bytes, minus header (1), plus terminating 0
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

        case SATNAV7_IDEN:
        {
            // http://graham.auld.me.uk/projects/vanbus/packets.html#94E
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#94E

            SERIAL.print("--> SatNav 7 - MFD to SatNav: ");

            if (dataLen != 4 && dataLen != 9 && dataLen != 11)
            {
                SERIAL.println("[unexpected packet length]");
                return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
            } // if

            uint16_t request = (uint16_t)data[0] << 8 | data[1];

            SERIAL.printf(
                "request=%s (0x%04X) (%s?), type=%d(%s)",
                SatNavRequestStr(data[0]),
                request,
                request == 0x021D ? "ENTER_CITY" : // 4, 9 or 11 bytes
                    request == 0x051D ? "ENTER_STREET" : // 4, 9 or 11 bytes
                    request == 0x061D ? "ENTER_HOUSE_NUMBER" : // 9 or 11 bytes
                    request == 0x080D && dataLen == 4 ? "REQUEST_LIST_SIZE_OF_CATEGORIES" :
                    request == 0x080D && dataLen == 9 ? "CHOOSE_CATEGORY" :

                    // Strange: when starting a SatNav session, the MFD seems to always start by asking the number of
                    // items in the list of categories. It gets the correct answer (38) but just ignores that.
                    request == 0x08FF && dataLen == 4 ? "START_SATNAV" :

                    request == 0x08FF && dataLen == 9 ? "REQUEST_LIST_OF_CATEGORIES" :
                    request == 0x090D ? "CHOOSE_CATEGORY":
                    request == 0x0E0D ? "CHOOSE_ADDRESS_FOR_PLACES_OF_INTEREST" :
                    request == 0x0EFF ? "REQUEST_ADDRESS_FOR_PLACES_OF_INTEREST" :
                    request == 0x0FFF ? "SHOW_CURRENT_DESTINATION" :
                    request == 0x100D ? "CHOOSE_CURRENT_ADDRESS" :
                    request == 0x10FF ? "REQUEST_CURRENT_ADDRESS" :
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
                    "??",

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
                    data[2] == 0x02 ? "CHOOSE_LINE" :
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
                SERIAL.printf(
                    ", select_from=%u, select_length=%u",
                    (uint16_t)data[5] << 8 | data[6],
                    (uint16_t)data[7] << 8 | data[8]
                );
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

            // TODO - is this really MFD to head unit? Sometimes it seems the opposite: head unit reporting to MFD.
            SERIAL.print("--> MFD to head unit: ");

            // Print only if not duplicate of previous packet
            static uint8_t packetData[MAX_DATA_BYTES] = "";  // Previous packet data
            if (memcmp(data, packetData, dataLen) == 0) return VAN_PACKET_DUPLICATE;
            memcpy(packetData, data, dataLen);

            // TODO data[1] == 0x7D --> MFD is finished requesting preset stations

            char buffer[5];
            sprintf_P(buffer, PSTR("0x%02X"), data[1]);

            if (data[0] == 0x11)
            {
                if (dataLen != 2)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                // data[1] & 0x03 == 0x02 --> ON/OFF
                // data[1] & 0x03 == 0x03 --> MUTE
                SERIAL.printf(
                    "command=HEAD_UNIT_UPDATE_SETTINGS, param=%s\n",
                    data[1] == 0x02 ? "POWER_OFF" :
                    data[1] == 0x80 ? "0x80" : // ??
                    data[1] == 0x82 ? "POWER_OFF" :
                    data[1] == 0x83 ? "MUTE" :
                    data[1] == 0xC0 ? "LOUDNESS_ON" :  // Never seen
                    data[1] == 0xC2 ? "RADIO_ON" :
                    data[1] == 0xC3 ? "RADIO_MUTE" :
                    data[1] == 0xF2 ? "CD_CHANGER_ON" :
                    data[1] == 0xF3 ? "CD_CHANGER_MUTE" :
                    buffer
                );

                // TODO
            }
            else if (data[0] == 0x12)
            {
                if (dataLen != 2 && dataLen != 16)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf(
                    "command=HEAD_UNIT_SWITCH_TO, param=%s\n",
                    data[1] == 0x01 ? "RADIO" :
                    data[1] == 0x02 ? "INTERNAL_CD_OR_TAPE" :
                    data[1] == 0x03 ? "CD_CHANGER" :

                    // TODO - or OFF? Check! I think this is the "default" mode for the head unit, to sit there and
                    // listen to the navigation audio. Check by setting a specific volume for the navigation.
                    data[1] == 0x05 ? "NAVIGATION_AUDIO" :

                    buffer
                );

                if (dataLen == 16)
                {
                    SERIAL.printf(
                        "    power=%s, source=%s,%s,%s,\n"
                        "    volume=%u%s, balance=%d%s, fader=%d%s, bass=%d%s, treble=%d%s\n",
                        data[2] & 0x01 ? "ON" : "OFF",
                        data[4] & 0x04 ? "CD_CHANGER" : "",
                        data[4] & 0x02 ? "INTERNAL_CD_OR_TAPE" : "",
                        data[4] & 0x01 ? "RADIO" : "",
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

                SERIAL.printf(
                    "command=HEAD_UNIT_UPDATE_VOLUME, param=%s(%s%s)\n",
                    buffer,
                    data[1] & 0x40 ? "relative: " : "absolute",
                    data[1] & 0x40 ?
                        data[1] & 0x20 ? "decrease" : "increase" :
                        ""
                );
            }
            else if (data[0] == 0x61)
            {
                if (dataLen != 4)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf(
                    "command=REQUEST_CD, param=%s\n",
                    data[1] == 0x03 ? "PLAY" :
                        data[3] == 0xFF ? "NEXT" :
                        data[3] == 0xFE ? "PREV" :
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

                SERIAL.printf(
                    "command=REQUEST_RADIO_INFO\n"
                );
            }
            else if (data[0] == 0xD6)
            {
                if (dataLen != 1)
                {
                    SERIAL.println("[unexpected packet length]");
                    return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
                } // if

                SERIAL.printf(
                    "command=REQUEST_CD_TRACK_INFO\n"
                );
            }
            else if (data[0] == 0x27)
            {
                SERIAL.printf("preset_request band=%s, preset=%u\n",
                    //data[1] >> 4 & 0x0F,
                    RadioBandStr(data[1] >> 4),
                    data[1] & 0x0F
                );
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
    Serial.println("Starting VAN bus packet parser");
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

        if (isQueueOverrun) Serial.print("QUEUE OVERRUN!\n");

        pkt.CheckCrcAndRepair();

        // Filter on specific IDENs

        uint16_t iden = pkt.Iden();

        // Choose either of the if-statements below (not both)

        // Show all packets, but filter out the following:
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
            // || iden == SATNAV_GUIDANCE_IDEN
            // || iden == AIRCON_IDEN
            // || iden == AIRCON2_IDEN
            // || iden == CDCHANGER_IDEN
            // || iden == SATNAV1_IDEN
            // || iden == SATNAV2_IDEN
            // || iden == SATNAV3_IDEN
            // || iden == SATNAV4_IDEN
            // || iden == SATNAV5_IDEN
            // || iden == SATNAV6_IDEN
            // || iden == SATNAV7_IDEN
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
            // && iden != SATNAV_GUIDANCE_IDEN
            // && iden != AIRCON_IDEN
            // && iden != AIRCON2_IDEN
            // && iden != CDCHANGER_IDEN
            // && iden != SATNAV1_IDEN
            // && iden != SATNAV2_IDEN
            // && iden != SATNAV3_IDEN
            // && iden != SATNAV4_IDEN
            // && iden != SATNAV5_IDEN
            // && iden != SATNAV6_IDEN
            // && iden != SATNAV7_IDEN
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
} // loop
