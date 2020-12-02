/*
 * VanBus packet parser
 * Shrink-wraps the data of recognized packets neatly into a JSON format
 *
 * Written by Erik Tromp
 *
 * Version 0.1 - June, 2020
 *
 * MIT license, all text above must be included in any redistribution.   
 */

// Uncomment to see the JSON buffers printed on the Serial port
//#define PRINT_JSON_BUFFERS_ON_SERIAL

#define JSON_BUFFER_SIZE 1024
char jsonBuffer[JSON_BUFFER_SIZE];

enum VanPacketParseResult_t
{
    VAN_PACKET_DUPLICATE  = 1, // Packet was the same as the last with this IDEN field
    VAN_PACKET_PARSE_OK = 0,  // Packet was parsed OK
    VAN_PACKET_PARSE_CRC_ERROR = -1,  // Packet had a CRC error
    VAN_PACKET_PARSE_UNEXPECTED_LENGTH = -2,  // Packet had unexpected length
    VAN_PACKET_PARSE_UNRECOGNIZED_IDEN = -3,  // Packet had unrecognized IDEN field
    VAN_PACKET_PARSE_TO_BE_DECODED = -4  // IDEN recognized but the correct parsing of this packet is not yet known
}; // enum VanPacketParseResult_t

typedef VanPacketParseResult_t (*TPacketParser)(const char*, TVanPacketRxDesc&, char*, int);

struct IdenHandler_t
{
    uint16_t iden;
    char* idenStr;
    int dataLen;
    TPacketParser parser;
    uint8_t prevData[VAN_MAX_DATA_BYTES];
}; // struct IdenHandler_t

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
}; // enum TunerBand_t

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
}; // enum TunerScanMode_t

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

const char* RadioPiCountry(uint8_t countryCode)
{
    // https://radio-tv-nederland.nl/rds/PI%20codes%20Europe.jpg
    // More than one country is assigned to the same code, just listing the most likely.
    return
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
        "???";
} // RadioPiCountry

const char* RadioPiAreaCoverage(uint8_t coverageCode)
{
    // https://www.pira.cz/rds/show.asp?art=rds_encoder_support
    return
        coverageCode == 0x00 ? "local" :
        coverageCode == 0x01 ? "international" :
        coverageCode == 0x02 ? "national" :
        coverageCode == 0x03 ? "supra-regional" :
        "regional";
} // RadioPiAreaCoverage

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
    Serial.printf("      %s%s%s%s%s\n",
        data[5] & 0x40 ? "(-)" : "   ",
        data[5] & 0x80 ? "(-)" : "   ",
        data[4] & 0x01 ? "(-)" : "   ",
        data[4] & 0x02 ? "(-)" : "   ",
        data[4] & 0x04 ? "(-)" : "   "
    );
    Serial.printf("       %s  %s  %s  %s  %s\n",
        data[0] == 6 ? "\\" : data[3] & 0x40 ? "." : " ",
        data[0] == 7 ? "\\" : data[3] & 0x80 ? "." : " ",
        data[0] == 8 ? "|" : data[2] & 0x01 ? "." : " ",
        data[0] == 9 ? "/" : data[2] & 0x02 ? "." : " ",
        data[0] == 10 ? "/" : data[2] & 0x04 ? "." : " "
    );
    Serial.printf("    %s%s           %s%s\n",
        data[5] & 0x20 ? "(-)" : "   ",
        data[0] == 5 ? "-" : data[3] & 0x20 ? "." : " ",
        data[0] == 11 ? "-" : data[2] & 0x08 ? "." : " ",
        data[4] & 0x08 ? "(-)" : "   "
    );
    Serial.printf("    %s%s     +     %s%s\n",
        data[5] & 0x10 ? "(-)" : "   ",
        data[0] == 4 ? "-" : data[3] & 0x10 ? "." : " ",
        data[0] == 12 ? "-" : data[2] & 0x10 ? "." : " ",
        data[4] & 0x10 ? "(-)" : "   "
    );
    Serial.printf("    %s%s     |     %s%s\n",
        data[5] & 0x08 ? "(-)" : "   ",
        data[0] == 3 ? "-" : data[3] & 0x08 ? "." : " ",
        data[0] == 13 ? "-" : data[2] & 0x20 ? "." : " ",
        data[4] & 0x20 ? "(-)" : "   "
    );
    Serial.printf("       %s  %s  |  %s  %s\n",
        data[0] == 2 ? "/" : data[3] & 0x04 ? "." : " ",
        data[0] == 1 ? "/" : data[3] & 0x02 ? "." : " ",
        data[0] == 14 ? "\\" : data[3] & 0x40 ? "." : " ",
        data[0] == 15 ? "\\" : data[3] & 0x80 ? "." : " "
    );
    Serial.printf("      %s%s%s%s%s\n",
        data[5] & 0x04 ? "(-)" : "   ",
        data[5] & 0x02 ? "(-)" : "   ",
        data[5] & 0x01 ? "(-)" : "   ",
        data[4] & 0x40 ? "(-)" : "   ",
        data[4] & 0x80 ? "(-)" : "   "
    );
} // PrintGuidanceInstruction

#if 0
#include <PrintEx.h>
const char* PacketRawToStr(TVanPacketRxDesc& pkt)
{
    static char dumpBuffer[MAX_DUMP_RAW_SIZE];

    // Dump to a string-stream
    GString str(dumpBuffer);
    PrintAdapter streamer(str);
    pkt.DumpRaw(streamer, '\0');

    return dumpBuffer;
} // PacketRawToStr

// Dump the raw packet data into a JSON object
VanPacketParseResult_t DefaultPacketParser(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "%s": "%s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter, idenStr, PacketRawToStr(pkt));

    return VAN_PACKET_PARSE_OK;
} // DefaultPacketParser
#endif

VanPacketParseResult_t ParseVinPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#E24
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#E24

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "vin": "%-17.17s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter, pkt.Data());

    return VAN_PACKET_PARSE_OK;
} // ParseVinPkt

VanPacketParseResult_t ParseEnginePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8A4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8A4

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "dash_light": "%S",
            "dash_actual_brightness": "%u",
            "contact_key_position": "%s",
            "engine": "%s",
            "economy_mode": "%s",
            "in_reverse": "%s",
            "trailer": "%s",
            "water_temp": "%s",
            "odometer_1": "%s",
            "exterior_temperature": "%s"
        }
    })===";

    char floatBuf[3][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? PSTR("FULL") : PSTR("DIMMED (LIGHTS ON)"),
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

    return VAN_PACKET_PARSE_OK;
} // ParseEnginePkt

VanPacketParseResult_t ParseHeadUnitStalkPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9C4

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "head_unit_stalk_buttons": "%s %s %s %s %s",
            "head_unit_stalk_wheel": "%d",
            "head_unit_stalk_wheel_rollover": "%u"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? "NEXT" : "",
        data[0] & 0x40 ? "PREV" : "",
        data[0] & 0x08 ? "VOL_UP" : "",
        data[0] & 0x04 ? "VOL_DOWN" : "",
        data[0] & 0x02 ? "SOURCE" : "",

        data[1] - 0x80,
        data[0] >> 4 & 0x03
    );

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitStalkPkt

VanPacketParseResult_t ParseLightsStatusPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_1
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanInstrumentClusterV1Structs.h

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 14) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "instrument_cluster": "%sENALED",
            "speed_regulator_wheel": "%s",
            "warning_led": "%s",
            "diesel_glow_plugs": "%s",
            "door_open": "%s",
            "remaining_km_to_service": "%u",
            "remaining_km_to_service_dash": "%u",
            "lights": "%s%s%s%s%s%s"
    )===";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? "" : "NOT ",
        data[0] & 0x40 ? "ON" : "OFF",
        data[0] & 0x20 ? "ON" : "OFF",
        data[0] & 0x04 ? "ON" : "OFF",
        data[1] & 0x01 ? "YES" : "NO",

        ((uint16_t)data[2] << 8 | data[3]) * 20,
        (((uint16_t)data[2] << 8 | data[3]) * 20) / 100 * 100,

        data[5] & 0x80 ? "DIPPED_BEAM " : "",
        data[5] & 0x40 ? "HIGH_BEAM " : "",
        data[5] & 0x20 ? "FOG_FRONT " : "",
        data[5] & 0x10 ? "FOG_REAR " : "",
        data[5] & 0x08 ? "INDICATOR_RIGHT " : "",
        data[5] & 0x04 ? "INDICATOR_LEFT " : ""
    );

    if (data[5] & 0x02)
    {
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(",\n\"auto_gearbox\": \"%s%s%s%s\""),
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

    if (data[6] != 0xFF)
    {
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"oil_temperature\": \"%d\""), (int)data[6] - 40);  // Never seen this
    } // if

    if (data[7] != 0xFF)
    {
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"fuel_level\": \"%u\""), data[7]);  // Never seen this
    } // if

    at += at >= JSON_BUFFER_SIZE ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_raw\": \"%u\""), data[8]),

    at += at >= JSON_BUFFER_SIZE ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_dash\": \"%s\""),
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
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"lpg_fuel_level\": \"%s\""),
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

        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control\": \"%s\""),
                data[11] == 0x41 ? "OFF" :
                data[11] == 0x49 ? "Cruise" :
                data[11] == 0x59 ? "Cruise - speed" :
                data[11] == 0x81 ? "Limiter" :
                data[11] == 0x89 ? "Limiter - speed" :
                "?"
            );

        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control_speed\": \"%u\""), data[12]);
    } // if

    at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));

    return VAN_PACKET_PARSE_OK;
} // ParseLightsStatusPkt

VanPacketParseResult_t ParseDeviceReportPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8C4

    int dataLen = pkt.DataLen();
    if (dataLen < 1 || dataLen > 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    if (data[0] == 0x8A)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data": {
                "head_unit_report": "%s"
        )===";

        int at = snprintf_P(buf, n, jsonFormatter,

            data[1] == 0x20 ? "TUNER_REPLY" :
            data[1] == 0x21 ? "AUDIO_SETTINGS_ANNOUNCE" :
            data[1] == 0x22 ? "BUTTON_PRESS_ANNOUNCE" :
            data[1] == 0x24 ? "TUNER_ANNOUNCEMENT" :
            data[1] == 0x28 ? "TAPE_PRESENCE_ANNOUNCEMENT" :
            data[1] == 0x30 ? "CD_PRESENT" :
            data[1] == 0x40 ? "TUNER_PRESETS_REPLY" :
            data[1] == 0x60 ? "TAPE_INFO_REPLY" :
            data[1] == 0x61 ? "TAPE_PLAYING_AUDIO_SETTINGS_ANNOUNCE" :
            data[1] == 0x62 ? "TAPE_PLAYING_BUTTON_PRESS_ANNOUNCE" :
            data[1] == 0x64 ? "TAPE_PLAYING_STARTING" :
            data[1] == 0x68 ? "TAPE_PLAYING_INFO" :
            data[1] == 0xC0 ? "INTERNAL_CD_TRACK_INFO_REPLY" :
            data[1] == 0xC1 ? "INTERNAL_CD_PLAYING_AUDIO_SETTINGS_ANNOUNCE" :
            data[1] == 0xC2 ? "INTERNAL_CD_PLAYING_BUTTON_PRESS_ANNOUNCE" :
            data[1] == 0xC4 ? "INTERNAL_CD_PLAYING_SEARCHING" :
            data[1] == 0xD0 ? "INTERNAL_CD_PLAYING_TRACK_INFO" :
            "??"
        );

        // Button-press announcement?
        if ((data[1] & 0x0F) == 0x02)
        {
            char buffer[5];
            sprintf_P(buffer, PSTR("0x%02X"), data[2]);

            at += at >= JSON_BUFFER_SIZE ? 0 :
                snprintf_P(buf + at, n - at, PSTR(",\n\"head_unit_button_pressed\": \"%s%s%s\""),

                    (data[2] & 0x1F) == 0x01 ? "'1'" :
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

        at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));
    }
    else if (data[0] == 0x96)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "cd_changer": "STATUS_UPDATE_ANNOUNCE"
            }
        }
        )===";

        snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0x07)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Unknown what this is
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    }
    else if (data[0] == 0x52)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Unknown what this is
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    return VAN_PACKET_PARSE_OK;
} // ParseDeviceReportPkt

VanPacketParseResult_t ParseCarStatus1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#564
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#564

    const uint8_t* data = pkt.Data();
    int dataLen = pkt.DataLen();

    // Process only if not duplicate of previous packet; ignore different sequence numbers
    static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
    if (memcmp(data + 1, packetData, dataLen - 2) == 0) return VAN_PACKET_DUPLICATE;
    memcpy(packetData, data + 1, dataLen - 2);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data": {
            "doors": "%s %s %s %s %s",
            "right_stalk_button": "%s",
            "avg_speed_1": "%u",
            "avg_speed_2": "%u",
            "exp_moving_avg_speed": "%u",
            "range_1": "%u",
            "avg_consumption_1": "%s",
            "range_2": "%u",
            "avg_consumption_2": "%s",
            "inst_consumption": "%s",
            "mileage": "%u"
        }
    })===";

    char floatBuf[3][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
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

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus1Pkt

VanPacketParseResult_t ParseCarStatus2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#524
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#524
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV1.h
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV2.h

    int dataLen = pkt.DataLen();
    if (dataLen != 14 && dataLen != 16) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

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

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "notification",
        "data": {
            "alarm_list": [
    )===";

    int at = snprintf_P(buf, n, jsonFormatter);

    const uint8_t* data = pkt.Data();

    bool first = true;
    for (int byte = 0; byte < 9; byte++)
    {
        for (int bit = 0; bit < 8; bit++)
        {
            if (data[byte] >> bit & 0x01)
            {
                char alarmText[80];  // Make sure this is large enough for the largest string it must hold
                strncpy_P(alarmText, (char *)pgm_read_dword(&(msgTable[byte * 8 + bit])), sizeof(alarmText) - 1);
                alarmText[sizeof(alarmText) - 1] = 0;
                at += at >= JSON_BUFFER_SIZE ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%s\n\"%s\""), first ? "" : ",", alarmText);
                first = false;
            } // if
        } // for
    } // for

    at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]\n}\n}"));

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus2Pkt

VanPacketParseResult_t ParseDashboardPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#824
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#824

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "rpm": "%s",
            "speed": "%s"
        }
    })===";

    char floatBuf[2][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
        data[0] == 0xFF && data[1] == 0xFF ?
            "---.-" :
            FloatToStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 8.0, 1),
        data[2] == 0xFF && data[3] == 0xFF ?
            "---.--" :
            FloatToStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2)
    );

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardPkt

VanPacketParseResult_t ParseDashboardButtonsPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#664
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#664

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "hazard_lights": "%s",
            "door_lock": "%s",
            "dashboard_programmed_brightness": "%u",
            "esp": "%s",
            "fuel_level_filtered": "%s"
            "fuel_level_raw": "%s"
        }
    })===";

    // data[6..10] - always 00-FF-00-FF-00

    char floatBuf[2][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
        data[0] & 0x02 ? "ON" : "OFF",
        data[2] & 0x40 ? "LOCKED" : "UNLOCKED",
        data[2] & 0x0F,
        data[3] & 0x02 ? "ON" : "OFF",

        // Surely fuel level. Test with tank full shows definitely level is in litres.
        FloatToStr(floatBuf[0], data[4] / 2.0, 1),
        FloatToStr(floatBuf[1], data[5] / 2.0, 1)
    );

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardButtonsPkt

VanPacketParseResult_t ParseHeadUnitPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#554

    // These packets are sent by the head unit

    // Head Unit info types
    enum HeatUnitInfoType_t
    {
        INFO_TYPE_TUNER = 0xD1,
        INFO_TYPE_TAPE,
        INFO_TYPE_PRESET,
        INFO_TYPE_CDCHANGER = 0xD5, // TODO - Not sure
        INFO_TYPE_CD,
    }; // enum HeatUnitInfoType_t

    const uint8_t* data = pkt.Data();
    uint8_t infoType = data[1];
    int dataLen = pkt.DataLen();

    switch (infoType)
    {
        case INFO_TYPE_TUNER:
        {
            // Message when the HeadUnit is in "tuner" (radio) mode

            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_1

            if (dataLen != 22) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // data[2]: radio band and preset position
            uint8_t band = data[2] & 0x07;
            uint8_t presetMemory = data[2] >> 3 & 0x0F;

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
            //   - Mono (not stereo) bit
            //   - Music/Speech (MS) bit
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
            sprintf_P(piBuffer, PSTR("%04X(%s, %s)"),
                piCode,
                RadioPiCountry(countryCode),
                RadioPiAreaCoverage(coverageCode)
            );

            char currPtyBuffer[40];
            sprintf_P(currPtyBuffer, PSTR("%s(%u)"), PtyStr(currPty), currPty);

            char selectedPtyBuffer[40];
            sprintf_P(selectedPtyBuffer, PSTR("%s(%u)"), PtyStr(selectedPty), selectedPty);

            char presetMemoryBuffer[20];
            sprintf_P(presetMemoryBuffer, PSTR("%u"), presetMemory);

            bool anyScanBusy = (scanMode != TS_NOT_SEARCHING);

            static char jsonFormatterCommon[] PROGMEM = R"===(
            {
                "event": "display",
                "data":
                {
                    "band": "%s",
                    "memory": "%s",
                    "frequency": "%s %S",
                    "signal_strength": "%s",
                    "scan_mode": "%s",
                    "scan_sensitivity": "%s",
                    "scan_direction": "%s")===";

            char floatBuf[MAX_FLOAT_SIZE];
            int at = snprintf_P(buf, n, jsonFormatterCommon,
                TunerBandStr(band),
                presetMemory == 0 ? "-" : presetMemoryBuffer,
                frequency == 0x07FF ? "---" :
                    band == TB_AM
                        ? FloatToStr(floatBuf, frequency, 0)  // AM and LW bands
                        : FloatToStr(floatBuf, frequency / 20.0 + 50.0, 2),  // FM bands
                band == TB_AM ? PSTR("KHz") : PSTR("MHz"),

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
                static char jsonFormatterFmBand[] PROGMEM = R"===(,
                    "pty_selection_menu": "%s",
                    "selected_pty": "%s",
                    "pty_standby_mode": "%s",
                    "pty_match": "%s",
                    "pty": "%s",
                    "pi": "%s",
                    "regional": "%s",
                    "ta_selected": "%s",
                    "ta_available": "%s",
                    "rds_selected": "%s",
                    "rds_available": "%s",
                    "rds_text": "%s",
                    "info_trafic": "%s"
                )===";

                at += at >= JSON_BUFFER_SIZE ? 0 :
                    snprintf_P(buf + at, n - at, jsonFormatterFmBand,
                        ptySelectionMenu ? "ON" : "OFF",
                        selectedPtyBuffer,
                        ptyStandbyMode ? "YES" : "NO",
                        ptyMatch ? "YES" : "NO",
                        currPty == 0x00 ? "---" : currPtyBuffer,

                        piCode == 0xFFFF ? "---" : piBuffer,
                        regional ? "ON" : "OFF",
                        taSelected ? "YES" : "NO",
                        taAvailable ? "YES" : "NO",
                        rdsSelected ? "YES" : "NO",
                        rdsAvailable ? "YES" : "NO",
                        rdsTxt,

                        taAnnounce ? "Info Trafic!" : ""
                    );
            } // if

            at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));
        }
        break;
        
        case INFO_TYPE_TAPE:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_2

            if (dataLen != 5) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            uint8_t status = data[2] & 0x3C;
            char buffer[5];
            sprintf_P(buffer, PSTR("0x%02X"), status);

            static char jsonFormatter[] PROGMEM = R"===(
            {
                "event": "display",
                "data":
                {
                    "tape_status": "%s",
                    "tape_side": "%s"
                }
            })===";

            snprintf_P(buf, n, jsonFormatter,
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

            if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            const char* tunerBand = TunerBandStr(data[2] >> 4 & 0x07);
            uint8_t tunerMemory = data[2] & 0x0F;

            char rdsOrFreqTxt[9];
            strncpy(rdsOrFreqTxt, (const char*) data + 3, 8);
            rdsOrFreqTxt[8] = 0;

            static char jsonFormatter[] PROGMEM = R"===(
            {
                "event": "display",
                "data":
                {
                    "radio_preset_%s_%u": "%s (%s)"
                }
            })===";

            snprintf_P(buf, n, jsonFormatter,
                tunerBand,
                tunerMemory,
                rdsOrFreqTxt,
                data[2] & 0x80 ? "RDS_TEXT" : "FREQUENCY"
            );
        }
        break;

        case INFO_TYPE_CD:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_6

            // TODO - do we know the fixed numbers? Seems like this can only be 10 or 12.
            if (dataLen < 10) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            static char jsonFormatter[] PROGMEM = R"===(
            {
                "event": "display",
                "data":
                {
                    "cd_status": "%s",
                    "cd_track_min": "%u",
                    "cd_track_sec": "%u",
                    "cd_current_track": "%u"
            )===";

            int at = snprintf_P(buf, n, jsonFormatter,
                GetBcd(data[5]),
                GetBcd(data[6]),
                GetBcd(data[7])
            );

            if (data[8] != 0xFF)
            {
                at += at >= JSON_BUFFER_SIZE ? 0 :
                    snprintf_P(buf + at, n - at, PSTR(",\n\"cd_total_tracks\": \"%u\""), GetBcd(data[8]));

                if (dataLen >= 12 && data[9] != 0xFF)
                {
                    at += at >= JSON_BUFFER_SIZE ? 0 :
                        snprintf_P(buf + at, n - at, PSTR(",\n\"cd_total_mins\": \"%u\",\n\"cd_total_secs\": \"%u\""),
                            GetBcd(data[9]),
                            GetBcd(data[10])
                        );
                } // if
            } // if

            at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));
        }
        break;

        case INFO_TYPE_CDCHANGER:
        {
            return VAN_PACKET_PARSE_TO_BE_DECODED;
        }
        break;

        default:
        {
            return VAN_PACKET_PARSE_TO_BE_DECODED;
        }
        break;
    } // switch

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitPkt

VanPacketParseResult_t ParseTimePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#984
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#984

    // TODO - seems to have nothing to do with time. Mine is always the same:
    //   Raw: #2692 ( 7/15) 10 0E 984 W-0 00-00-00-06-08-D0-C8 NO_ACK OK D0C8 CRC_OK
    // regardless of the time.

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "uptime_battery": "%u",
            "uptime": "%uh%um"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,
        (uint16_t)data[0] << 8 | data[1],
        data[3],
        data[4]
    );

    return VAN_PACKET_PARSE_OK;
} // ParseTimePkt

VanPacketParseResult_t ParseAudioSettingsPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4D4
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanRadioInfoStructs.h

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "power": "%s",
            "tape": "%s",
            "cd": "%s",
            "source": "%s",
            "ext_mute": "%s",
            "mute": "%s",
            "volume": "%u%s",
            "audio_menu": "%s",
            "bass": "%d%s",
            "treble": "%d%s",
            "loudness": "%s",
            "fader": "%d%s",
            "balance": "%d%s",
            "auto_volume": "%s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,
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

        // ext_mute. Activated when head unit ISO connector A pin 1 ("Phone mute") is pulled LOW (to Ground).
        data[1] & 0x02 ? "ON" : "OFF",

        // mute. To activate: press both VOL_UP and VOL_DOWN buttons on stalk.
        data[1] & 0x01 ? "ON" : "OFF",

        data[5] & 0x7F,  // volume
        data[5] & 0x80 ? "(UPD)" : "",

        // audio_menu. Bug: if CD changer is playing, this one is always "OPEN" (even if it isn't).
        data[1] & 0x20 ? "OPEN" : "CLOSED",

        (sint8_t)(data[8] & 0x7F) - 0x3F,  // bass
        data[8] & 0x80 ? "(UPD)" : "",
        (sint8_t)(data[9] & 0x7F) - 0x3F,  // treble
        data[9] & 0x80 ? "(UPD)" : "",
        data[1] & 0x10 ? "ON" : "OFF",  // loudness
        (sint8_t)(0x3F) - (data[7] & 0x7F),  // fader
        data[7] & 0x80 ? "(UPD)" : "",
        (sint8_t)(0x3F) - (data[6] & 0x7F),  // balance
        data[6] & 0x80 ? "(UPD)" : "",
        data[1] & 0x04 ? "ON" : "OFF"  // auto_volume
    );

    return VAN_PACKET_PARSE_OK;
} // ParseAudioSettingsPkt

VanPacketParseResult_t ParseMfdStatusPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#5E4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#5E4

    const uint8_t* data = pkt.Data();
    uint16_t mfdStatus = (uint16_t)data[0] << 8 | data[1];
    char buffer[7];
    sprintf_P(buffer, PSTR("0x%04X"), mfdStatus);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "mfd_status": "%s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,

        // hmmm... MFD can also be ON if this is reported; this happens e.g. in the "minimal VAN network" test
        // setup with only the head unit (radio) and MFD. Maybe this is a status report: the MFD shows if has
        // received any packets that show connectivity to e.g. the BSI?
        data[0] == 0x00 && data[1] == 0xFF ? "SCREEN_OFF" :

        data[0] == 0x20 && data[1] == 0xFF ? "SCREEN_ON" :
        buffer
    );

    return VAN_PACKET_PARSE_OK;
} // ParseMfdStatusPkt

VanPacketParseResult_t ParseAirCon1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#464
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#464
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner1Structs.h

    const uint8_t* data = pkt.Data();

    // data[0] - bits:
    // - & 0x01: Rear window heater ON
    // - & 0x04: Air recirculation ON
    // - & 0x10: A/C icon?
    // - & 0x20: A/C icon? "YES" : "NO",  // TODO - what is this? This bit is always 0
    // - & 0x40: mode ? "AUTO" : "MANUAL",  // TODO - what is this? This bit is always 0
    // - & 0x0A: demist ? "ON" : "OFF",  // TODO - what is this? These bits are always 0

    // data[4] - reported fan speed
    //
    // Real-world tests: reported fan_speed values with various settings of the fan speed icon (0 = fan icon
    // not visible at all ... 7 = all four blades visible), under varying conditions:
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

    bool rear_heater = data[0] & 0x01;
    bool ac_icon = data[0] & 0x10;
    uint8_t setFanSpeed = data[4];
    if (rear_heater) setFanSpeed -= 12;
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

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "ac_icon": "%s",
            "recirc": "%s",
            "rear_heater_1": "%s",
            "reported_fan_speed": "%u",
            "set_fan_speed": "%u"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,
        ac_icon ? "ON" : "OFF",
        data[0] & 0x04 ? "ON" : "OFF",
        rear_heater ? "YES" : "NO",
        data[4],
        setFanSpeed
    );

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon1Pkt

VanPacketParseResult_t ParseAirCon2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4DC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4DC
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanAirConditioner2Structs.h

    const uint8_t* data = pkt.Data();
    int dataLen = pkt.DataLen();

    // Evaporator temperature is contantly toggling between 2 values, while the rest of the data is the same.
    // So process only if not duplicate of previous 2 packets.
    static uint8_t packetData[2][VAN_MAX_DATA_BYTES];  // Previous packet data

    if (memcmp(data, packetData[0], dataLen) == 0) return VAN_PACKET_DUPLICATE;
    if (memcmp(data, packetData[1], dataLen) == 0) return VAN_PACKET_DUPLICATE;

    memcpy(packetData[0], packetData[1], dataLen);
    memcpy(packetData[1], data, dataLen);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "contact_key_on": "%s",
            "ac_enabled": "%s",
            "rear_heater_2": "%s",
            "ac_compressor": "%s",
            "contact_key_position": "%s",
            "condenser_temperature": "%s",
            "evaporator_temperature": "%s"
        }
    })===";

    char floatBuf[2][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
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

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon2Pkt

VanPacketParseResult_t ParseCdChangerPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4EC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4EC
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanCdChangerStructs.h

    int dataLen = pkt.DataLen();
    if (dataLen == 0) return VAN_PACKET_PARSE_OK; // "Request" packet; nothing to show
    if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "cdc_random": "%s",
            "cdc_state": "%s",
            "cdc_cartridge": "%s",
            "cdc_track_time_min": "%s",
            "cdc_track_time_sec": "%s",
            "cdc_current_track": "%u",
            "cdc_total_tracks": "%u",
            "cdc_current_cd": "%u",
            "cdc_disc_1_present": "%s",
            "cdc_disc_2_present": "%s",
            "cdc_disc_3_present": "%s",
            "cdc_disc_4_present": "%s",
            "cdc_disc_5_present": "%s",
            "cdc_disc_6_present": "%s"
        }
    })===";

    char floatBuf[3][MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
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

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerPkt

VanPacketParseResult_t ParseSatnavStatus1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#54E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#54E

    const uint8_t* data = pkt.Data();
    uint16_t status = (uint16_t)data[1] << 8 | data[2];

    char buffer[10];
    sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data[1], data[2]);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "satnav_status_1": "%s%s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,

        // TODO - check; total guess
        status == 0x0000 ? "NOT_OPERATING" :
        status == 0x0001 ? "0x0001" :
        status == 0x0020 ? "0x0020" : // Nearly at destination ??
        status == 0x0080 ? "READY" :
        status == 0x0101 ? "0x0101" :
        status == 0x0200 ? "READING_DISC_1" :
        status == 0x0220 ? "0x0220" :
        status == 0x0300 ? "IN_GUIDANCE_MODE_1" :
        status == 0x0301 ? "IN_GUIDANCE_MODE_2" :
        status == 0x0320 ? "STOPPING_GUIDANCE" :
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

    return VAN_PACKET_PARSE_OK;
} // ParseSatnavStatus1Pkt

VanPacketParseResult_t ParseSatnavStatus2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#7CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#7CE

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "satnav_status_2": "%s",
            "satnav_disc": "%s",
            "satnav_gps_fix": "%s",
            "satnav_gps_fix_lost": "%s",
            "satnav_gps_scanning": "%s",
            "satnav_gps_speed": "%u km/h%s"
    )===";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[1] == 0x11 ? "STOPPING_GUIDANCE" :
        data[1] == 0x15 ? "IN_GUIDANCE_MODE" :
        data[1] == 0x20 ? "IDLE_NOT_READY" :
        data[1] == 0x21 ? "IDLE_READY" :
        data[1] == 0x25 ? "CALCULATING_ROUTE" :
        data[1] == 0x41 ? "???" :
        data[1] == 0xC1 ? "FINISHED_DOWNLOADING" :
        "??",

        (data[2] & 0x70) == 0x70 ? "NONE_PRESENT" :
        (data[2] & 0x70) == 0x30 ? "RECOGNIZED" :
        "??",

        data[2] & 0x01 ? "YES" : "NO",
        data[2] & 0x02 ? "YES" : "NO",
        data[2] & 0x04 ? "YES" : "NO",

        // 0xE0 as boundary for "reverse": just guessing. Do we ever drive faster than 224 km/h?
        data[16] < 0xE0 ? data[16] : 0xFF - data[16] + 1,

        data[16] >= 0xE0 ? " (reverse)" : ""
    );

    // TODO - what is this?
    uint16_t zzz = (uint16_t)data[9] << 8 | data[10];
    if (zzz != 0x00)
    {
        at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_zzz\": \"%u\""), zzz);
    } // if

    if (data[17] != 0x00)
    {
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_disc_status\": \"%s%s%s%s%s%s%s\""),

                data[17] & 0x01 ? "LOADING_AUDIO_FRAGMENT " : "",
                data[17] & 0x02 ? "AUDIO_OUTPUT " : "",
                data[17] & 0x04 ? "NEW_GUIDANCE_INSTRUCTION " : "",
                data[17] & 0x08 ? "READING_DISC " : "",
                data[17] & 0x10 ? "CALCULATING_ROUTE " : "",
                data[17] & 0x20 ? "DISC_PRESENT " : "",
                data[17] & 0x80 ? "REACHED_DESTINATION " : ""
            );
    } // if

    at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));

    return VAN_PACKET_PARSE_OK;
} // ParseSatnavStatus2Pkt

VanPacketParseResult_t ParseSatnavStatus3Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8CE

    int dataLen = pkt.DataLen();
    if (dataLen != 2 && dataLen != 3 && dataLen != 17) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    if (dataLen == 2)
    {
        uint16_t status = (uint16_t)data[0] << 8 | data[1];

        char buffer[7];
        sprintf_P(buffer, PSTR("0x%04X"), status);

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "satnav_status_3": "%s"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,

            // TODO - check; total guess
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

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data": {
                "satnav_system_id": [
        )===";

        int at = snprintf_P(buf, n, jsonFormatter);

        char txt[VAN_MAX_DATA_BYTES - 1 + 1];  // Max 28 data bytes, minus header (1), plus terminating '\0'

        bool first = true;
        int at2 = 1;
        while (at2 < dataLen)
        {
            strncpy(txt, (const char*) data + at2, dataLen - at2);
            txt[dataLen - at2] = 0;
            at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("%s\n\"%s\""), first ? "" : ",", txt);
            at2 += strlen(txt) + 1;
            first = false;
        } // while

        at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]\n}\n}"));
    } // if

    return VAN_PACKET_PARSE_OK;
} // ParseSatnavStatus3Pkt

VanPacketParseResult_t ParseSatnavGuidanceDataPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9CE

    const uint8_t* data = pkt.Data();

    uint16_t currHeading = (uint16_t)data[1] << 8 | data[2];
    uint16_t headingToDestination = (uint16_t)data[3] << 8 | data[4];
    uint16_t distanceToDestination = (uint16_t)(data[5] & 0x7F) << 8 | data[6];
    uint16_t gpsDistanceToDestination = (uint16_t)(data[7] & 0x7F) << 8 | data[8];
    uint16_t distanceToNextTurn = (uint16_t)(data[9] & 0x7F) << 8 | data[10];
    uint16_t headingOnRoundabout = (uint16_t)data[11] << 8 | data[12];
    uint16_t minutesToTravel = (uint16_t)data[13] << 8 | data[14];

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "satnav_curr_heading": "%u deg",
            "satnav_heading_to_dest": "%u deg",
            "satnav_distance_to_dest": "%u %s",
            "satnav_distance_to_dest_straight_line": "%u %s",
            "satnav_turn_at": "%u %s",
            "satnav_heading_on_roundabout": "%s deg",
            "satnav_minutes_to_travel": "%u"
        }
    })===";

    char floatBuf[MAX_FLOAT_SIZE];
    snprintf_P(buf, n, jsonFormatter,
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

    return VAN_PACKET_PARSE_OK;
} // ParseSatnavGuidanceDataPkt

VanPacketParseResult_t ParseSatnavGuidancePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#64E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#64E

    int dataLen = pkt.DataLen();
    if (dataLen != 3 && dataLen != 4 && dataLen != 6 && dataLen != 13 && dataLen != 23)
    {
        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
    } // if

    const uint8_t* data = pkt.Data();

    char buffer[20];
    sprintf_P(buffer, PSTR("unknown(0x%02X-0x%02X)"), data[1], data[2]);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "satnav_guidance_instruction": "%s"
    )===";

    int at = snprintf_P(buf, n, jsonFormatter,
        data[1] == 0x01 ? "SINGLE_TURN" :
        data[1] == 0x03 ? "DOUBLE_TURN" :
        data[1] == 0x04 ? "TURN_AROUND_IF_POSSIBLE" :
        data[1] == 0x05 ? "FOLLOW_ROAD" :
        data[1] == 0x06 ? "NOT_ON_MAP" :
        buffer
    );

    if (data[1] == 0x01)  // Single turn
    {
        if (data[2] == 0x00 || data[2] == 0x01)
        {
            if (dataLen != 13) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // One "detailed instruction" in data[4...11]
            at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"current_instruction\": "));
            PrintGuidanceInstruction(data + 4); // TODO
        }
        else if (data[2] == 0x02)
        {
            if (dataLen != 6) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // Fork or exit instruction
            at += at >= JSON_BUFFER_SIZE ? 0 :
                snprintf_P(buf + at, n - at, PSTR(",\n\"current_instruction\": \"%s\""),
                    data[4] == 0x41 ? "KEEP_LEFT_ON_FORK" :
                    data[4] == 0x14 ? "KEEP_RIGHT_ON_FORK" :
                    data[4] == 0x12 ? "TAKE_RIGHT_EXIT" :
                    "??"
                );
        }
        else
        {
            at += at >= JSON_BUFFER_SIZE ? 0 :
                snprintf_P(buf + at, n - at, PSTR(",\n\"current_instruction\": \"%s\""), buffer);
        } // if
    }
    else if (data[1] == 0x03)  // Double turn
    {
        if (dataLen != 23) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Two "detailed instructions": current in data[6...13], next in data[14...21]
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"current_instruction\": "));
        PrintGuidanceInstruction(data + 6); // TODO
        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"next_instruction\": "));
        PrintGuidanceInstruction(data + 14); // TODO
    }
    else if (data[1] == 0x04)  // Turn around if possible
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"current_instruction\": \"TURN_AROUND_IF_POSSIBLE\""));
    } // if
    else if (data[1] == 0x05)  // Follow road
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"follow_road_next_instruction\": \"%s\""),
                data[2] == 0x00 ? "NONE" :
                data[2] == 0x01 ? "TURN_RIGHT" :
                data[2] == 0x02 ? "TURN_LEFT" :
                data[2] == 0x04 ? "ROUNDABOUT" :
                data[2] == 0x08 ? "GO_STRAIGHT_AHEAD" :
                data[2] == 0x10 ? "RETRIEVING_NEXT_INSTRUCTION" :
                "??"
            );
    }
    else if (data[1] == 0x06)  // Not on map
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        at += at >= JSON_BUFFER_SIZE ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"not_on_map_follow_heading\": \"%u\""), data[2]);
    } // if

    n -= n <= 0 ? 0 : snprintf_P(buf, n, PSTR("\n}\n}"));

    return VAN_PACKET_PARSE_OK;
} // ParseSatnavGuidancePkt

VanPacketParseResult_t ParseSatnavReportPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#6CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#6CE

    int dataLen = pkt.DataLen();
    if (dataLen < 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    #define MAX_SATNAV_STRING_SIZE 128
    static char buffer[MAX_SATNAV_STRING_SIZE];
    static int offsetInBuffer = 0;

    const uint8_t* data = pkt.Data();
    int offsetInPacket = 1;

    if ((data[0] & 0x7F) <= 7)
    {
        // First packet of report sequence
        offsetInPacket = 2;
        offsetInBuffer = 0;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "satnav_report": "%s"
        )===";

        n -= snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(data[1]));
    }
    else
    {
        // TODO - check if data[0] & 0x7F has incremented by either 1 or 9 w.r.t. the last received packet.
        // If it has incremented by e.g. 10 (9+1) or 18 (9+9), then we have obviously missed a packet, so
        // appending the text of the current packet to that of the previous packet would be incorrect.

        // TODO

        Serial.print("\n    ");
    } // if

    while (offsetInPacket < dataLen - 1)
    {
        // TODO

        // New record?
        if (data[offsetInPacket] == 0x01)
        {
            offsetInPacket++;
            offsetInBuffer = 0;
            Serial.print("\n    ");
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
            Serial.printf("'%s' - ", buffer);
            offsetInBuffer = 0;
        }
        else
        {
            offsetInBuffer = strlen(buffer);
        } // if
    } // while

    // Last packet in report sequence?
    if (data[0] & 0x80) Serial.print("--LAST--");
    Serial.println();


    return VAN_PACKET_PARSE_OK;
} // ParseSatnavReportPkt

VanPacketParseResult_t ParseMfdToSatnavPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#94E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#94E

    int dataLen = pkt.DataLen();
    if (dataLen != 4 && dataLen != 9 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    // TODO



    return VAN_PACKET_PARSE_OK;
} // ParseMfdToSatnavPkt

VanPacketParseResult_t ParseSatnavToMfdPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#74E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#74E

    // TODO


    return VAN_PACKET_PARSE_OK;
} // ParseSatnavToMfdPkt

VanPacketParseResult_t ParseWheelSpeedPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#744

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "wheel_speed_rear_right": "%s",
            "wheel_speed_rear_left": "%s",
            "wheel_pulses_rear_right": "%u",
            "wheel_pulses_rear_left": "%u"
        }
    })===";

    char floatBuf[2][MAX_FLOAT_SIZE];

    snprintf_P(buf, n, jsonFormatter,
        FloatToStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 100.0, 2),
        FloatToStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2),
        (uint16_t)data[4] << 8 | data[5],
        (uint16_t)data[6] << 8 | data[7]
    );

    return VAN_PACKET_PARSE_OK;
} // ParseWheelSpeedPkt

VanPacketParseResult_t ParseOdometerPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8FC

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "odometer_2": "%lu"
        }
    })===";

    char floatBuf[MAX_FLOAT_SIZE];

    snprintf_P(buf, n, jsonFormatter,
        FloatToStr(floatBuf, ((uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3]) / 10.0, 1)
    );

    return VAN_PACKET_PARSE_OK;
} // ParseOdometerPkt

VanPacketParseResult_t ParseCom2000Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#450

    const uint8_t* data = pkt.Data();

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "com2000_light_switch": "%s%s%s%s%s%s%s%s",
            "com2000_right_stalk": "%s%s%s%s%s%s%s%s",
            "com2000_turn_signal": "%s%s",
            "com2000_head_unit_stalk": "%s%s%s%s%s",
            "com2000_head_unit_stalk_wheel_pos": "%d"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,

        data[1] & 0x01 ? "Auto light button pressed, " : "",
        data[1] & 0x02 ? "Fog light switch turned FORWARD, " : "",
        data[1] & 0x04 ? "Fog light switch turned BACKWARD, " : "",
        data[1] & 0x08 ? "Main beam handle gently ON, " : "",
        data[1] & 0x10 ? "Main beam handle fully ON, " : "",
        data[1] & 0x20 ? "All OFF, " : "",
        data[1] & 0x40 ? "Sidelights ON, " : "",
        data[1] & 0x80 ? "Low beam ON, " : "",

        data[2] & 0x01 ? "Trip computer button pressed, " : "",
        data[2] & 0x02 ? "Rear wiper switched turned to screen wash position, " : "",
        data[2] & 0x04 ? "Rear wiper switched turned to position 1, " : "",
        data[2] & 0x08 ? "Screen wash, " : "",
        data[2] & 0x10 ? "Single screen wipe, " : "",
        data[2] & 0x20 ? "Screen wipe speed 1, " : "",
        data[2] & 0x40 ? "Screen wipe speed 2, " : "",
        data[2] & 0x80 ? "Screen wipe speed 3, " : "",

        data[3] & 0x40 ? "Left signal ON, " : "",
        data[3] & 0x80 ? "Right signal ON, " : "",

        data[5] & 0x02 ? "SRC button pressed, " : "",
        data[5] & 0x03 ? "Volume down button pressed, " : "",
        data[5] & 0x08 ? "Volume up button pressed, " : "",
        data[5] & 0x40 ? "Seek backward button pressed, " : "",
        data[5] & 0x80 ? "Seek forward button pressed, " : "",

        (sint8_t)data[6]
    );

    return VAN_PACKET_PARSE_OK;
} // ParseCom2000Pkt

VanPacketParseResult_t ParseCdChangerCmdPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8EC

    const uint8_t* data = pkt.Data();
    uint16_t cdcCommand = (uint16_t)data[0] << 8 | data[1];

    char buffer[7];
    sprintf_P(buffer, PSTR("0x%04X"), cdcCommand);

    static char jsonFormatter[] PROGMEM = R"===(
    {
        "event": "display",
        "data":
        {
            "cd_changer_command": "%s"
        }
    })===";

    snprintf_P(buf, n, jsonFormatter,

        cdcCommand == 0x1101 ? "POWER_OFF" :
        cdcCommand == 0x2101 ? "POWER_OFF" :
        cdcCommand == 0x1181 ? "PAUSE" :
        cdcCommand == 0x1183 ? "PLAY" :
        cdcCommand == 0x31FE ? "PREVIOUS_TRACK" :
        cdcCommand == 0x31FF ? "NEXT_TRACK" :
        cdcCommand == 0x4101 ? "CD_1" :
        cdcCommand == 0x4102 ? "CD_2" :
        cdcCommand == 0x4103 ? "CD_3" :
        cdcCommand == 0x4104 ? "CD_4" :
        cdcCommand == 0x4105 ? "CD_5" :
        cdcCommand == 0x4106 ? "CD_6" :
        cdcCommand == 0x41FE ? "PREVIOUS_CD" :
        cdcCommand == 0x41FF ? "NEXT_CD" :
        buffer
    );

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerCmdPkt

VanPacketParseResult_t ParseMfdToHeadUnitPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8D4

    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();

    char buffer[5];
    sprintf_P(buffer, PSTR("0x%02X"), data[1]);

    if (data[0] == 0x11)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_update_audio_bits_mute": "%s",
                "head_unit_update_audio_bits_auto_volume": "%s",
                "head_unit_update_audio_bits_loudness": "%s",
                "head_unit_update_audio_bits_audio_menu": "%s",
                "head_unit_update_audio_bits_power": "%s",
                "head_unit_update_audio_bits_contact_key": "%s"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,
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
        if (dataLen != 2 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_update_switch_to": "%s"
        )===";

        int at = snprintf_P(buf, n, jsonFormatter,
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
            static char jsonFormatter2[] PROGMEM = R"===(,
                "head_unit_update_power": "%s",
                "head_unit_update_source": "%s",
                "head_unit_update_volume_1": "%u%s",
                "head_unit_update_balance": "%d%s",
                "head_unit_update_fader": "%d%s",
                "head_unit_update_bass": "%d%s",
                "head_unit_update_treble": "%d%s"
            )===";

            at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, jsonFormatter2,

                data[2] & 0x01 ? "ON" : "OFF",

                (data[4] & 0x0F) == 0x00 ? "NONE" :  // source
                (data[4] & 0x0F) == 0x01 ? "TUNER" :
                (data[4] & 0x0F) == 0x02 ? "INTERNAL_CD_OR_TAPE" :
                (data[4] & 0x0F) == 0x03 ? "CD_CHANGER" :

                // This is the "default" mode for the head unit, to sit there and listen to the navigation
                // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                // whenever this source is chosen.
                (data[4] & 0x0F) == 0x05 ? "NAVIGATION_AUDIO" :

                "???",

                data[5] & 0x7F,
                data[5] & 0x80 ? "(UPD)" : "",
                (sint8_t)(0x3F) - (data[6] & 0x7F),
                data[6] & 0x80 ? "(UPD)" : "",
                (sint8_t)(0x3F) - (data[7] & 0x7F),
                data[7] & 0x80 ? "(UPD)" : "",
                (sint8_t)(data[8] & 0x7F) - 0x3F,
                data[8] & 0x80 ? "(UPD)" : "",
                (sint8_t)(data[9] & 0x7F) - 0x3F,
                data[9] & 0x80 ? "(UPD)" : ""
            );

        } // if

        at += at >= JSON_BUFFER_SIZE ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}"));
    }
    else if (data[0] == 0x13)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_update_volume_2": "%u(%s%s)"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,
            data[1] & 0x1F,
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

        if (dataLen != 5) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // TODO - bit 7 of data[1] is always 1 ?

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_update_audio_levels_balance": "%d",
                "head_unit_update_audio_levels_fader": "%d",
                "head_unit_update_audio_levels_bass": "%d",
                "head_unit_update_audio_levels_treble": "%d"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,
            (sint8_t)(0x3F) - (data[1] & 0x7F),
            (sint8_t)(0x3F) - data[2],
            (sint8_t)data[3] - 0x3F,
            (sint8_t)data[4] - 0x3F
        );
    }
    else if (data[0] == 0x27)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_preset_request_band": "%s",
                "head_unit_preset_request_memory": "%u"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,
            TunerBandStr(data[1] >> 4 & 0x07),
            data[1] & 0x0F
        );
    }
    else if (data[0] == 0x61)
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_cd_request": "%s"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter,
            data[1] == 0x02 ? "PAUSE" :
            data[1] == 0x03 ? "PLAY" :
            data[3] == 0xFF ? "NEXT" :
            data[3] == 0xFE ? "PREVIOUS" :
            "??"
        );
    }
    else if (data[0] == 0xD1)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_tuner_info_request": "REQUEST"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD2)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_tape_info_request": "REQUEST"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD6)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        static char jsonFormatter[] PROGMEM = R"===(
        {
            "event": "display",
            "data":
            {
                "head_unit_cd_track_info_request": "REQUEST"
            }
        })===";

        snprintf_P(buf, n, jsonFormatter);
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    return VAN_PACKET_PARSE_OK;
} // ParseMfdToHeadUnitPkt

// Print the new packet on Serial, highlighting the bytes that differ
void PrintPacketDataDiff(TVanPacketRxDesc& pkt, IdenHandler_t* handler)
{
    uint16_t iden = pkt.Iden();
    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();

    // First print only the differing bytes from the previous packet
    Serial.printf("%03X (%s) ", iden, handler->idenStr);
    for (int i = 0; i < dataLen; i++)
    {
        char diffByte[3] = "  ";
        if (data[i] != handler->prevData[i])
        {
            snprintf_P(diffByte, sizeof(diffByte), PSTR("%02X"), handler->prevData[i]);
        } // if
        Serial.printf("%s%c", diffByte, i < dataLen - 1 ? '-' : '\n');
    } // for

    // Save packet data to compare against at next packet reception
    memset(handler->prevData, '\0', VAN_MAX_DATA_BYTES);
    memcpy(handler->prevData, data, dataLen);

    // Now print the new packet's data
    Serial.printf("%03X (%s) ", iden, handler->idenStr);
    for (int i = 0; i < dataLen; i++) Serial.printf("%02X%c", data[i], i < dataLen - 1 ? '-' : '\n');
} // PrintPacketDataDiff

static IdenHandler_t handlers[] = 
{
    // Columns:
    // IDEN value, IDEN string, number of expected bytes (or -1 if varying/unknown), handler function
    { 0xE24, "vin", 17, &ParseVinPkt },
    { 0x8A4, "engine", 7, &ParseEnginePkt },
    { 0x9C4, "head_unit_stalk", 2, &ParseHeadUnitStalkPkt },
    { 0x4FC, "lights_status", -1, &ParseLightsStatusPkt },
    { 0x8C4, "device_report", -1, &ParseDeviceReportPkt },
    { 0x564, "car_status_1", 27, &ParseCarStatus1Pkt },
    { 0x524, "car_status_2", -1, &ParseCarStatus2Pkt },
    { 0x824, "dashboard", 7, &ParseDashboardPkt },
    { 0x664, "dashboard_buttons", -1, &ParseDashboardButtonsPkt },
    { 0x554, "head_unit", -1, &ParseHeadUnitPkt },
    { 0x984, "time", 5, &ParseTimePkt },
    { 0x4D4, "audio_settings", 11, &ParseAudioSettingsPkt },
    { 0x5E4, "mfd_status", 2, &ParseMfdStatusPkt },
    { 0x464, "aircon_1", 5, &ParseAirCon1Pkt },
    { 0x4DC, "aircon_2", 7, &ParseAirCon2Pkt },
    { 0x4EC, "cd_changer", -1, &ParseCdChangerPkt },
    { 0x54E, "satnav_status_1", 6, &ParseSatnavStatus1Pkt },
    { 0x7CE, "satnav_status_2", 20, &ParseSatnavStatus2Pkt },
    { 0x8CE, "satnav_status_3", -1, &ParseSatnavStatus3Pkt },
    { 0x9CE, "satnav_guidance_data", 16, &ParseSatnavGuidanceDataPkt },
    { 0x64E, "satnav_guidance", -1, &ParseSatnavGuidancePkt },
    { 0x6CE, "satnav_report", -1, &ParseSatnavReportPkt },
    { 0x94E, "mfd_to_satnav", -1, &ParseMfdToSatnavPkt },
    { 0x74E, "satnav_to_mfd", 27, &ParseSatnavToMfdPkt },
    { 0x744, "wheel_speed", 5, &ParseWheelSpeedPkt },
    { 0x8FC, "odometer", 5, &ParseOdometerPkt },
    { 0x450, "com2000", 10, &ParseCom2000Pkt },
    { 0x8EC, "cd_changer_command", 2, &ParseCdChangerCmdPkt },
    { 0x8D4, "display_to_head_unit", -1, &ParseMfdToHeadUnitPkt },
#if 0
    { 0xADC, "aircon_diag", -1, &DefaultPacketParser },
    { 0xA5C, "aircon_diag_command", -1, &DefaultPacketParser },
#endif
}; // handlers

const IdenHandler_t* const handlers_end = handlers + sizeof(handlers) / sizeof(handlers[0]);

const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt)
{
    if (! pkt.CheckCrcAndRepair()) return ""; // CRC error

    int dataLen = pkt.DataLen();
    if (dataLen < 0 || dataLen > VAN_MAX_DATA_BYTES) return ""; // Unexpected packet length

    uint16_t iden = pkt.Iden();
    IdenHandler_t* handler = handlers;

    // Search the correct handler. Relying on short-circuit boolean evaluation.
    while (handler != handlers_end && handler->iden != iden) handler++;

    // Hander found?
    if (handler != handlers_end)
    {
        if (handler->dataLen >= 0 && dataLen != handler->dataLen) return ""; // Unexpected packet length

        // Only process if packet content differs from previous packet
        // TODO - specify handling modes for duplicate packets in handlers table: not all duplicate packets should
        //   be ignored.

        const uint8_t* data = pkt.Data();

        // Assumption is that 'handler->prevData' is initialized to all-zeroes; see also:
        // https://stackoverflow.com/questions/2589749/how-to-initialize-array-to-0-in-c which says: 
        //
        // "Global variables and static variables are automatically initialized to zero. If you have simply
        // char ZEROARRAY[1024];
        // at global scope it will be all zeros at runtime."
        //
        if (memcmp(data, handler->prevData, dataLen) == 0) return "";  // Duplicate packet

        // Print the new packet on Serial, highlighting the bytes that differ
        PrintPacketDataDiff(pkt, handler);

        int result = handler->parser(handler->idenStr, pkt, jsonBuffer, JSON_BUFFER_SIZE);
        if (result == VAN_PACKET_PARSE_OK)
        {
            Serial.printf_P(PSTR("Parsed: %s (0x%03X)"), handler->idenStr, iden);
            #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
            Serial.print(F(": "));
            Serial.println(jsonBuffer);
            #endif // PRINT_JSON_BUFFERS_ON_SERIAL
        } // if
        return jsonBuffer;
    } // if

    return ""; // Unrecognized IDEN value
} // ParseVanPacketToJson
