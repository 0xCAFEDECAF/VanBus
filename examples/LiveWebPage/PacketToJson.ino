/*
 * VanBus packet parser - Shrink-wraps the data of recognized packets neatly into a JSON format
 *
 * Written by Erik Tromp
 *
 * MIT license, all text above must be included in any redistribution.
 */

// TODO - reduce size of large JSON packets like the ones containing guidance instruction icons
#define JSON_BUFFER_SIZE 4096
char jsonBuffer[JSON_BUFFER_SIZE];

enum VanPacketParseResult_t
{
    VAN_PACKET_NO_CONTENT  = 2, // Packet is OK but contains no useful (new) content
    VAN_PACKET_DUPLICATE  = 1, // Packet was the same as the last with this IDEN field
    VAN_PACKET_PARSE_OK = 0,  // Packet was parsed OK
    VAN_PACKET_PARSE_CRC_ERROR = -1,  // Packet had a CRC error
    VAN_PACKET_PARSE_UNEXPECTED_LENGTH = -2,  // Packet had unexpected length
    VAN_PACKET_PARSE_UNRECOGNIZED_IDEN = -3,  // Packet had unrecognized IDEN field
    VAN_PACKET_PARSE_TO_BE_DECODED = -4,  // IDEN recognized but the correct parsing of this packet is not yet known
    VAN_PACKET_PARSE_JSON_TOO_LONG = -5,  // IDEN recognized but the parsing into JSON overflows 'jsonBuffer'
    VAN_PACKET_PARSE_FRAGMENT_MISSED = -6  // Missed at least on fragment of a multi-fragment message
}; // enum VanPacketParseResult_t

typedef VanPacketParseResult_t (*TPacketParser)(const char*, TVanPacketRxDesc&, char*, const int);

struct IdenHandler_t
{
    uint16_t iden;
    char* idenStr;
    int dataLen;
    TPacketParser parser;
    uint8_t* prevData;
}; // struct IdenHandler_t

// Often used string constants
const char PROGMEM emptyStr[] = "";
const char PROGMEM commaStr[] = ",";
const char PROGMEM onStr[] = "ON";
const char PROGMEM offStr[] = "OFF";
const char PROGMEM yesStr[] = "YES";
const char PROGMEM noStr[] = "NO";
const char PROGMEM openStr[] = "OPEN";
const char PROGMEM closedStr[] = "CLOSED";
const char PROGMEM presentStr[] = "PRESENT";
const char PROGMEM notPresentStr[] = "NOT_PRESENT";
const char PROGMEM styleDisplayBlockStr[] = "block";  // To set 'style="display:block;"' in HTML
const char PROGMEM styleDisplayNoneStr[] = "none";  // To set 'style="display:none;"' in HTML
const char PROGMEM noneStr[] = "NONE";
const char PROGMEM updatedStr[] = "(UPD)";
const char PROGMEM notApplicable2Str[] = "--";
const char PROGMEM notApplicable3Str[] = "---";

// Defined in PacketFilter.ino
bool IsPacketSelected(uint16_t iden, VanPacketFilter_t filter);

// Uses statically allocated buffer, so don't call twice within the same printf invocation 
char* ToStr(uint8_t data)
{
    #define MAX_UINT8_STR_SIZE 4
    static char buffer[MAX_UINT8_STR_SIZE];
    sprintf_P(buffer, PSTR("%u"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation 
char* ToStr(uint16_t data)
{
    #define MAX_UINT16_STR_SIZE 6
    static char buffer[MAX_UINT16_STR_SIZE];
    sprintf_P(buffer, PSTR("%u"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation 
char* ToStr(sint16_t data)
{
    #define MAX_SINT16_STR_SIZE 7
    static char buffer[MAX_SINT16_STR_SIZE];
    sprintf_P(buffer, PSTR("%d"), data);

    return buffer;
} // ToStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation 
char* ToBcdStr(uint8_t data)
{
    #define MAX_UINT8_BCD_STR_SIZE 3
    static char buffer[MAX_UINT8_BCD_STR_SIZE];
    sprintf_P(buffer, PSTR("%X"), data);

    return buffer;
} // ToBcdStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint8_t data)
{
    #define MAX_UINT8_HEX_STR_SIZE 5
    static char buffer[MAX_UINT8_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%02X"), data);

    return buffer;
} // ToHexStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint8_t data1, uint8_t data2)
{
    #define MAX_2_UINT8_HEX_STR_SIZE 10
    static char buffer[MAX_2_UINT8_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%02X-0x%02X"), data1, data2);

    return buffer;
} // ToHexStr

// Uses statically allocated buffer, so don't call twice within the same printf invocation
char* ToHexStr(uint16_t data)
{
    #define MAX_UINT16_HEX_STR_SIZE 7
    static char buffer[MAX_UINT16_HEX_STR_SIZE];
    sprintf_P(buffer, PSTR("0x%04X"), data);

    return buffer;
} // ToHexStr

// Replace special (e.g. extended Ascii) characters by their HTML-safe representation.
// See also: https://www.ascii-code.com/ .
void AsciiToHtml(String& in)
{
    for (int i = 0; i < in.length(); i++)
    {
        char c = in.c_str()[i];

        // Vast majority of characters is printable, so this if-statement is entered seldomly
        if (c > 127)
        {
            // Hope this is somewhat efficient...
            String from(c);
            String to((int)c, DEC);
            to = "&#" + to + ";";
            in.replace(from, to);  // in.length() will become larger; no problem
            i += 5;  // We can skip the just added characters
        } // if
    } // for
} // ToHtml

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

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* TunerBandStr(uint8_t data)
{
    return
        data == TB_NONE ? noneStr :
        data == TB_FM1 ? PSTR("FM1") :
        data == TB_FM2 ? PSTR("FM2") :
        data == TB_FM3 ? PSTR("FM3") :  // Never seen, just guessing
        data == TB_FMAST ? PSTR("FMAST") :
        data == TB_AM ? PSTR("AM") :
        data == TB_PTY_SELECT ? PSTR("PTY_SELECT") :  // When selecting PTY to search for
        ToHexStr(data);
} // TunerBandStr

// Tuner search mode
// Bits:
//  2  1  0
// ---+--+---
//  0  0  0 : Not searching
//  0  0  1 : Manual tuning
//  0  1  0 : Searching by frequency
//  0  1  1 :
//  1  0  0 : Searching for station with matching PTY
//  1  0  1 :
//  1  1  0 :
//  1  1  1 : Auto-station search in the FMAST band (long-press "Radio Band" button)
enum TunerSearchMode_t
{
    TS_NOT_SEARCHING = 0,
    TS_MANUAL = 1,
    TS_BY_FREQUENCY = 2,
    TS_BY_MATCHING_PTY = 4,
    TS_FM_AST = 7
}; // enum TunerSearchMode_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* TunerSearchModeStr(uint8_t data)
{
    return
        data == TS_NOT_SEARCHING ? PSTR("NOT_SEARCHING") :
        data == TS_MANUAL ? PSTR("MANUAL_TUNING") :
        data == TS_BY_FREQUENCY ? PSTR("SEARCHING_BY_FREQUENCY") :
        data == TS_BY_MATCHING_PTY ? PSTR("SEARCHING_MATCHING_PTY") : // Searching for station with matching PTY
        data == TS_FM_AST ? PSTR("FM_AST_SCAN") : // AutoSTore scan in the FMAST band (long-press "Radio Band" button)
        ToHexStr(data);
} // TunerSearchModeStr

// "Full" PTY string
// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* PtyStrFull(uint8_t ptyCode)
{
    // See also:
    // https://www.electronics-notes.com/articles/audio-video/broadcast-audio/rds-radio-data-system-pty-codes.php
    return
        ptyCode ==  0 ? PSTR("Not defined") :
        ptyCode ==  1 ? PSTR("News") :
        ptyCode ==  2 ? PSTR("Current affairs") :
        ptyCode ==  3 ? PSTR("Information") :
        ptyCode ==  4 ? PSTR("Sport") :
        ptyCode ==  5 ? PSTR("Education") :
        ptyCode ==  6 ? PSTR("Drama") :
        ptyCode ==  7 ? PSTR("Culture") :
        ptyCode ==  8 ? PSTR("Science") :
        ptyCode ==  9 ? PSTR("Varied") :
        ptyCode == 10 ? PSTR("Pop Music") :
        ptyCode == 11 ? PSTR("Rock Music") :
        ptyCode == 12 ? PSTR("Easy Listening") :  // also: "Middle of the road music"
        ptyCode == 13 ? PSTR("Light Classical") :
        ptyCode == 14 ? PSTR("Serious Classical") :
        ptyCode == 15 ? PSTR("Other Music") :
        ptyCode == 16 ? PSTR("Weather") :
        ptyCode == 17 ? PSTR("Finance") :
        ptyCode == 18 ? PSTR("Children’s Programmes") :
        ptyCode == 19 ? PSTR("Social Affairs") :
        ptyCode == 20 ? PSTR("Religion") :
        ptyCode == 21 ? PSTR("Phone-in") :
        ptyCode == 22 ? PSTR("Travel") :
        ptyCode == 23 ? PSTR("Leisure") :
        ptyCode == 24 ? PSTR("Jazz Music") :
        ptyCode == 25 ? PSTR("Country Music") :
        ptyCode == 26 ? PSTR("National Music") :
        ptyCode == 27 ? PSTR("Oldies Music") :
        ptyCode == 28 ? PSTR("Folk Music") :
        ptyCode == 29 ? PSTR("Documentary") :
        ptyCode == 30 ? PSTR("Alarm Test") :
        ptyCode == 31 ? PSTR("Alarm") :
        notApplicable3Str;
} // PtyStrFull

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* RadioPiCountry(uint8_t countryCode)
{
    // See also:
    // - http://poupa.cz/rds/countrycodes.htm
    // - https://radio-tv-nederland.nl/rds/PI%20codes%20Europe.jpg
    // Note: more than one country is assigned to the same code, just listing the most likely.
    return
        countryCode == 0x01 || countryCode == 0x0D ? PSTR("DE") :  // Germany
        countryCode == 0x02 ? PSTR("IE") :  // Ireland
        countryCode == 0x03 ? PSTR("PL") :  // Poland
        countryCode == 0x04 ? PSTR("CH") :  // Switzerland
        countryCode == 0x05 ? PSTR("IT") :  // Italy
        countryCode == 0x06 ? PSTR("BEL") :  // Belgium
        countryCode == 0x07 ? PSTR("LUX") :  // Luxemburg
        countryCode == 0x08 ? PSTR("NL") :  // Netherlands
        countryCode == 0x09 ? PSTR("DNK") :  // Denmark
        countryCode == 0x0A ? PSTR("AUT") :  // Austria
        countryCode == 0x0B ? PSTR("HU") :  // Hungary
        countryCode == 0x0C ? PSTR("GB") :  // United Kingdom
        countryCode == 0x0E ? PSTR("ES") :  // Spain
        countryCode == 0x0F ? PSTR("FR") :  // France
        notApplicable2Str;
} // RadioPiCountry

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* RadioPiAreaCoverage(uint8_t coverageCode)
{
    // https://www.pira.cz/rds/show.asp?art=rds_encoder_support
    return
        coverageCode == 0x00 ? PSTR("local") :
        coverageCode == 0x01 ? PSTR("international") :
        coverageCode == 0x02 ? PSTR("national") :
        coverageCode == 0x03 ? PSTR("supra-regional") :
        PSTR("regional");
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
enum SatNavRequest_t
{
    SR_ENTER_COUNTRY = 0x00,  // Never seen, just guessing
    SR_ENTER_PROVINCE = 0x01,  // Never seen, just guessing
    SR_ENTER_CITY = 0x02,
    SR_ENTER_DISTRICT = 0x03,  // Never seen, just guessing
    SR_ENTER_NEIGHBORHOOD = 0x04,  // Never seen, just guessing
    SR_ENTER_STREET = 0x05,
    SR_ENTER_HOUSE_NUMBER = 0x06,  // Range of house numbers to choose from
    SR_ENTER_HOUSE_NUMBER_LETTER = 0x07,  // Never seen, just guessing
    SR_SERVICE_LIST = 0x08,
    SR_SERVICE_ADDRESS = 0x09,
    // TODO - SR_ARCHIVE_IN_PERSONAL_DIRECTORY = 0x0A ? In that case, SR_ARCHIVE_IN_PROFESSIONAL_DIRECTORY = 0x0B
    SR_ARCHIVE_IN_DIRECTORY = 0x0B,
    // TODO - SR_RENAME_PERSONAL_DIRECTORY_ENTRY = 0x0C ? In that case, SR_RENAME_PROFESSIONAL_DIRECTORY_ENTRY = 0x0D
    SR_RENAME_DIRECTORY_ENTRY = 0x0D,
    SR_LAST_DESTINATION = 0x0E,
    SR_NEXT_STREET = 0x0F,  // Shown during SatNav guidance in the (solid line) top box
    SR_CURRENT_STREET = 0x10,  // Shown during SatNav guidance in the (dashed line) bottom box
    SR_PERSONAL_ADDRESS = 0x11,
    SR_PROFESSIONAL_ADDRESS = 0x12,
    SR_SOFTWARE_MODULE_VERSIONS = 0x13,
    SR_PERSONAL_ADDRESS_LIST = 0x1B,
    SR_PROFESSIONAL_ADDRESS_LIST = 0x1C,
    SR_DESTINATION = 0x1D
}; // enum SatNavRequest_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* SatNavRequestStr(uint8_t data)
{
    return
        data == SR_ENTER_COUNTRY ? PSTR("Enter country") :
        data == SR_ENTER_PROVINCE ? PSTR("Enter province") :
        data == SR_ENTER_CITY ? PSTR("Enter city") :
        data == SR_ENTER_DISTRICT ? PSTR("Enter district") :
        data == SR_ENTER_NEIGHBORHOOD ? PSTR("Enter neighborhood") :
        data == SR_ENTER_STREET ? PSTR("Enter street") :
        data == SR_ENTER_HOUSE_NUMBER ? PSTR("Enter number") :
        data == SR_ENTER_HOUSE_NUMBER_LETTER ? PSTR("Enter letter") :
        data == SR_SERVICE_LIST ? PSTR("Service") :
        data == SR_SERVICE_ADDRESS ? PSTR("Service address") :
        data == SR_ARCHIVE_IN_DIRECTORY ? PSTR("Archive in directory") :
        data == SR_RENAME_DIRECTORY_ENTRY ? PSTR("Rename directory entry") :
        data == SR_LAST_DESTINATION ? PSTR("Last destination") :  // TODO - change to SR_CURR_DESTINATION - "Current destination"
        data == SR_NEXT_STREET ? PSTR("Next street") :
        data == SR_CURRENT_STREET ? PSTR("Current street") :
        data == SR_PERSONAL_ADDRESS ? PSTR("Personal address") :
        data == SR_PROFESSIONAL_ADDRESS ? PSTR("Professional address") :
        data == SR_SOFTWARE_MODULE_VERSIONS ? PSTR("Software module versions") :
        data == SR_PERSONAL_ADDRESS_LIST ? PSTR("Personal address list") :
        data == SR_PROFESSIONAL_ADDRESS_LIST ? PSTR("Professional address list") :
        data == SR_DESTINATION ? PSTR("Current destination") :  // TODO - change to SR_NEW_DESTINATION - "New destination"
        ToHexStr(data);
} // SatNavRequestStr

enum SatNavRequestType_t
{
    SRT_REQ_N_ITEMS = 0,  // Request for number of items
    SRT_REQ_ITEMS = 1,  // Request (single or list of) item(s)
    SRT_SELECT = 2,  // Select item
    SRT_SELECT_CITY_CENTER = 3  // Select city center
}; // enum SatNavRequestType_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* SatNavRequestTypeStr(uint8_t data)
{
    return
        data == SRT_REQ_N_ITEMS ? PSTR("REQ_N_ITEMS") :
        data == SRT_REQ_ITEMS ? PSTR("REQ_ITEMS") :
        data == SRT_SELECT ? PSTR("SELECT") :
        data == SRT_SELECT_CITY_CENTER ? PSTR("SELECT_CITY_CENTER") :
        ToStr(data);
} // SatNavRequestTypeStr

enum SatNavGuidancePreference_t
{
    SGP_FASTEST_ROUTE = 0x01,
    SGP_SHORTEST_DISTANCE = 0x04,
    SGP_AVOID_HIGHWAY = 0x12,
    SGP_COMPROMISE_FAST_SHORT = 0x02
}; // enum SatNavGuidancePreference_t

// Returns a PSTR (allocated in flash, saves RAM). In printf formatter use "%S" (capital S) instead of "%s".
const char* SatNavGuidancePreferenceStr(uint8_t data)
{
    return
        data == SGP_FASTEST_ROUTE ? PSTR("FASTEST_ROUTE") :
        data == SGP_SHORTEST_DISTANCE ? PSTR("SHORTEST_DISTANCE") :
        data == SGP_AVOID_HIGHWAY ? PSTR("AVOID_HIGHWAY") :
        data == SGP_COMPROMISE_FAST_SHORT ? PSTR("COMPROMISE_FAST_SHORT") :
        notApplicable3Str;
} // SatNavGuidancePreferenceStr

// Convert SatNav guidance instruction icon details to JSON
//
// A detailed SatNav guidance instruction consists of 8 bytes:
// * 0   : turn angle in increments of 22.5 degrees, measured clockwise, starting with 0 at 6 o-clock.
//         E.g.: 0x4 == 90 deg left, 0x8 = 180 deg = straight ahead, 0xC = 270 deg = 90 deg right.
// * 1   : always 0x00 ??
// * 2, 3: bit pattern indicating which legs are present in the junction or roundabout. Each bit set is for one leg.
//         Lowest bit of byte 3 corresponds to the leg of 0 degrees (straight down, which is
//         always there, because that is where we are currently driving), running clockwise up to the
//         highest bit of byte 2, which corresponds to a leg of 337.5 degrees (very sharp right).
// * 4, 5: bit pattern indicating which legs in the junction are "no entry". The coding of the bits is the same
//         as for bytes 2 and 3.
// * 6   : always 0x00 ??
// * 7   : always 0x00 ??
//
void GuidanceInstructionIconJson(const char* iconName, const uint8_t data[8], char* buf, int& at, const int n)
{
    // Show all the legs in the junction

    uint16_t legBits = (uint16_t)data[2] << 8 | data[3];
    for (int legBit = 1; legBit < 16; legBit++)
    {
        uint16_t degrees10 = legBit * 225;
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"%S_leg_%u_%u\": \"%S\""),
                iconName,
                degrees10 / 10,
                degrees10 % 10,
                legBits & (1 << legBit) ? onStr : offStr
            );
    } // for

    // Show all the "no-entry" legs in the junction

    uint16_t noEntryBits = (uint16_t)data[4] << 8 | data[5];
    for (int noEntryBit = 1; noEntryBit < 16; noEntryBit++)
    {
        uint16_t degrees10 = noEntryBit * 225;
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"%S_no_entry_%u_%u\": \"%S\""),
                iconName,
                degrees10 / 10,
                degrees10 % 10,
                noEntryBits & (1 << noEntryBit) ? onStr : offStr
            );
    } // for

    // Show the direction to go (indicated clock-wise, i.e. 90 degrees is left turn)

    uint16_t direction = (1800 + data[0] * 225) % 3600;

    const static char jsonFormatter[] PROGMEM =
        ",\n"
        "\"%S_direction_as_text\": \"%u.%u\",\n"  // degrees
        "\"%S_direction\":\n"
        "{\n"
            "\"style\":\n"
            "{\n"
                "\"transform\": \"rotate(%u.%udeg)\"\n"
            "}\n"
        "}";

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, jsonFormatter,
            iconName,
            direction / 10,
            direction % 10,
            iconName,
            direction / 10,
            direction % 10
        );
} // GuidanceInstructionIconJson

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
VanPacketParseResult_t DefaultPacketParser(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"%s\": \"%s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, idenStr, PacketRawToStr(pkt));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // DefaultPacketParser

#endif

VanPacketParseResult_t ParseVinPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#E24
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#E24

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"vin\": \"%-17.17s\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter, pkt.Data());

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseVinPkt

VanPacketParseResult_t ParseEnginePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8A4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8A4

    const uint8_t* data = pkt.Data();

    // Coolant water temperature value often falls back to 'invalid' (0xFF), even if a valid value was previously
    // found. Keep the last reported valid value.
    static uint8_t lastValidWaterTempRaw = 0xFF;
    uint8_t waterTempRaw = data[2];
    if (waterTempRaw != 0xFF) lastValidWaterTempRaw = waterTempRaw;  // Copy only if packet value is valid
    bool isWaterTempValid = lastValidWaterTempRaw != 0xFF;
    sint16_t waterTemp = (uint16_t)lastValidWaterTempRaw - 39;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"dash_light\": \"%S\",\n"
            "\"dash_actual_brightness\": \"%u\",\n"
            "\"contact_key_position\": \"%S\",\n"
            "\"engine_running\": \"%S\",\n"
            "\"economy_mode\": \"%S\",\n"
            "\"in_reverse\": \"%S\",\n"
            "\"trailer\": \"%S\",\n"
            "\"water_temp\": \"%S\",\n"
            "\"water_temp_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%S)\"\n"
                "}\n"
            "},\n"
            "\"odometer_1\": \"%s\",\n"
            "\"exterior_temperature\": \"%s &deg;C\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? PSTR("FULL") : PSTR("DIMMED (LIGHTS ON)"),
        data[0] & 0x0F,

        (data[1] & 0x03) == 0x00 ? offStr :
        (data[1] & 0x03) == 0x01 ? PSTR("ACC") :
        (data[1] & 0x03) == 0x03 ? onStr :
        (data[1] & 0x03) == 0x02 ? PSTR("START") :
        ToHexStr((uint8_t)(data[1] & 0x03)),

        data[1] & 0x04 ? yesStr : noStr,
        data[1] & 0x10 ? onStr : offStr,
        data[1] & 0x20 ? yesStr : noStr,
        data[1] & 0x40 ? presentStr : notPresentStr,
        isWaterTempValid ? ToStr(waterTemp) : notApplicable3Str,

        // TODO - hard coded value 130 degrees Celsius for 100%
        #define MAX_COOLANT_TEMP (130)
        ! isWaterTempValid || waterTemp < 0 ? PSTR("0") :
            waterTemp >= MAX_COOLANT_TEMP ? PSTR("1") :
                FloatToStr(floatBuf[0], (float)waterTemp / MAX_COOLANT_TEMP, 2),

        FloatToStr(floatBuf[1], ((uint32_t)data[3] << 16 | (uint32_t)data[4] << 8 | data[5]) / 10.0, 1),
        FloatToStr(floatBuf[2], (data[6] - 0x50) / 2.0, 1)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseEnginePkt

VanPacketParseResult_t ParseHeadUnitStalkPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9C4

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"head_unit_stalk_button_next\": \"%S\",\n"
            "\"head_unit_stalk_button_prev\": \"%S\",\n"
            "\"head_unit_stalk_button_volume_up\": \"%S\",\n"
            "\"head_unit_stalk_button_volume_down\": \"%S\",\n"
            "\"head_unit_stalk_button_source\": \"%S\",\n"
            "\"head_unit_stalk_wheel\": \"%d\",\n"
            "\"head_unit_stalk_wheel_rollover\": \"%u\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? onStr : offStr,
        data[0] & 0x40 ? onStr : offStr,
        data[0] & 0x08 ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        data[0] & 0x02 ? onStr : offStr,

        data[1] - 0x80,
        data[0] >> 4 & 0x03
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitStalkPkt

VanPacketParseResult_t ParseLightsStatusPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_1
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanInstrumentClusterV1Structs.h

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 14) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    uint16_t remainingKmToService = ((uint16_t)data[2] << 8 | data[3]) * 20;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"instrument_cluster\": \"%SENALED\",\n"
            "\"speed_regulator_wheel\": \"%S\",\n"
            "\"warning_led\": \"%S\",\n"
            "\"diesel_glow_plugs\": \"%S\",\n"
            "\"door_open\": \"%S\",\n"
            "\"remaining_km_to_service\": \"%u\",\n"
            "\"remaining_km_to_service_dash\": \"%u\",\n"
            "\"remaining_km_to_service_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%S)\"\n"
                "}\n"
            "},\n"
            "\"lights\": \"%S%S%S%S%S%S\"";

    char floatBuf[MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        data[0] & 0x80 ? emptyStr : PSTR("NOT "),
        data[0] & 0x40 ? onStr : offStr,
        data[0] & 0x20 ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        data[1] & 0x01 ? yesStr : noStr,

        remainingKmToService,
        remainingKmToService / 100 * 100,  // Round downwards to nearest multiple of 100 kms

        // TODO - hard coded value 30,000 kms for 100%
        #define SERVICE_INTERVAL (30000)
        remainingKmToService >= SERVICE_INTERVAL ? PSTR("1") :
            FloatToStr(floatBuf, (float)remainingKmToService / SERVICE_INTERVAL, 2),

        data[5] & 0x80 ? PSTR("DIPPED_BEAM ") : emptyStr,
        data[5] & 0x40 ? PSTR("HIGH_BEAM ") : emptyStr,
        data[5] & 0x20 ? PSTR("FOG_FRONT ") : emptyStr,
        data[5] & 0x10 ? PSTR("FOG_REAR ") : emptyStr,
        data[5] & 0x08 ? PSTR("INDICATOR_RIGHT ") : emptyStr,
        data[5] & 0x04 ? PSTR("INDICATOR_LEFT ") : emptyStr
    );

    if (data[5] & 0x02)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at,
                PSTR(",\n\"auto_gearbox\": \"%S%S%S%S\""),
                (data[4] & 0x70) == 0x00 ? PSTR("P") :
                (data[4] & 0x70) == 0x10 ? PSTR("R") :
                (data[4] & 0x70) == 0x20 ? PSTR("N") :
                (data[4] & 0x70) == 0x30 ? PSTR("D") :
                (data[4] & 0x70) == 0x40 ? PSTR("4") :
                (data[4] & 0x70) == 0x50 ? PSTR("3") :
                (data[4] & 0x70) == 0x60 ? PSTR("2") :
                (data[4] & 0x70) == 0x70 ? PSTR("1") :
                ToHexStr((uint8_t)(data[4] & 0x70)),

                data[4] & 0x08 ? PSTR(" - Snow") : emptyStr,
                data[4] & 0x04 ? PSTR(" - Sport") : emptyStr,
                data[4] & 0x80 ? PSTR(" (blinking)") : emptyStr
            );
    } // if

    if (data[6] != 0xFF)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"oil_temperature\": \"%d\""), (int)data[6] - 40);  // Never seen this
    } // if

    if (data[7] != 0xFF)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"fuel_level\": \"%u\""), data[7]);  // Never seen this
    } // if

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_raw\": \"%u\""), data[8]);

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at,
            ",\n"
            "\"oil_level_raw_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%S)\"\n"
                "}\n"
            "}",

            // 0x55 = 85
            #define MAX_OIL_LEVEL (0x55)
            data[8] >= MAX_OIL_LEVEL ?
                PSTR("1") :
                FloatToStr(floatBuf, (float)data[8] / MAX_OIL_LEVEL, 2)
        );

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR(",\n\"oil_level_dash\": \"%S\""),
            data[8] <= 0x0B ? PSTR("------") :
            data[8] <= 0x19 ? PSTR("O-----") :
            data[8] <= 0x27 ? PSTR("OO----") :
            data[8] <= 0x35 ? PSTR("OOO---") :
            data[8] <= 0x43 ? PSTR("OOOO--") :
            data[8] <= 0x51 ? PSTR("OOOOO-") :
            PSTR("OOOOOO")
        );

    if (data[10] != 0xFF)
    {
        // Never seen this; I don't have LPG
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"lpg_fuel_level\": \"%S\""),
                data[10] <= 0x08 ? PSTR("1") :
                data[10] <= 0x11 ? PSTR("2") :
                data[10] <= 0x21 ? PSTR("3") :
                data[10] <= 0x32 ? PSTR("4") :
                data[10] <= 0x43 ? PSTR("5") :
                data[10] <= 0x53 ? PSTR("6") :
                PSTR("7")
            );
    } // if

    if (dataLen == 14)
    {
        // Vehicles made in/after 2004?

        // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4FC_2

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control\": \"%S\""),
                data[11] == 0x41 ? offStr :
                data[11] == 0x49 ? PSTR("Cruise") :
                data[11] == 0x59 ? PSTR("Cruise - speed") :
                data[11] == 0x81 ? PSTR("Limiter") :
                data[11] == 0x89 ? PSTR("Limiter - speed") :
                ToHexStr(data[11])
            );

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"cruise_control_speed\": \"%u\""), data[12]);
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseLightsStatusPkt

VanPacketParseResult_t ParseDeviceReportPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8C4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8C4

    int dataLen = pkt.DataLen();
    if (dataLen < 1 || dataLen > 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    int at = 0;

    if (data[0] == 0x8A)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_report\": \"%S\"";

        at = snprintf_P(buf, n, jsonFormatter,

            data[1] == 0x20 ? PSTR("TUNER_REPLY") :
            data[1] == 0x21 ? PSTR("AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0x22 ? PSTR("BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0x24 ? PSTR("TUNER_ANNOUNCEMENT") :
            data[1] == 0x28 ? PSTR("TAPE_PRESENCE_ANNOUNCEMENT") :
            data[1] == 0x30 ? PSTR("CD_PRESENT") :
            data[1] == 0x40 ? PSTR("TUNER_PRESETS_REPLY") :
            data[1] == 0x60 ? PSTR("TAPE_INFO_REPLY") :
            data[1] == 0x61 ? PSTR("TAPE_PLAYING_AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0x62 ? PSTR("TAPE_PLAYING_BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0x64 ? PSTR("TAPE_PLAYING_STARTING") :
            data[1] == 0x68 ? PSTR("TAPE_PLAYING_INFO") :
            data[1] == 0xC0 ? PSTR("INTERNAL_CD_TRACK_INFO_REPLY") :
            data[1] == 0xC1 ? PSTR("INTERNAL_CD_PLAYING_AUDIO_SETTINGS_ANNOUNCE") :
            data[1] == 0xC2 ? PSTR("INTERNAL_CD_PLAYING_BUTTON_PRESS_ANNOUNCE") :
            data[1] == 0xC4 ? PSTR("INTERNAL_CD_PLAYING_SEARCHING") :
            data[1] == 0xD0 ? PSTR("INTERNAL_CD_PLAYING_TRACK_INFO") :
            ToHexStr(data[1])
        );

        // Button-press announcement?
        if ((data[1] & 0x0F) == 0x02)
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at,
                    PSTR(",\n\"head_unit_button_pressed\": \"%S%S\""),

                    (data[2] & 0x1F) == 0x01 ? PSTR("1") :
                    (data[2] & 0x1F) == 0x02 ? PSTR("2") :
                    (data[2] & 0x1F) == 0x03 ? PSTR("3") :
                    (data[2] & 0x1F) == 0x04 ? PSTR("4") :
                    (data[2] & 0x1F) == 0x05 ? PSTR("5") :
                    (data[2] & 0x1F) == 0x06 ? PSTR("6") :
                    (data[2] & 0x1F) == 0x11 ? PSTR("AUDIO_DOWN") :
                    (data[2] & 0x1F) == 0x12 ? PSTR("AUDIO_UP") :
                    (data[2] & 0x1F) == 0x13 ? PSTR("SEEK_BACKWARD") :
                    (data[2] & 0x1F) == 0x14 ? PSTR("SEEK_FORWARD") :
                    (data[2] & 0x1F) == 0x16 ? PSTR("AUDIO") :
                    (data[2] & 0x1F) == 0x17 ? PSTR("MAN") :
                    (data[2] & 0x1F) == 0x1B ? PSTR("TUNER") :
                    (data[2] & 0x1F) == 0x1C ? PSTR("TAPE") :
                    (data[2] & 0x1F) == 0x1D ? PSTR("CD") :
                    (data[2] & 0x1F) == 0x1E ? PSTR("CD_CHANGER") :
                    ToHexStr(data[2]),

                    (data[2] & 0xC0) == 0xC0 ? PSTR(" (held)") :
                    data[2] & 0x40 ? PSTR(" (released)") :
                    data[2] & 0x80 ? PSTR(" (repeat)") :
                    emptyStr
                );
        } // if

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
    }
    else if (data[0] == 0x96)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"cd_changer_announce\": \"STATUS_UPDATE_ANNOUNCE\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0x07)
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // MFD request to sat nav (can e.g. be triggered by the user selecting a button on a sat nav selection
        // screen by using the remote control).
        //
        // Found combinations:
        //
        // 07-00-01
        // 07-00-02 - MFD requests "satnav_status_3" ?
        // 07-00-03 - MFD requests "satnav_status_3" and "satnav_status_2" ?
        // 07-01-00 - MFD requests "satnav_status_1" ?
        // 07-01-01 - User selected street from list. MFD requests "satnav_status_1" ?
        // 07-01-03
        // 07-10-00 - User pressed "Val" on remote control
        // 07-20-00 - MFD requests next "satnav_report" packet
        // 07-21-00 - MFD requests "satnav_status_1" and next "satnav_report" packet ?
        // 07-21-01 - User selected city from list. MFD requests "satnav_status_1" and next "satnav_report" packet ?
        // 07-40-00 - MFD requests "satnav_status_2" ?
        // 07-40-02 - MFD requests "satnav_status_2"
        // 07-41-00 - MFD requests "satnav_status_1" and "satnav_status_2" ?
        // 07-44-00 - MFD requests "satnav_status_2" and "satnav_guidance_data"
        // 07-47-00 - MFD requests "satnav_status_1", "satnav_status_2", "satnav_guidance_data" and "satnav_guidance" ?
        // 07-60-00
        //
        // So it looks like data[1] and data[2] are in fact bitfields:
        //
        // data[1]
        // & 0x01: Requesting "satnav_status_1" (IDEN 0x54E)
        // & 0x02: Requesting "satnav_guidance" (IDEN 0x64E)
        // & 0x04: Requesting "satnav_guidance_data" (IDEN 0x9CE)
        // & 0x10: User pressing "Val" on remote control, requesting "satnav_to_mfd_response" (IDEN 0x74E)
        // & 0x20: Requesting next "satnav_report" (IDEN 0x6CE) in sequence
        // & 0x40: Requesting "satnav_status_2" (IDEN 0x7CE)
        //
        // data[2]
        // & 0x01: User selecting
        // & 0x02: Requesting "satnav_status_3" (IDEN 0x8CE) ?

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"mfd_to_satnav_instruction\": \"%S\"\n"
            "}\n"
        "}\n";

        uint16_t code = (uint16_t)data[1] << 8 | data[2];

        at = snprintf_P(buf, n, jsonFormatter,

            // User clicks on "Accept" button (usually bottom left of dialog screen)
            code == 0x0001 ? PSTR("Accept") :

            // Always follows 0x1000
            code == 0x0100 ? PSTR("End-of-button-press") :

            // User selects street from list
            // TODO - also when user selects category from list of services
            code == 0x0101 ? PSTR("Selected_street_from_list") :

            // User selects a menu entry or letter? User pressed "Val" (middle button on IR remote control).
            // Always followed by 0x0100.
            code == 0x1000 ? PSTR("Val") :

            // MFD requests sat nav for next report packet (IDEN 0x6CE) in sequence
            // (Or: select from list using "Val"?)
            code == 0x2000 ? PSTR("Request_next_sat_nav_report_packet") :

            // User selects city from list
            code == 0x2101 ? PSTR("Selected_city_from_list") :

            code == 0x4000 ? PSTR("Request_status_2") :

            code == 0x4100 ? PSTR("Request_status_1_and_2") :

            // MFD asks for sat nav guidance data packet (IDEN 0x9CE) and satnav_status_2 (IDEN 0x7CE)
            code == 0x4400 ? PSTR("Request_sat_nav_guidance_data") :

            ToHexStr(code)
        );
    }
    else if (data[0] == 0x52)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Unknown what this is
        //
        // Found combinations:
        //
        // 52-08
        //
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDeviceReportPkt

VanPacketParseResult_t ParseCarStatus1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#564
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#564

    const uint8_t* data = pkt.Data();
    int dataLen = pkt.DataLen();

    /*
    // Process only if not duplicate of previous packet; ignore different sequence numbers

    // 2021-02-21 - No, don't skip

    static uint8_t packetData[VAN_MAX_DATA_BYTES] = "";  // Previous packet data
    if (memcmp(data + 1, packetData, dataLen - 2) == 0) return VAN_PACKET_DUPLICATE;
    memcpy(packetData, data + 1, dataLen - 2);
    */

    const static char jsonFormatter1[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"door_front_right\": \"%S\",\n"
            "\"door_front_left\": \"%S\",\n"
            "\"door_rear_right\": \"%S\",\n"
            "\"door_rear_left\": \"%S\",\n"
            "\"door_boot\": \"%S\",\n"
            "\"right_stalk_button\": \"%S\",\n"
            "\"exp_moving_avg_speed\": \"%u\",\n"
            "\"inst_consumption_lt_100\": \"%S\",\n"
            "\"distance_to_empty\": \"%u\",\n"
            "\"avg_speed_1\": \"%u\",\n"
            "\"distance_1\": \"%S\",\n"
            "\"avg_consumption_lt_100_1\": \"%S\"";

    uint8_t avgSpeedTrip1 = data[11];
    uint8_t avgSpeedTrip2 = data[12];
    uint16_t distanceTrip1 = (uint16_t)data[14] << 8 | data[15];
    uint16_t avgConsumptionLt100Trip1 = (uint16_t)data[16] << 8 | data[17];
    uint16_t distanceTrip2 = (uint16_t)data[18] << 8 | data[19];
    uint16_t avgConsumptionLt100Trip2 = (uint16_t)data[20] << 8 | data[21];
    uint16_t instConsumptionLt100 = (uint16_t)data[22] << 8 | data[23];

    char floatBuf[2][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter1,
        data[7] & 0x80 ? openStr : closedStr,
        data[7] & 0x40 ? openStr : closedStr,
        data[7] & 0x20 ? openStr : closedStr,
        data[7] & 0x10 ? openStr : closedStr,
        data[7] & 0x08 ? openStr : closedStr,
        data[10] & 0x01 ? PSTR("PRESSED") : PSTR("RELEASED"),

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

        instConsumptionLt100 == 0xFFFF ? PSTR("--.-") : FloatToStr(floatBuf[0], (float)instConsumptionLt100 / 10.0, 1),
        (uint16_t)data[24] << 8 | data[25],
        avgSpeedTrip1,
        distanceTrip1 == 0xFFFF ? notApplicable2Str : ToStr(distanceTrip1),
        avgConsumptionLt100Trip1 == 0xFFFF ? PSTR("--.-") : FloatToStr(floatBuf[1], (float)avgConsumptionLt100Trip1 / 10.0, 1)
    );

    const static char jsonFormatter2[] PROGMEM =
            ",\n"
            "\"avg_speed_2\": \"%u\",\n"
            "\"distance_2\": \"%S\",\n"
            "\"avg_consumption_lt_100_2\": \"%S\"\n"
        "}\n"
    "}\n";

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, jsonFormatter2,
            avgSpeedTrip2,
            distanceTrip2 == 0xFFFF ? notApplicable2Str : ToStr(distanceTrip2),
            avgConsumptionLt100Trip2 == 0xFFFF ? PSTR("--.-") : FloatToStr(floatBuf[0], (float)avgConsumptionLt100Trip2 / 10.0, 1)
        );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus1Pkt

VanPacketParseResult_t ParseCarStatus2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#524
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#524
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV1.h
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanDisplayStructsV2.h

    int dataLen = pkt.DataLen();
    if (dataLen != 14 && dataLen != 16) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    // All known notifications, as literally retrieved from my vehicle (406 year 2003, DAM number 9586; your vehicle
    // may have other texts). Retrieving was done with VanBus/examples/DisplayNotifications/DisplayNotifications.ino .

    // By convention, warning notifications end with "!"; information notifications do not.
    // TODO - translate into all languages

    // Byte 0, 0x00...0x07
    static const char msg_0_0[] PROGMEM = "Tyre pressure too low!";
    static const char msg_0_1[] PROGMEM = "";
    static const char msg_0_2[] PROGMEM = "Automatic gearbox temperature too high!";
    static const char msg_0_3[] PROGMEM = "Brake fluid level low!"; // and exclamation mark on instrument cluster
    static const char msg_0_4[] PROGMEM = "Hydraulic suspension pressure defective!";
    static const char msg_0_5[] PROGMEM = "Suspension defective!";
    static const char msg_0_6[] PROGMEM = "Engine oil temperature too high!"; // and oil can on instrument cluster
    static const char msg_0_7[] PROGMEM = "Engine temperature too high!";

    // Byte 1, 0x08...0x0F
    static const char msg_1_0[] PROGMEM = "Clear diesel filter (FAP) URGENT"; // and mil icon on instrument cluster
    static const char msg_1_1[] PROGMEM = "";
    static const char msg_1_2[] PROGMEM = "Min level additive gasoil!";
    static const char msg_1_3[] PROGMEM = "Fuel cap open!";
    static const char msg_1_4[] PROGMEM = "Puncture(s) detected!";
    static const char msg_1_5[] PROGMEM = "Cooling circuit level too low!"; // and icon on instrument cluster
    static const char msg_1_6[] PROGMEM = "Oil pressure insufficient!";
    static const char msg_1_7[] PROGMEM = "Engine oil level too low!";

    // Byte 2, 0x10...0x17
    static const char msg_2_0[] PROGMEM = "Engine antipollution system defective!";
    static const char msg_2_1[] PROGMEM = "Brake pads worn!";
    static const char msg_2_2[] PROGMEM = "Check Control OK";  // Wow... bad translation
    static const char msg_2_3[] PROGMEM = "Automatic gearbox defective!";
    static const char msg_2_4[] PROGMEM = "ASR / ESP system defective!"; // and icon on instrument cluster
    static const char msg_2_5[] PROGMEM = "ABS brake system defective!";
    static const char msg_2_6[] PROGMEM = "Suspension and power steering defective!";
    static const char msg_2_7[] PROGMEM = "Brake system defective!";

    // Byte 3, 0x18...0x1F
    static const char msg_3_0[] PROGMEM = "Airbag defective!";
    static const char msg_3_1[] PROGMEM = "Airbag defective!";
    static const char msg_3_2[] PROGMEM = "";
    static const char msg_3_3[] PROGMEM = "Engine temperature high!";
    static const char msg_3_4[] PROGMEM = "";
    static const char msg_3_5[] PROGMEM = "";
    static const char msg_3_6[] PROGMEM = "";
    static const char msg_3_7[] PROGMEM = "Water in Diesel fuel filter"; // and icon on instrument cluster

    // Byte 4, 0x20...0x27
    static const char msg_4_0[] PROGMEM = "";
    static const char msg_4_1[] PROGMEM = "Automatic beam adjustment defective!";
    static const char msg_4_2[] PROGMEM = "";
    static const char msg_4_3[] PROGMEM = "";
    static const char msg_4_4[] PROGMEM = "Service battery charge low!";
    static const char msg_4_5[] PROGMEM = "Battery charge low!"; // and battery icon on instrument cluster
    static const char msg_4_6[] PROGMEM = "Diesel antipollution system (FAP) defective!";
    static const char msg_4_7[] PROGMEM = "Engine antipollution system inoperative!"; // MIL icon flashing on instrument cluster

    // Byte 5, 0x28...0x2F
    static const char msg_5_0[] PROGMEM = "Handbrake on!";
    static const char msg_5_1[] PROGMEM = "Safety belt not fastened!";
    static const char msg_5_2[] PROGMEM = "Passenger airbag neutralized"; // and icon on instrument cluster
    static const char msg_5_3[] PROGMEM = "Windshield liquid level too low";
    static const char msg_5_4[] PROGMEM = "Current speed too high";
    static const char msg_5_5[] PROGMEM = "Ignition key still inserted";
    static const char msg_5_6[] PROGMEM = "Lights not on";
    static const char msg_5_7[] PROGMEM = "";

    // Byte 6, 0x30...0x37
    static const char msg_6_0[] PROGMEM = "Impact sensor defective";
    static const char msg_6_1[] PROGMEM = "";
    static const char msg_6_2[] PROGMEM = "Tyre pressure sensor battery low";
    static const char msg_6_3[] PROGMEM = "Plip remote control battery low";
    static const char msg_6_4[] PROGMEM = "";
    static const char msg_6_5[] PROGMEM = "Place automatic gearbox in P position";
    static const char msg_6_6[] PROGMEM = "Testing stop lamps : brake gently";
    static const char msg_6_7[] PROGMEM = "Fuel level low!";

    // Byte 7, 0x38...0x3F
    static const char msg_7_0[] PROGMEM = "Automatic headlight activation system disabled";
    static const char msg_7_1[] PROGMEM = "Turn-headlight defective!";
    static const char msg_7_2[] PROGMEM = "Turn-headlight disable";
    static const char msg_7_3[] PROGMEM = "Turn-headlight enable";
    static const char msg_7_4[] PROGMEM = "";
    static const char msg_7_5[] PROGMEM = "7 tyre pressure sensors missing!";
    static const char msg_7_6[] PROGMEM = "7 tyre pressure sensors missing!";
    static const char msg_7_7[] PROGMEM = "7 tyre pressure sensors missing!";

    // Byte 8, 0x40...0x47
    static const char msg_8_0[] PROGMEM = "Doors locked";
    static const char msg_8_1[] PROGMEM = "ASR / ESP system disabled";
    static const char msg_8_2[] PROGMEM = "Child safety lock enabled";
    static const char msg_8_3[] PROGMEM = "Door self locking system enabled";
    static const char msg_8_4[] PROGMEM = "Automatic headlight activation system enabled";
    static const char msg_8_5[] PROGMEM = "Automatic wiper system enabled";
    static const char msg_8_6[] PROGMEM = "Electronic anti-theft system defective";
    static const char msg_8_7[] PROGMEM = "Sport suspension mode enabled";

    // Byte 9 is the index of the current message

    // Byte 10, 0x50...0x57
    static const char msg_10_0[] PROGMEM = "";
    static const char msg_10_1[] PROGMEM = "";
    static const char msg_10_2[] PROGMEM = "";
    static const char msg_10_3[] PROGMEM = "";
    static const char msg_10_4[] PROGMEM = "";
    static const char msg_10_5[] PROGMEM = "";
    static const char msg_10_6[] PROGMEM = "";
    static const char msg_10_7[] PROGMEM = "";

    // Byte 11, 0x58...0x5F
    static const char msg_11_0[] PROGMEM = "";
    static const char msg_11_1[] PROGMEM = "";
    static const char msg_11_2[] PROGMEM = "";
    static const char msg_11_3[] PROGMEM = "";
    static const char msg_11_4[] PROGMEM = "";
    static const char msg_11_5[] PROGMEM = "";
    static const char msg_11_6[] PROGMEM = "";
    static const char msg_11_7[] PROGMEM = "";

    // Byte 12, 0x60...0x67
    static const char msg_12_0[] PROGMEM = "";
    static const char msg_12_1[] PROGMEM = "";
    static const char msg_12_2[] PROGMEM = "";
    static const char msg_12_3[] PROGMEM = "";
    static const char msg_12_4[] PROGMEM = "";
    static const char msg_12_5[] PROGMEM = "";
    static const char msg_12_6[] PROGMEM = "";
    static const char msg_12_7[] PROGMEM = "";

    // Byte 13, 0x68...0x6F
    static const char msg_13_0[] PROGMEM = "";
    static const char msg_13_1[] PROGMEM = "";
    static const char msg_13_2[] PROGMEM = "";
    static const char msg_13_3[] PROGMEM = "";
    static const char msg_13_4[] PROGMEM = "";
    static const char msg_13_5[] PROGMEM = "";
    static const char msg_13_6[] PROGMEM = "";
    static const char msg_13_7[] PROGMEM = "";

    // On vehicles made after 2004

    // Byte 14, 0x70...0x77
    static const char msg_14_0[] PROGMEM = "";
    static const char msg_14_1[] PROGMEM = "";
    static const char msg_14_2[] PROGMEM = "";
    static const char msg_14_3[] PROGMEM = "";
    static const char msg_14_4[] PROGMEM = "";
    static const char msg_14_5[] PROGMEM = "";
    static const char msg_14_6[] PROGMEM = "";
    static const char msg_14_7[] PROGMEM = "";

    // Byte 15, 0x78...0x7F
    static const char msg_15_0[] PROGMEM = "";
    static const char msg_15_1[] PROGMEM = "";
    static const char msg_15_2[] PROGMEM = "";
    static const char msg_15_3[] PROGMEM = "";
    static const char msg_15_4[] PROGMEM = "";
    static const char msg_15_5[] PROGMEM = "";
    static const char msg_15_6[] PROGMEM = "";
    static const char msg_15_7[] PROGMEM = "";

    // To save precious RAM, we use a PROGMEM array of PROGMEM strings
    // See also: https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
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
        msg_8_0, msg_8_1, msg_8_2, msg_8_3, msg_8_4, msg_8_5, msg_8_6, msg_8_7,
        0, 0, 0, 0, 0, 0, 0, 0,
        msg_10_0, msg_10_1, msg_10_2, msg_10_3, msg_10_4, msg_10_5, msg_10_6, msg_10_7,
        msg_11_0, msg_11_1, msg_11_2, msg_11_3, msg_11_4, msg_11_5, msg_11_6, msg_11_7,
        msg_12_0, msg_12_1, msg_12_2, msg_12_3, msg_12_4, msg_12_5, msg_12_6, msg_12_7,
        msg_13_0, msg_13_1, msg_13_2, msg_13_3, msg_13_4, msg_13_5, msg_13_6, msg_13_7,
        msg_14_0, msg_14_1, msg_14_2, msg_14_3, msg_14_4, msg_14_5, msg_14_6, msg_14_7,
        msg_15_0, msg_15_1, msg_15_2, msg_15_3, msg_15_4, msg_15_5, msg_15_6, msg_15_7
    };

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"alarm_list\":\n"
            "[";

    int at = snprintf_P(buf, n, jsonFormatter);

    const uint8_t* data = pkt.Data();

    bool first = true;
    for (int byte = 0; byte < dataLen; byte++)
    {
        if (byte == 9) byte++;
        for (int bit = 0; bit < 8; bit++)
        {
            if (data[byte] >> bit & 0x01)
            {
                char alarmText[80];  // Make sure this is large enough for the largest string it must hold; see above
                strncpy_P(alarmText, (char *)pgm_read_dword(&(msgTable[byte * 8 + bit])), sizeof(alarmText) - 1);
                alarmText[sizeof(alarmText) - 1] = 0;

                // TODO - with lots of alarms set, this packet could become very large and overflow the JSON buffer
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%S\n\"%s\""), first ? emptyStr : commaStr, alarmText);
                first = false;
            } // if
        } // for
    } // for

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]"));

    uint8_t currentMsg = data[9];

    // Relying on short-circuit boolean evaluation
    if (currentMsg <= 0x7F && strlen_P(msgTable[currentMsg]) > 0)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"notification_message_on_mfd\": \"%S\""), msgTable[currentMsg]);
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCarStatus2Pkt

VanPacketParseResult_t ParseDashboardPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#824
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#824

    const uint8_t* data = pkt.Data();

    uint16_t engineRpm = (uint16_t)data[0] << 8 | data[1];
    uint16_t vehicleSpeed = (uint16_t)data[2] << 8 | data[3];

    static long savedEngineRpm = 0;
    static long savedVehicleSpeed = 0;

    // With engine running, there are about 20 or so of these packets per second. Limit the rate somewhat.
    // Send only if any of the reported values changes with more than 10 /min (engine_rpm) or 1 km/h (vehicle_speed).
    long diffEngineRpm = engineRpm - savedEngineRpm;
    long diffVehicleSpeed = vehicleSpeed - savedVehicleSpeed;
    if (abs(diffEngineRpm) < 10 * 8 && abs(diffVehicleSpeed) < 100 * 1) return VAN_PACKET_NO_CONTENT;

    savedEngineRpm = engineRpm;
    savedVehicleSpeed = vehicleSpeed;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"engine_rpm\": \"%S\",\n"
            "\"vehicle_speed\": \"%S\"\n"
        "}\n"
    "}\n";

    char floatBuf[2][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        engineRpm == 0xFFFF ?
            //PSTR("---.-") :
            PSTR("---") :
            //FloatToStr(floatBuf[0], engineRpm / 8.0, 1),
            FloatToStr(floatBuf[0], engineRpm / 8.0, 0),
        vehicleSpeed == 0xFFFF ?
            //PSTR("---.--") :
            PSTR("--") :
            //FloatToStr(floatBuf[1], vehicleSpeed / 100.0, 2)
            FloatToStr(floatBuf[1], vehicleSpeed / 100.0, 0)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardPkt

VanPacketParseResult_t ParseDashboardButtonsPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#664
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#664

    int dataLen = pkt.DataLen();
    if (dataLen != 11 && dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    float fuelLevelFiltered = data[4] / 2.0;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"hazard_lights\": \"%S\",\n"
            "\"door_lock\": \"%S\",\n"
            "\"dashboard_programmed_brightness\": \"%u\",\n"
            "\"esp\": \"%S\",\n"
            "\"fuel_level_filtered\": \"%S\",\n"
            "\"fuel_level_filtered_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%S)\"\n"
                "}\n"
            "},\n"
            "\"fuel_level_raw\": \"%S\"\n"
        "}\n"
    "}\n";

    // data[6..10] - always 00-FF-00-FF-00

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        data[0] & 0x02 ? onStr : offStr,
        data[2] & 0x40 ? PSTR("LOCKED") : PSTR("UNLOCKED"),
        data[2] & 0x0F,
        data[3] & 0x02 ? onStr : offStr,

        // Surely fuel level. Test with tank full shows definitely level is in litres.
        data[4] == 0xFF ? notApplicable3Str : FloatToStr(floatBuf[0], fuelLevelFiltered, 1),

        #define FULL_TANK_LITRES (73.0)
        data[4] == 0xFF ? PSTR("0") :
            fuelLevelFiltered >= FULL_TANK_LITRES ? PSTR("1") :
                FloatToStr(floatBuf[1], fuelLevelFiltered / FULL_TANK_LITRES, 2),

        data[5] == 0xFF ? notApplicable3Str : FloatToStr(floatBuf[2], data[5] / 2.0, 1)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseDashboardButtonsPkt

VanPacketParseResult_t ParseHeadUnitPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
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
    int at = 0;

    switch (infoType)
    {
        case INFO_TYPE_TUNER:
        {
            // Message when the head unit is in "tuner" (radio) mode

            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_1

            if (dataLen != 22) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // data[2]: radio band and preset position
            uint8_t band = data[2] & 0x07;
            uint8_t presetMemory = data[2] >> 3 & 0x0F;
            char presetMemoryBuffer[3];
            sprintf_P(presetMemoryBuffer, PSTR("%1u"), presetMemory);

            // data[3]: search bits
            bool dxSensitivity = data[3] & 0x02;  // Tuner sensitivity: distant (Dx) or local (Lo)
            bool ptyStandbyMode = data[3] & 0x04;

            uint8_t searchMode = data[3] >> 3 & 0x07;
            bool searchDirectionUp = data[3] & 0x80;
            bool anySearchBusy = searchMode != TS_NOT_SEARCHING;
            bool automaticSearchBusy = anySearchBusy && searchMode != TS_MANUAL;  // Any search except manual

            // data[4] and data[5]: frequency being scanned or tuned in to
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
            //   - No AF (Alternative Frequencies) available
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

            const static char jsonFormatterCommon[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"tuner_band\": \"%S\",\n"
                    "\"fm_band\": \"%S\",\n"
                    "\"fm_band_1\": \"%S\",\n"
                    "\"fm_band_2\": \"%S\",\n"
                    "\"fm_band_ast\": \"%S\",\n"
                    "\"am_band\": \"%S\",\n"
                    "\"tuner_memory\": \"%S\",\n"
                    "\"frequency\": \"%S %S\",\n"
                    "\"signal_strength\": \"%S\",\n"
                    "\"search_mode\": \"%S\",\n"
                    "\"search_sensitivity\": \"%S\",\n"
                    "\"search_direction\": \"%S\"";

            char floatBuf[MAX_FLOAT_SIZE];
            at = snprintf_P(buf, n, jsonFormatterCommon,
                TunerBandStr(band),
                band == TB_FM1 || band == TB_FM2 || band == TB_FM3 || band == TB_FMAST || band == TB_PTY_SELECT ?
                    onStr : offStr,
                band == TB_FM1 ? onStr : offStr,
                band == TB_FM2 ? onStr : offStr,
                band == TB_FMAST ? onStr : offStr,
                band == TB_AM ? onStr : offStr,
                presetMemory == 0 ? PSTR("-") : presetMemoryBuffer,
                frequency == 0x07FF ? notApplicable3Str :
                    band == TB_AM
                        ? FloatToStr(floatBuf, frequency, 0)  // AM and LW bands
                        : FloatToStr(floatBuf, (frequency / 2 + 500) / 10.0, 1),  // FM bands
                band == TB_AM ? PSTR("KHz") : PSTR("MHz"),

                // Also applicable in AM mode
                signalStrength == 15 && (searchMode == TS_BY_FREQUENCY || searchMode == TS_BY_MATCHING_PTY)
                    ? notApplicable2Str
                    : signalStrengthBuffer,

                TunerSearchModeStr(searchMode),

                // Sensitivity of automatic search: distant (Dx) or local (Lo)
                ! automaticSearchBusy ? emptyStr : dxSensitivity ? PSTR("Dx") : PSTR("Lo"),

                ! anySearchBusy ? emptyStr : searchDirectionUp ? PSTR("UP") : PSTR("DOWN")
            );

            if (band != TB_AM)
            {
                // FM bands only

                // data[7]: TA, RDS and REG (regional) bits
                bool rdsSelected = data[7] & 0x01;
                bool taSelected = data[7] & 0x02;
                bool regional = data[7] & 0x04;  // Long-press "RDS" button
                bool rdsNotAvailable = (data[7] & 0x20) == 0;
                bool taNotAvailable = (data[7] & 0x40) == 0;
                bool taAnnounce = data[7] & 0x80;

                // data[8] and data[9]: PI code
                // See also:
                // - https://en.wikipedia.org/wiki/Radio_Data_System#Program_Identification_Code_(PI_Code),
                // - https://radio-tv-nederland.nl/rds/rds.html
                // - https://people.uta.fi/~jk54415/dx/pi-codes.html
                // - http://poupa.cz/rds/countrycodes.htm
                // - https://www.pira.cz/rds/p232man.pdf
                uint16_t piCode = (uint16_t)data[8] << 8 | data[9];
                uint8_t countryCode = piCode >> 12 & 0x0F;
                uint8_t coverageCode = piCode >> 8 & 0x0F;
                char piBuffer[40];
                sprintf_P(piBuffer, PSTR("%04X"), piCode);

                // data[10]: for PTY-based search mode
                // & 0x1F: PTY code to search
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

                const static char jsonFormatterFmBand[] PROGMEM = ",\n"
                    "\"pty_selection_menu\": \"%S\",\n"
                    "\"selected_pty_full\": \"%S\",\n"
                    "\"pty_standby_mode\": \"%S\",\n"
                    "\"pty_match\": \"%S\",\n"
                    "\"pty_full\": \"%S\",\n"
                    "\"pi_code\": \"%S\",\n"
                    "\"pi_country\": \"%S\",\n"
                    "\"pi_area_coverage\": \"%S\",\n"
                    "\"regional\": \"%S\",\n"
                    "\"ta_selected\": \"%S\",\n"
                    "\"ta_not_available\": \"%S\",\n"
                    "\"rds_selected\": \"%S\",\n"
                    "\"rds_not_available\": \"%S\",\n"
                    "\"rds_text\": \"%s\",\n"
                    "\"info_traffic\": \"%S\"";

                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, jsonFormatterFmBand,
                        ptySelectionMenu ? onStr : offStr,

                        selectedPty == 0x00 ? notApplicable3Str : PtyStrFull(selectedPty),

                        ptyStandbyMode ? yesStr : noStr,
                        ptyMatch ? yesStr : noStr,
                        currPty == 0x00 ? notApplicable3Str : PtyStrFull(currPty),

                        piCode == 0xFFFF ? notApplicable3Str : piBuffer,
                        piCode == 0xFFFF ? notApplicable2Str : RadioPiCountry(countryCode),
                        piCode == 0xFFFF ? notApplicable3Str : RadioPiAreaCoverage(coverageCode),

                        regional ? onStr : offStr,
                        taSelected ? yesStr : noStr,
                        taNotAvailable ? yesStr : noStr,
                        rdsSelected ? yesStr : noStr,
                        rdsNotAvailable ? yesStr : noStr,
                        rdsTxt,

                        taAnnounce ? yesStr : noStr
                    );
            } // if

            at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
        }
        break;

        case INFO_TYPE_TAPE:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_2

            if (dataLen != 5) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            uint8_t status = data[2] & 0x3C;

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"tape_side\": \"%S\",\n"
                    "\"tape_status\": \"%S\",\n"
                    "\"tape_status_stopped\": \"%S\",\n"
                    "\"tape_status_loading\": \"%S\",\n"
                    "\"tape_status_play\": \"%S\",\n"
                    "\"tape_status_fast_forward\": \"%S\",\n"
                    "\"tape_status_next_track\": \"%S\",\n"
                    "\"tape_status_rewind\": \"%S\",\n"
                    "\"tape_status_previous_track\": \"%S\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,

                data[2] & 0x01 ? PSTR("2") : PSTR("1"),

                status == 0x00 ? PSTR("STOPPED") :
                status == 0x04 ? PSTR("LOADING") :
                status == 0x0C ? PSTR("PLAY") :
                status == 0x10 ? PSTR("FAST_FORWARD") :
                status == 0x14 ? PSTR("NEXT_TRACK") :
                status == 0x30 ? PSTR("REWIND") :
                status == 0x34 ? PSTR("PREVIOUS_TRACK") :
                ToHexStr(status),

                status == 0x00 ? onStr : offStr,
                status == 0x04 ? onStr : offStr,
                status == 0x0C ? onStr : offStr,
                status == 0x10 ? onStr : offStr,
                status == 0x14 ? onStr : offStr,
                status == 0x30 ? onStr : offStr,
                status == 0x34 ? onStr : offStr
            );
        }
        break;

        case INFO_TYPE_PRESET:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_3

            if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            uint8_t tunerBand = data[2] >> 4 & 0x07;
            uint8_t tunerMemory = data[2] & 0x0F;

            char rdsOrFreqTxt[9];
            strncpy(rdsOrFreqTxt, (const char*) data + 3, 8);
            rdsOrFreqTxt[8] = 0;

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"radio_preset_%S_%u\": \"%s%S\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,
                TunerBandStr(tunerBand),
                tunerMemory,
                rdsOrFreqTxt,
                tunerBand == TB_AM ? PSTR(" KHz") : data[2] & 0x80 ? emptyStr : PSTR(" MHz")
            );
        }
        break;

        case INFO_TYPE_CD:
        {
            // http://pinterpeti.hu/psavanbus/PSA-VAN.html#554_6

            // TODO - do we know the fixed numbers? Seems like this can only be 10 or 12.
            if (dataLen < 10) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            char trackTimeStr[7];
            sprintf_P(trackTimeStr, PSTR("%X:%02X"), data[5], data[6]);

            static bool loading = false;
            bool searching = data[3] & 0x10;

            // Keep on reporting "loading" until no longer "searching"
            if ((data[3] & 0x0F) == 0x01) loading = true;
            if (! searching) loading = false;

            uint8_t totalTracks = data[8];
            bool totalTracksInvalid = totalTracks == 0xFF;
            char totalTracksStr[3];
            if (! totalTracksInvalid) sprintf_P(totalTracksStr, PSTR("%X"), totalTracks);

            char totalTimeStr[7];
            bool totalTimeInvalid = dataLen < 12;
            if (! totalTimeInvalid)
            {
                uint8_t totalTimeMin = data[9];
                uint8_t totalTimeSec = data[10];
                totalTimeInvalid = totalTimeMin == 0xFF || totalTimeSec == 0xFF;
                if (! totalTimeInvalid) sprintf_P(totalTimeStr, PSTR("%X:%02X"), totalTimeMin, totalTimeSec);
            } // if

            const static char jsonFormatter[] PROGMEM =
            "{\n"
                "\"event\": \"display\",\n"
                "\"data\":\n"
                "{\n"
                    "\"cd_status\": \"%S\",\n"
                    "\"cd_status_loading\": \"%S\",\n"
                    "\"cd_status_eject\": \"%S\",\n"
                    "\"cd_status_pause\": \"%S\",\n"
                    "\"cd_status_play\": \"%S\",\n"
                    "\"cd_status_fast_forward\": \"%S\",\n"
                    "\"cd_status_rewind\": \"%S\",\n"
                    "\"cd_status_searching\": \"%S\",\n"
                    "\"cd_track_time\": \"%S\",\n"
                    "\"cd_current_track\": \"%X\",\n"
                    "\"cd_total_tracks\": \"%S\",\n"
                    "\"cd_total_time\": \"%S\",\n"
                    "\"cd_random\": \"%S\"\n"
                "}\n"
            "}\n";

            at = snprintf_P(buf, n, jsonFormatter,

                data[3] == 0x00 ? PSTR("EJECT") :
                data[3] == 0x10 ? PSTR("ERROR") :  // E.g. disc inserted upside down
                data[3] == 0x11 ? PSTR("LOADING") :
                data[3] == 0x12 ? PSTR("PAUSE-SEARCHING") :
                data[3] == 0x13 ? PSTR("PLAY-SEARCHING") :
                data[3] == 0x02 ? PSTR("PAUSE") :
                data[3] == 0x03 ? PSTR("PLAY") :
                data[3] == 0x04 ? PSTR("FAST_FORWARD") :
                data[3] == 0x05 ? PSTR("REWIND") :
                ToHexStr(data[3]),

                loading ? onStr : offStr,
                data[3] == 0x00 ? onStr : offStr,
                (data[3] & 0x0F) == 0x02 && ! searching ? onStr : offStr,
                (data[3] & 0x0F) == 0x03 && ! searching ? onStr : offStr,
                (data[3] & 0x0F) == 0x04 ? onStr : offStr,
                (data[3] & 0x0F) == 0x05 ? onStr : offStr,

                (data[3] == 0x12 || data[3] == 0x13) && ! loading ? onStr : offStr,

                searching ? PSTR("--:--") : trackTimeStr,

                data[7],
                totalTracksInvalid ? notApplicable2Str : totalTracksStr,
                totalTimeInvalid ? PSTR("--:--") : totalTimeStr,

                data[2] & 0x01 ? onStr : offStr
            );
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

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseHeadUnitPkt

VanPacketParseResult_t ParseTimePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#984
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#984

    // TODO - seems to have nothing to do with time. Mine is always the same:
    //   Raw: #2692 ( 7/15) 10 0E 984 W-0 00-00-00-06-08-D0-C8 NO_ACK OK D0C8 CRC_OK
    // regardless of the time.

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"uptime_battery\": \"%u\",\n"
            "\"uptime\": \"%uh%um\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,
        (uint16_t)data[0] << 8 | data[1],
        data[3],
        data[4]
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseTimePkt

VanPacketParseResult_t ParseAudioSettingsPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4D4
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanRadioInfoStructs.h

    const uint8_t* data = pkt.Data();
    uint8_t volume = data[5] & 0x7F;
    bool tapePresent = data[4] & 0x20;
    bool cdPresent = data[4] & 0x40;
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"power\": \"%S\",\n"  // TODO - change to "head_unit_power"
            "\"tape_present\": \"%S\",\n"
            "\"cd_present\": \"%S\",\n"
            "\"audio_source\": \"%S\",\n"
            "\"ext_mute\": \"%S\",\n"
            "\"mute\": \"%S\",\n"
            "\"volume\": \"%u\",\n"
            "\"volume_update\": \"%S\",\n"
            "\"volume_perc\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"scaleX(%S)\"\n"
                "}\n"
            "},\n"
            "\"audio_menu\": \"%S\",\n"
            "\"bass\": \"%+d\",\n"
            "\"bass_update\": \"%S\",\n"
            "\"treble\": \"%+d\",\n"
            "\"treble_update\": \"%S\",\n"
            "\"loudness\": \"%S\",\n"
            "\"fader\": \"%+d\",\n"
            "\"fader_update\": \"%S\",\n"
            "\"balance\": \"%+d\",\n"
            "\"balance_update\": \"%S\",\n"
            "\"auto_volume\": \"%S\"\n"
        "}\n"
    "}\n";

    char floatBuf[MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        data[2] & 0x01 ? onStr : offStr,  // Power
        tapePresent ? yesStr : noStr,  // Tape present
        cdPresent ? yesStr : noStr,  // CD present

        (data[4] & 0x0F) == 0x00 ? noneStr :  // Source of audio
        (data[4] & 0x0F) == 0x01 ? PSTR("TUNER") :
        (data[4] & 0x0F) == 0x02 ?
            tapePresent ? PSTR("TAPE") :
            cdPresent ? PSTR("CD") :
            PSTR("INTERNAL_CD_OR_TAPE") :
        (data[4] & 0x0F) == 0x03 ? PSTR("CD_CHANGER") :

        // This is the "default" mode for the head unit, to sit there and listen to the navigation
        // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
        // whenever this source is chosen.
        (data[4] & 0x0F) == 0x05 ? PSTR("NAVIGATION") :

        ToHexStr((uint8_t)(data[4] & 0x0F)),

        // External mute. Activated when head unit ISO connector A pin 1 ("Phone mute") is pulled LOW (to Ground).
        data[1] & 0x02 ? onStr : offStr,

        // Mute. To activate: press both VOL_UP and VOL_DOWN buttons on stalk.
        data[1] & 0x01 ? onStr : offStr,

        volume,
        data[5] & 0x80 ? yesStr : noStr,

        // TODO - hard coded value 30 for 100%
        #define MAX_AUDIO_VOLUME (30)
        FloatToStr(floatBuf, (float)volume / MAX_AUDIO_VOLUME, 2),

        // Audio menu. Bug: if CD changer is playing, this one is always "OPEN" (even if it isn't).
        data[1] & 0x20 ? openStr : closedStr,

        (sint8_t)(data[8] & 0x7F) - 0x3F,  // Bass
        data[8] & 0x80 ? yesStr : noStr,
        (sint8_t)(data[9] & 0x7F) - 0x3F,  // Treble
        data[9] & 0x80 ? yesStr : noStr,
        data[1] & 0x10 ? onStr : offStr,  // Loudness
        (sint8_t)(0x3F) - (data[7] & 0x7F),  // Fader
        data[7] & 0x80 ? yesStr : noStr,
        (sint8_t)(0x3F) - (data[6] & 0x7F),  // Balance
        data[6] & 0x80 ? yesStr : noStr,
        data[1] & 0x04 ? onStr : offStr  // Auto volume
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAudioSettingsPkt

VanPacketParseResult_t ParseMfdStatusPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#5E4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#5E4

    const uint8_t* data = pkt.Data();
    uint16_t mfdStatus = (uint16_t)data[0] << 8 | data[1];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_status\": \"MFD_SCREEN_%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        // hmmm... MFD can also be ON if this is reported; this happens e.g. in the "minimal VAN network" test
        // setup with only the head unit (radio) and MFD. Maybe this is a status report: the MFD shows if has
        // received any packets that show connectivity to e.g. the BSI?
        data[0] == 0x00 && data[1] == 0xFF ? offStr :

        data[0] == 0x20 && data[1] == 0xFF ? onStr :
        ToHexStr(mfdStatus)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdStatusPkt

VanPacketParseResult_t ParseAirCon1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
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
        setFanSpeed >= 18 ? 7 :  // Four blades
        setFanSpeed >= 10 ? 6 :  // Three blades - high
        setFanSpeed >= 8 ? 5 :  // Three blades - low
        setFanSpeed == 7 ? 4 :  // Two blades - high
        setFanSpeed >= 5 ? 3 :  // Two blades - low
        setFanSpeed == 4 ? 2 :  // One blade (2) or all empty blades (1)
        setFanSpeed == 3 ? 1 :  // All empty blades (1)
        0;  // Fan icon not visible at all

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"ac_icon\": \"%S\",\n"
            "\"recirc\": \"%S\",\n"
            "\"rear_heater_1\": \"%S\",\n"
            "\"reported_fan_speed\": \"%u\",\n"
            "\"set_fan_speed\": \"%u\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,
        ac_icon ? onStr : offStr,
        data[0] & 0x04 ? onStr : offStr,
        rear_heater ? yesStr : noStr,
        data[4],
        setFanSpeed
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon1Pkt

VanPacketParseResult_t ParseAirCon2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
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

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"contact_key_on\": \"%S\",\n"
            "\"ac_enabled\": \"%S\",\n"
            "\"rear_heater_2\": \"%S\",\n"
            "\"ac_compressor\": \"%S\",\n"
            "\"contact_key_position_ac\": \"%S\",\n"
            "\"condenser_temperature\": \"%S\",\n"
            "\"evaporator_temperature\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[2][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,
        data[0] & 0x80 ? yesStr : noStr,
        data[0] & 0x40 ? yesStr : noStr,
        data[0] & 0x20 ? onStr : offStr,
        data[0] & 0x01 ? onStr : offStr,

        data[1] == 0x1C ? PSTR("ACC_OR_OFF") :
        data[1] == 0x18 ? PSTR("ACC-->OFF") :
        data[1] == 0x04 ? PSTR("ON-->ACC") :
        data[1] == 0x00 ? onStr :
        ToHexStr(data[1]),

        // This is not interior temperature. This rises quite rapidly if the aircon compressor is
        // running, and drops again when the aircon compressor is off. So I think this is the condenser
        // temperature.
        data[2] == 0xFF ? notApplicable2Str : FloatToStr(floatBuf[0], data[2], 0),

        FloatToStr(floatBuf[1], ((uint16_t)data[3] << 8 | data[4]) / 10.0 - 40.0, 1)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseAirCon2Pkt

VanPacketParseResult_t ParseCdChangerPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#4EC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#4EC
    // https://github.com/morcibacsi/PSAVanCanBridge/blob/master/src/Van/Structs/VanCdChangerStructs.h

    int dataLen = pkt.DataLen();
    if (dataLen == 0) return VAN_PACKET_NO_CONTENT; // "Request" packet; nothing to show
    if (dataLen != 12) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();

    bool searching = data[2] & 0x10;
    bool loading = data[2] & 0x08;
    bool ejecting = data[2] == 0xC1 && data[10] == 0;  // Ejecting cartridge

    bool cdChangerCartridgePresent = data[3] == 0x16; // "not present" is 0x06

    uint8_t trackTimeMin = data[4];
    uint8_t trackTimeSec = data[5];
    bool trackTimeInvalid = trackTimeMin == 0xFF || trackTimeSec == 0xFF;
    char trackTimeStr[7];
    if (! trackTimeInvalid) sprintf_P(trackTimeStr, PSTR("%X:%02X"), trackTimeMin, trackTimeSec);

    uint8_t totalTracks = data[8];
    bool totalTracksInvalid = totalTracks == 0xFF;
    char totalTracksStr[3];
    if (! totalTracksInvalid) sprintf_P(totalTracksStr, PSTR("%X"), totalTracks);

    uint8_t currentTrack = data[6];
    uint8_t currentDisc = data[7];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"cd_changer_present\": \"%S\",\n"
            "\"cd_changer_status_loading\": \"%S\",\n"
            "\"cd_changer_status_eject\": \"%S\",\n"
            "\"cd_changer_status_operational\": \"%S\",\n"
            "\"cd_changer_status_searching\": \"%S\",\n"
            "\"cd_changer_status\": \"%S\",\n"
            "\"cd_changer_status_pause\": \"%S\",\n"
            "\"cd_changer_status_play\": \"%S\",\n"
            "\"cd_changer_status_fast_forward\": \"%S\",\n"
            "\"cd_changer_status_rewind\": \"%S\",\n"
            "\"cd_changer_cartridge_present\": \"%S\",\n"
            "\"cd_changer_track_time\": \"%S\",\n"
            "\"cd_changer_current_track\": \"%S\",\n"
            "\"cd_changer_total_tracks\": \"%S\",\n"
            "\"cd_changer_disc_1_present\": \"%S\",\n"
            "\"cd_changer_disc_2_present\": \"%S\",\n"
            "\"cd_changer_disc_3_present\": \"%S\",\n"
            "\"cd_changer_disc_4_present\": \"%S\",\n"
            "\"cd_changer_disc_5_present\": \"%S\",\n"
            "\"cd_changer_disc_6_present\": \"%S\",\n"
            "\"cd_changer_current_disc\": \"%S\",\n"
            "\"cd_changer_random\": \"%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[2] & 0x40 ? yesStr : noStr,  // CD changer device present
        loading ? yesStr : noStr,  // Loading disc
        ejecting ? yesStr : noStr,  // Ejecting cartridge

        // Head unit powered on; CD changer operational (either standby or selected as audio source)
        data[2] & 0x80 ? yesStr : noStr,

        searching ? onStr : offStr,

        data[2] == 0x40 ? PSTR("POWER_OFF") :  // Not sure
        data[2] == 0x41 ? PSTR("POWER_ON") :  // Not sure
        data[2] == 0x49 ? PSTR("INITIALIZE") :  // Not sure
        data[2] == 0x4B ? PSTR("LOADING") :
        data[2] == 0xC0 ? PSTR("POWER_ON_READY") :  // Not sure
        data[2] == 0xC1 ? 
            data[10] == 0 ? PSTR("EJECT") :
            PSTR("PAUSE") :
        data[2] == 0xC3 ? PSTR("PLAY") :
        data[2] == 0xC4 ? PSTR("FAST_FORWARD") :
        data[2] == 0xC5 ? PSTR("REWIND") :
        data[2] == 0xD3 ?
            // "PLAY-SEARCHING" (data[2] == 0xD3) with discs found (data[10] > 0) and invalid values for currentDisc/
            // currentTrack seems to indicate an error condition, e.g. disc inserted wrong way round.
            data[10] >= 0 && currentDisc == 0xFF && currentTrack == 0xFF ? PSTR("ERROR") :
            PSTR("PLAY-SEARCHING") :
        ToHexStr(data[2]),

        (data[2] & 0x07) == 0x01 && ! ejecting && ! loading ? onStr : offStr,  // Pause
        (data[2] & 0x07) == 0x03 && ! searching && ! loading ? onStr : offStr,  // Play
        (data[2] & 0x07) == 0x04 ? onStr : offStr,  // Fast forward
        (data[2] & 0x07) == 0x05 ? onStr : offStr,  // Rewind

        cdChangerCartridgePresent ? yesStr : noStr,

        trackTimeInvalid ? PSTR("--:--") : trackTimeStr,

        currentTrack == 0xFF ? notApplicable2Str : ToBcdStr(currentTrack),
        totalTracksInvalid ? notApplicable2Str : totalTracksStr,

        data[10] & 0x01 ? yesStr : noStr,
        data[10] & 0x02 ? yesStr : noStr,
        data[10] & 0x04 ? yesStr : noStr,
        data[10] & 0x08 ? yesStr : noStr,
        data[10] & 0x10 ? yesStr : noStr,
        data[10] & 0x20 ? yesStr : noStr,

        // Pass the number only if disc is actually present: if cartridge is present without any discs, then
        // 'currentDisc' (data[7]) will be incorrectly reported as '1'.
        currentDisc == 0xFF ? notApplicable2Str :
            currentDisc == 1 && data[10] & 0x01 ? PSTR("1") :
            currentDisc == 2 && data[10] & 0x02 ? PSTR("2") :
            currentDisc == 3 && data[10] & 0x04 ? PSTR("3") :
            currentDisc == 4 && data[10] & 0x08 ? PSTR("4") :
            currentDisc == 5 && data[10] & 0x10 ? PSTR("5") :
            currentDisc == 6 && data[10] & 0x20 ? PSTR("6") :
            notApplicable2Str,

        data[1] == 0x01 ? onStr : offStr
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerPkt

VanPacketParseResult_t ParseSatNavStatus1Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#54E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#54E

    const uint8_t* data = pkt.Data();
    uint16_t status = (uint16_t)data[1] << 8 | data[2];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_status_1\": \"%S%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        // TODO - check; total guess
        status == 0x0000 ? emptyStr :
        status == 0x0001 ? PSTR("DESTINATION_NOT_ON_MAP") :
        status == 0x0020 ? ToHexStr(status) :  // Seen this but what is it?? Nearly at destination ??
        status == 0x0080 ? PSTR("READY") :
        status == 0x0101 ? ToHexStr(status) :  // Seen this but what is it??
        status == 0x0200 ? PSTR("READING_DISC_1") :
        status == 0x0220 ? PSTR("NEARLY_AT_DESTINATION") :  // TODO - guessing
        status == 0x0300 ? PSTR("IN_GUIDANCE_MODE_1") :
        status == 0x0301 ? PSTR("IN_GUIDANCE_MODE_2") :
        status == 0x0320 ? PSTR("STOPPING_GUIDANCE") :
        status == 0x0400 ? PSTR("START_OF_AUDIO_MESSAGE") :
        status == 0x0410 ? PSTR("ARRIVED_AT_DESTINATION_1") :
        status == 0x0600 ? ToHexStr(status) :  // Seen this but what is it??
        status == 0x0700 ? PSTR("INSTRUCTION_AUDIO_MESSAGE_START_1") :
        status == 0x0701 ? PSTR("INSTRUCTION_AUDIO_MESSAGE_START_2") :
        status == 0x0800 ? PSTR("END_OF_AUDIO_MESSAGE") :  // Follows 0x0400, 0x0700, 0x0701
        status == 0x4000 ? PSTR("GUIDANCE_STOPPED") :
        status == 0x4001 ? ToHexStr(status) :  // Seen this but what is it??
        status == 0x4080 ? ToHexStr(status) :  // Seen this but what is it??
        status == 0x4200 ? PSTR("ARRIVED_AT_DESTINATION_2") :
        status == 0x9000 ? PSTR("READING_DISC_2") :
        status == 0x9080 ? PSTR("START_CALCULATING_ROUTE") : // TODO - guessing
        status == 0xD001 ? ToHexStr(status) :  // Seen this but what is it??
        ToHexStr(status),

        data[4] == 0x0B ? PSTR(" 0x0B") :  // Seen with status 0x4001 and 0xD001
        data[4] == 0x0C ? PSTR(" DISC_UNREADABLE") :
        data[4] == 0x0E ? PSTR(" NO_DISC") :
        emptyStr
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus1Pkt

VanPacketParseResult_t ParseSatNavStatus2Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#7CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#7CE

    const uint8_t* data = pkt.Data();

    uint8_t satnavStatus2 = data[1] & 0x0F;

    const char* satnavStatus2Str =
        satnavStatus2 == 0x00 ? PSTR("INITIALIZING") : // TODO - change to "IDLE"
        satnavStatus2 == 0x01 ? PSTR("IDLE") : // TODO - change to "READY"
        satnavStatus2 == 0x05 ? PSTR("IN_GUIDANCE_MODE") :
        ToHexStr(satnavStatus2);

    bool satnavGuidanceActive = satnavStatus2 == 0x05;
    bool satnavDiscPresent = (data[2] & 0x70) == 0x30;

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_route_computed\": \"%S\",\n" // Report this first. TODO - explain why
            "\"satnav_status_2\": \"%S\",\n"
            "\"satnav_destination_reachable\": \"%S\",\n"
            "\"satnav_on_map\": \"%S\",\n"
            "\"satnav_download_finished\": \"%S\",\n"
            "\"satnav_disc_present\": \"%S\",\n"
            "\"satnav_gps_fix\": \"%S\",\n"
            "\"satnav_gps_fix_lost\": \"%S\",\n"
            "\"satnav_gps_scanning\": \"%S\",\n"
            "\"satnav_gps_speed\": \"%S%u\"";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[1] & 0x20 ? noStr : yesStr,

        satnavStatus2Str,

        data[1] & 0x10 ? yesStr : noStr,
        data[1] & 0x40 ? noStr : yesStr,
        data[1] & 0x80 ? yesStr : noStr,

        (data[2] & 0x70) == 0x70 ? noStr :
        (data[2] & 0x70) == 0x30 ? yesStr :
        ToHexStr((uint8_t)(data[2] & 0x70)),

        data[2] & 0x01 ? yesStr : noStr,
        data[2] & 0x02 ? yesStr : noStr,
        data[2] & 0x04 ? yesStr : noStr,

        // 0xE0 as boundary for "reverse": just guessing. Do we ever drive faster than 224 km/h?
        data[16] >= 0xE0 ? PSTR("-") : emptyStr,
        data[16] < 0xE0 ? data[16] : 0xFF - data[16] + 1
    );

    // TODO - what is this?
    uint16_t zzz = (uint16_t)data[9] << 8 | data[10];
    if (zzz != 0x00)
    {
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_zzz\": \"%u\""), zzz);
    } // if

    if (data[17] != 0x00)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR(",\n\"satnav_guidance_status\": \"%S%S%S%S%S%S%S\""),

                data[17] & 0x01 ? PSTR("LOADING_AUDIO_FRAGMENT ") : emptyStr,
                data[17] & 0x02 ? PSTR("AUDIO_OUTPUT ") : emptyStr,
                data[17] & 0x04 ? PSTR("NEW_GUIDANCE_INSTRUCTION ") : emptyStr,
                data[17] & 0x08 ? PSTR("READING_DISC ") : emptyStr,
                data[17] & 0x10 ? PSTR("CALCULATING_ROUTE ") : emptyStr,
                data[17] & 0x20 ? PSTR("DISC_PRESENT ") : emptyStr,
                data[17] & 0x80 ? PSTR("REACHED_DESTINATION ") : emptyStr
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus2Pkt

VanPacketParseResult_t ParseSatNavStatus3Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8CE

    int dataLen = pkt.DataLen();
    if (dataLen != 2 && dataLen != 3 && dataLen != 17) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    int at = 0;

    if (dataLen == 2)
    {
        uint16_t status = (uint16_t)data[0] << 8 | data[1];

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_status_3\": \"%S\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,

            // TODO - check; total guess
            status == 0x0000 ? PSTR("CALCULATING_ROUTE") :
            status == 0x0001 ? PSTR("STOPPING_NAVIGATION") :
            status == 0x0101 ? ToHexStr(status) :

            // This starts when the nag screen is accepted and is seen repeatedly when selecting a destination
            // and during guidance. It stops after a "STOPPING_NAVIGATION" status message.
            status == 0x0108 ? PSTR("SATNAV_IN_OPERATION") :

            status == 0x0110 ? PSTR("VOCAL_SYNTHESIS_LEVEL_SETTING") :
            status == 0x0120 ? PSTR("ACCEPTED_TERMS_AND_CONDITIONS") :
            status == 0x0140 ? PSTR("GPS_POS_FOUND") :
            status == 0x0306 ? PSTR("SATNAV_DISC_ID_READ") :
            status == 0x0C01 ? PSTR("CD_ROM_FOUND") :
            status == 0x0C02 ? PSTR("POWERING_OFF") :
            ToHexStr(status)
        );
    }
    else if (dataLen == 3)
    {
        // Sat nav guidance preference

        uint8_t preference = data[1];

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_guidance_preference\": \"%S\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter, SatNavGuidancePreferenceStr(preference));
    }
    else if (dataLen == 17 && data[0] == 0x20)
    {
        // Some set of ID strings. Stays the same even when the navigation CD is changed.

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"satnav_system_id\":\n"
                "[";

        at = snprintf_P(buf, n, jsonFormatter);

        char txt[VAN_MAX_DATA_BYTES - 1 + 1];  // Max 28 data bytes, minus header (1), plus terminating '\0'

        bool first = true;
        int at2 = 1;
        while (at2 < dataLen)
        {
            strncpy(txt, (const char*) data + at2, dataLen - at2);
            txt[dataLen - at2] = 0;
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("%S\n\"%s\""), first ? emptyStr : commaStr, txt);
            at2 += strlen(txt) + 1;
            first = false;
        } // while

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n]\n}\n}\n"));
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavStatus3Pkt

VanPacketParseResult_t ParseSatNavGuidanceDataPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#9CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#9CE

    const uint8_t* data = pkt.Data();

    uint16_t currHeading = (uint16_t)data[1] << 8 | data[2];  // in compass degrees (0...359)
    uint16_t headingToDestination = (uint16_t)data[3] << 8 | data[4];  // in compass degrees (0...359)
    uint16_t roadDistanceToDestination = (uint16_t)(data[5] & 0x7F) << 8 | data[6];
    bool roadDistanceToDestinationInKms = data[5] & 0x80;
    uint16_t gpsDistanceToDestination = (uint16_t)(data[7] & 0x7F) << 8 | data[8];
    bool gpsDistanceToDestinationInKms = data[7] & 0x80;
    uint16_t distanceToNextTurn = (uint16_t)(data[9] & 0x7F) << 8 | data[10];
    bool distanceToNextTurnInKms = data[9] & 0x80;
    uint16_t headingOnRoundabout = (uint16_t)data[11] << 8 | data[12];

    // TODO - Not sure, just guessing. Could also be number of instructions still to be done.
    uint16_t minutesToTravel = (uint16_t)data[13] << 8 | data[14];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_curr_heading_compass_needle\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%udeg)\"\n"
                "}\n"
            "},\n"
            "\"satnav_curr_heading\": \"%u\",\n"  // degrees
            "\"satnav_heading_to_dest_pointer\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%udeg)\"\n"
                "}\n"
            "},\n"
            "\"satnav_heading_to_dest\": \"%u\",\n"  // degrees
            "\"satnav_distance_to_dest_via_road\": \"%S %Sm\",\n"
            "\"satnav_distance_to_dest_via_straight_line\": \"%S %Sm\",\n"
            "\"satnav_turn_at\": \"%S %Sm\",\n"
            "\"satnav_heading_on_roundabout_as_text\": \"%S\",\n"  // degrees
            "\"satnav_minutes_to_travel\": \"%u\"\n"
        "}\n"
    "}\n";

    char floatBuf[4][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

        360 - currHeading, // Compass needle direction is current heading, mirrored over a vertical line
        currHeading,
        (360 - currHeading + headingToDestination) % 360, // To show pointer indicating direction relative to current heading
        headingToDestination,

        FloatToStr(floatBuf[0], roadDistanceToDestination, 0),
        roadDistanceToDestinationInKms ? PSTR("k") : emptyStr,

        FloatToStr(floatBuf[1], gpsDistanceToDestination, 0),
        gpsDistanceToDestinationInKms ? PSTR("k") : emptyStr,

        FloatToStr(floatBuf[2], distanceToNextTurn, 0),
        distanceToNextTurnInKms ? PSTR("k") : emptyStr,

        headingOnRoundabout == 0x7FFF ? notApplicable3Str : FloatToStr(floatBuf[3], headingOnRoundabout, 0),
        minutesToTravel
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavGuidanceDataPkt

VanPacketParseResult_t ParseSatNavGuidancePkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#64E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#64E

    int dataLen = pkt.DataLen();
    if (dataLen != 3 && dataLen != 4 && dataLen != 6 && dataLen != 13 && dataLen != 23)
    {
        return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;
    } // if

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_curr_turn_icon\": \"%S\",\n"
            "\"satnav_fork_icon_take_right_exit\": \"%S\",\n"
            "\"satnav_fork_icon_keep_right\": \"%S\",\n"
            "\"satnav_fork_icon_take_left_exit\": \"%S\",\n"
            "\"satnav_fork_icon_keep_left\": \"%S\",\n"
            "\"satnav_next_turn_icon\": \"%S\",\n"
            "\"satnav_turn_around_if_possible_icon\": \"%S\",\n"
            "\"satnav_follow_road_icon\": \"%S\",\n"
            "\"satnav_not_on_map_icon\": \"%S\"";

    // Determines which guidance icon(s) will be visible
    int at = snprintf_P(buf, n, jsonFormatter,
        (data[1] == 0x01 && (data[2] == 0x00 || data[2] == 0x01)) || data[1] == 0x03 ? onStr : offStr,
        data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x12 ? onStr : offStr,
        data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x14 ? onStr : offStr,
        data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x21 ? onStr : offStr,  // Never seen; just guessing
        data[1] == 0x01 && data[2] == 0x02 && data[4] == 0x41 ? onStr : offStr,
        data[1] == 0x03 ? onStr : offStr,
        data[1] == 0x04 ? onStr : offStr,
        data[1] == 0x05 ? onStr : offStr,
        data[1] == 0x06 ? onStr : offStr
    );

    if (data[1] == 0x01)  // Single turn
    {
        if (data[2] == 0x00 || data[2] == 0x01)
        {
            if (dataLen != 13) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // One instruction icon: current in data[4...11]

            GuidanceInstructionIconJson(PSTR("satnav_curr_turn_icon"), data + 4, buf, at, n);
        }
        else if (data[2] == 0x02)
        {
            if (dataLen != 6) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

            // "Fork or exit" instruction
        } // if
    }
    else if (data[1] == 0x03)  // Double turn
    {
        if (dataLen != 23) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Two instruction icons: current in data[6...13], next in data[14...21]

        GuidanceInstructionIconJson(PSTR("satnav_curr_turn_icon"), data + 6, buf, at, n);
        GuidanceInstructionIconJson(PSTR("satnav_next_turn_icon"), data + 14, buf, at, n);
    }
    else if (data[1] == 0x04)  // Turn around if possible
    {
        if (dataLen != 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // No further data in this packet
    } // if
    else if (data[1] == 0x05)  // Follow road
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        // Show one of the five available icons

        const static char jsonFormatter[] PROGMEM =
            ",\n"
            "\"satnav_follow_road_then_turn_right\": \"%S\",\n"
            "\"satnav_follow_road_then_turn_left\": \"%S\",\n"
            "\"satnav_follow_road_until_roundabout\": \"%S\",\n"
            "\"satnav_follow_road_straight_ahead\": \"%S\",\n"
            "\"satnav_follow_road_retrieving_next_instruction\": \"%S\"";

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, jsonFormatter,

                data[2] == 0x01 ? onStr : offStr,
                data[2] == 0x02 ? onStr : offStr,
                data[2] == 0x04 ? onStr : offStr,
                data[2] == 0x08 ? onStr : offStr,
                data[2] == 0x10 ? onStr : offStr
            );
    }
    else if (data[1] == 0x06)  // Not on map
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        uint16_t direction = (data[2] * 225 + 1800) % 3600;

        const static char jsonFormatter[] PROGMEM =
            ",\n"
            "\"satnav_not_on_map_follow_heading_as_text\": \"%u.%u\",\n"  // degrees
            "\"satnav_not_on_map_follow_heading\":\n"
            "{\n"
                "\"style\":\n"
                "{\n"
                    "\"transform\": \"rotate(%u.%udeg)\"\n"
                "}\n"
            "}";

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, jsonFormatter,

                direction / 10,
                direction % 10,
                direction / 10,
                direction % 10
            );
    } // if

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavGuidancePkt

VanPacketParseResult_t ParseSatNavReportPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#6CE
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#6CE

    int dataLen = pkt.DataLen();
    if (dataLen < 3) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    // Each report is a sequence of one or more VAN bus packets, so we keep a static buffer of the strings we've
    // read thus far.
    // TODO - This two-dimensional array is pretty heavy on RAM. Better to use some kind of linked list, e.g. from
    //   ESPAsyncWebServer-master\src\StringArray.h .
    #define MAX_SATNAV_STRINGS_PER_RECORD 15
    #define MAX_SATNAV_RECORDS 40
    static String records[MAX_SATNAV_RECORDS][MAX_SATNAV_STRINGS_PER_RECORD];
    static int currentRecord = 0;
    static int currentString = 0;

    // String currently being read
    #define MAX_SATNAV_STRING_SIZE 128
    static char buffer[MAX_SATNAV_STRING_SIZE];
    static int offsetInBuffer = 0;

    const uint8_t* data = pkt.Data();

    uint8_t globalSeqNo = data[0] & 0x07;
    uint8_t packetFragmentNo = data[0] >> 3 & 0x0F;
    bool lastPacket = data[0] & 0x80;

    #define INVALID_SATNAV_REPORT (0xFF)
    static uint8_t report = INVALID_SATNAV_REPORT;
    static uint8_t lastFragmentNo = 0;

    int offsetInPacket = 1;
    if (packetFragmentNo == 0)
    {
        // First fragment

        report = data[1];
        lastFragmentNo = 0;

        // Reset
        offsetInPacket = 2;
        offsetInBuffer = 0;

        // Clear all records and reset array indexes. Otherwise, if the last fragment of the previous report
        // was missed, the data will continue to pile up after the previous records.
        for (int i = 0; i < currentRecord; i++)
        {
            for (int j = 0; j < MAX_SATNAV_STRINGS_PER_RECORD; j++) records[i][j] = "";
        } // for
        currentRecord = 0;
        currentString = 0;
    }
    else
    {
        // Missed a previous fragment?
        if (report == INVALID_SATNAV_REPORT) return VAN_PACKET_PARSE_FRAGMENT_MISSED;

        uint8_t expectedFragmentNo = lastFragmentNo + 1;
        if (expectedFragmentNo > 0x0F) expectedFragmentNo = 1;  // Rolling over to 1 instead of 0??

        // Missed the expected fragment?
        if (packetFragmentNo != expectedFragmentNo)
        {
            Serial.printf_P(PSTR("--> packetFragmentNo = %u ; expectedFragmentNo = %u\n"), packetFragmentNo, expectedFragmentNo);
            report = INVALID_SATNAV_REPORT;
            return VAN_PACKET_PARSE_FRAGMENT_MISSED;
        } // if

        lastFragmentNo = packetFragmentNo;
    } // if

    while (offsetInPacket < dataLen - 1)
    {
        // New record?
        if (data[offsetInPacket] == 0x01)
        {
            offsetInPacket++;

            if (++currentRecord >= MAX_SATNAV_RECORDS)
            {
                // Warning on Serial output
                Serial.print(F("--> WARNING: too many records in satnav report!\n"));
            } // if

            currentString = 0;
            offsetInBuffer = 0;
        } // if

        int maxLen = dataLen - 1 - offsetInPacket;
        if (offsetInBuffer + maxLen >= MAX_SATNAV_STRING_SIZE)
        {
            maxLen = MAX_SATNAV_STRING_SIZE - offsetInBuffer - 1;
        } // if

        strncpy(buffer + offsetInBuffer, (const char*) data + offsetInPacket, maxLen);
        buffer[offsetInBuffer + maxLen] = 0;

        size_t bufLen = strlen(buffer);
        offsetInPacket += bufLen + 1 - offsetInBuffer;
        if (offsetInPacket <= dataLen - 1)
        {
            // Better safe than sorry
            if (currentRecord < MAX_SATNAV_RECORDS && currentString < MAX_SATNAV_STRINGS_PER_RECORD)
            {
                // Copy the current string buffer into the array
                records[currentRecord][currentString] = buffer;

                // The last character can be:
                // - 0x80: indicates that the entry cannot be selected because the current disk cannot be read.
                //   This is shown as a "?".
                // - 0x81: indicates that the entry cannot be selected because the current disk is for a different
                //   country/region.
                //   This is shown on the MFD as an circle with a bar "(-)"; here we use a circle with a cross "(X)".
                records[currentRecord][currentString].replace("\x80", "?");
                records[currentRecord][currentString].replace("\x81", "&#x24E7;");

                // Replace special characters by HTML-safe ones, e.g. "\xEB" (ë) by "&euml;"
                AsciiToHtml(records[currentRecord][currentString]);

                if (++currentString >= MAX_SATNAV_STRINGS_PER_RECORD)
                {
                    // Warning on Serial output
                    Serial.print(F("--> WARNING: too many strings in record in satnav report!\n"));
                } // if
            } // if

            offsetInBuffer = 0;
        }
        else
        {
            offsetInBuffer = bufLen;
        } // if
    } // while

    // Not last fragment?
    if ((data[0] & 0x80) == 0x00) return VAN_PACKET_NO_CONTENT;

    // Fragment was missed?
    if (report == INVALID_SATNAV_REPORT) return VAN_PACKET_PARSE_FRAGMENT_MISSED;

    // Last packet in sequence

    // Create an 'easily digestable' report in JSON format

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_report\": \"%S\"";

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(report));

    switch(report)
    {
        case SR_CURRENT_STREET:
        case SR_NEXT_STREET:
        {
            if (records[0][6].length() == 0)
            {
                const static char jsonFormatter[] PROGMEM =
                    ",\n"
                    "\"%S\": \"\"";

                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, jsonFormatter,
                        report == SR_CURRENT_STREET ? PSTR("satnav_curr_street") : PSTR("satnav_next_street")
                    );

                break;
            } // if

            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%S\": \"%s%s (%s%S%s)\"";

            // Current/next street is in first (and only) record. Copy only city [3], district [4] (if any) and
            // street [5, 6]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    report == SR_CURRENT_STREET ? PSTR("satnav_curr_street") : PSTR("satnav_next_street"),

                    // Street
                    records[0][5].c_str() + 1,  // Skip the fixed first letter 'G'
                    records[0][6].c_str(),

                    // City + optional district
                    records[0][3].c_str(),
                    records[0][4].length() == 0 ? emptyStr : PSTR(" - "),
                    records[0][4].c_str()
                );
        }
        break;

        case SR_DESTINATION:
        case SR_LAST_DESTINATION:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%S\": \"%s\",\n"
                "\"%S\": \"%s\",\n"
                "\"%S\": \"%s%S%s\",\n"
                "\"%S\": \"%s%s\",\n"
                "\"%S\": \"%S\"";

            // Address is in first (and only) record. Copy at least only city [3], district [4] (if any), street [5, 6] and
            // house number [7]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_country") :
                        PSTR("satnav_last_destination_country"),

                    // Country
                    records[0][1].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_province") :
                        PSTR("satnav_last_destination_province"),

                    // Province
                    records[0][2].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_city") :
                        PSTR("satnav_last_destination_city"),

                    // City + optional district
                    records[0][3].c_str(),
                    records[0][4].length() == 0 ? emptyStr : PSTR(" - "),
                    records[0][4].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_street") :
                        PSTR("satnav_last_destination_street"),

                    // Street
                    // Note: if the street is empty: it means "City center"
                    records[0][5].c_str() + 1,  // Skip the fixed first letter 'G'
                    records[0][6].c_str(),

                    report == SR_DESTINATION ?
                        PSTR("satnav_current_destination_house_number") :
                        PSTR("satnav_last_destination_house_number"),

                    // First string is either "C" or "V"; "C" has GPS coordinates in [7] and [8]; "V" has house number
                    // in [7]. If we see "V", show house number
                    records[0][0] == "V" && records[0][7] != "0" ?
                        records[0][7].c_str() :
                        emptyStr
                );
        }
        break;

        case SR_PERSONAL_ADDRESS:
        case SR_PROFESSIONAL_ADDRESS:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"%S\": \"%s\",\n"
                "\"%S\": \"%s\",\n"
                "\"%S\": \"%s\",\n"
                "\"%S\": \"%s%S%s\",\n"
                "\"%S\": \"%s%s\",\n"
                "\"%S\": \"%s\"";

            // Chosen address is in first (and only) record. Copy at least city [3], district [4] (if any), street [5, 6]
            // house number [7] and entry name [8]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_entry") :
                        PSTR("satnav_professional_address_entry"),

                    // Name of the entry
                    records[0][0] == "C" ? records[0][9].c_str() : records[0][8].c_str(),

                    // Address

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_country") :
                        PSTR("satnav_professional_address_country"),

                    // Country
                    records[0][1].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_province") :
                        PSTR("satnav_professional_address_province"),

                    // Province
                    records[0][2].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_city") :
                        PSTR("satnav_professional_address_city"),

                    // City + optional district
                    records[0][3].c_str(),
                    records[0][4].length() == 0 ? emptyStr : PSTR(" - "),
                    records[0][4].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_street") :
                        PSTR("satnav_professional_address_street"),

                    // Street
                    // Note: if the street is empty: it means "City center"
                    records[0][5].c_str() + 1,  // Skip the fixed first letter 'G'
                    records[0][6].c_str(),

                    report == SR_PERSONAL_ADDRESS ?
                        PSTR("satnav_personal_address_house_number") :
                        PSTR("satnav_professional_address_house_number"),

                    // First string is either "C" or "V"; "C" has GPS coordinates in [7] and [8]; "V" has house number
                    // in [7]. If we see "V", show house number
                    records[0][0] == "V" && records[0][7] != "0" ?
                        records[0][7].c_str() :
                        emptyStr
                );
        }
        break;

        case SR_SERVICE_ADDRESS:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_place_of_interest_address_entry\": \"%s\",\n"
                "\"satnav_place_of_interest_address_country\": \"%s\",\n"
                "\"satnav_place_of_interest_address_province\": \"%s\",\n"
                "\"satnav_place_of_interest_address_city\": \"%s%S%s\",\n"
                "\"satnav_place_of_interest_address_street\": \"%s%s\",\n"
                "\"satnav_place_of_interest_address_distance\": \"%s m\"";

            // Chosen place of interest address is in first (and only) record. Copy at least city [3], district [4]
            // (if any), street [5, 6], entry name [9] and distance [11]; skip the other strings.
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    // Name of the place of interest
                    records[0][9].c_str(),

                    // Address of the place of interest

                    // Country
                    records[0][1].c_str(),

                    // Province
                    records[0][2].c_str(),

                    // City + optional district
                    records[0][3].c_str(),
                    records[0][4].length() == 0 ? emptyStr : PSTR(" - "),
                    records[0][4].c_str(),

                    // Street
                    records[0][5].c_str() + 1,  // Skip the fixed first letter ('G' or 'I')
                    records[0][6].c_str(),

                    // Distance (in metres) to the place of interest
                    records[0][11].c_str()
                );
        }
        break;

        case SR_ENTER_CITY:
        case SR_ENTER_STREET:
        case SR_PERSONAL_ADDRESS_LIST:
        case SR_PROFESSIONAL_ADDRESS_LIST:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each item in the list is a single string in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%S\n\"%s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i][0].c_str()
                    );
            } // for
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        }
        break;

        case SR_ENTER_HOUSE_NUMBER:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_house_number_range\": \"From %s to %s\"";

            // Range of "house numbers" is in first (and only) record, the lowest number is in the first string, and
            // highest number is in the second string.
            // Note: "0...0" means: not applicable. MFD will follow through directly to showing the address (without a
            //   house number).
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter,

                    records[0][0].c_str(),
                    records[0][1].c_str()
                );
        }
        break;

        case SR_SERVICE_LIST:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each "category" in the list is a single string in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%S\n\"%s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i][0].c_str()
                    );
            } // for

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        }
        break;

        case SR_SOFTWARE_MODULE_VERSIONS:
        {
            const static char jsonFormatter[] PROGMEM =
                ",\n"
                "\"satnav_software_modules_list\":\n"
                "[";

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, jsonFormatter);

            // Each "module" in the list is a triplet of strings ('module_name', then 'version' and 'date' in a rather
            // free format) in a separate record
            for (int i = 0; i < currentRecord; i++)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at,
                        PSTR("%S\n\"%s - %s - %s\""),
                        i == 0 ? emptyStr : commaStr,
                        records[i][0].c_str(),
                        records[i][1].c_str(),
                        records[i][2].c_str()
                    );
            } // for

            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR("\n]"));
        } // if
        break;

    } // switch

    at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // Reset
    report = INVALID_SATNAV_REPORT;
    offsetInBuffer = 0;

    // Clear all records and reset array indexes. Otherwise, if the first fragment of the next report
    // is missed, the data will continue to pile up after the current records.
    for (int i = 0; i < currentRecord; i++)
    {
        for (int j = 0; j < MAX_SATNAV_STRINGS_PER_RECORD; j++) records[i][j] = "";
    } // for
    currentRecord = 0;
    currentString = 0;

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavReportPkt

VanPacketParseResult_t ParseMfdToSatNavPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#94E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#94E

    int dataLen = pkt.DataLen();
    if (dataLen != 4 && dataLen != 9 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

    const uint8_t* data = pkt.Data();
    uint8_t request = data[0];
    uint8_t param = data[1];
    uint8_t type = data[2];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"mfd_to_satnav_request\": \"%S\",\n"
            "\"mfd_to_satnav_request_type\": \"%S\"";

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(request), SatNavRequestTypeStr(type));

    const char* goToScreen =

        // Determine which sat nav screen must be/become visible.
        // An exmpty string ("") indicates: stay in current screen for now, trigger screen change on remote control
        // button press.

        // Combinations:

        // * request == 0x02 (SR_ENTER_CITY),
        //   param == 0x1D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request (remaining) list length
        //     -- data[3]: (next) character to narrow down selection with. 0x00 if none.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list (0-based)
        //     -- data[7] << 8 | data[8]: number of items to retrieve
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_ENTER_CITY && param == 0x1D && type == SRT_REQ_N_ITEMS ? PSTR("satnav_enter_city_characters") :
        request == SR_ENTER_CITY && param == 0x1D && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_ENTER_CITY && param == 0x1D && type == SRT_SELECT ? emptyStr :

        // * request == 0x05 (SR_ENTER_STREET),
        //   param == 0x1D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request (remaining) list length
        //     -- data[3]: (next) character to narrow down selection with. 0x00 if none.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list (0-based)
        //     -- data[7] << 8 | data[8]: number of items to retrieve
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)
        //   - type = 3 (SRT_SELECT_CITY_CENTER) (dataLen = 9): select city center
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_ENTER_STREET && param == 0x1D && type == SRT_REQ_N_ITEMS ? PSTR("satnav_enter_street_characters") :
        request == SR_ENTER_STREET && param == 0x1D && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_ENTER_STREET && param == 0x1D && type == SRT_SELECT ? emptyStr :
        request == SR_ENTER_STREET && param == 0x1D && type == SRT_SELECT_CITY_CENTER ? PSTR("satnav_show_current_destination") :

        // * request == 0x06 (SR_ENTER_HOUSE_NUMBER),
        //   param == 0x1D:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request range of house numbers
        //     -- data[7] << 8 | data[8]: always 1
        //   - type = 2 (SRT_SELECT) (dataLen = 11): enter house number
        //     -- data[5] << 8 | data[6]: entered house number. 0 if not applicable.

        request == SR_ENTER_HOUSE_NUMBER && param == 0x1D && type == SRT_REQ_ITEMS ? PSTR("satnav_current_destination_house_number") :
        request == SR_ENTER_HOUSE_NUMBER && param == 0x1D && type == SRT_SELECT ? PSTR("satnav_show_current_destination") :

        // * request == 0x08 (SR_SERVICE_LIST),
        //   param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length. Satnav will respond with 38 and no
        //              letters or number to choose from.
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_SERVICE_LIST && param == 0x0D && type == SRT_REQ_N_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_SERVICE_LIST && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x08 (SR_SERVICE_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): present nag screen. Satnav response is PLACE_OF_INTEREST_CATEGORY_LIST
        //              with list_size=38, but the MFD ignores that.
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 38)

        request == SR_SERVICE_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr :
        request == SR_SERVICE_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :

        // * request == 0x09 (SR_SERVICE_ADDRESS),
        //   param == 0x0D:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1: MFD browses address by address)

        request == SR_SERVICE_ADDRESS && param == 0x0D && type == SRT_REQ_N_ITEMS ? PSTR("satnav_choose_from_list") :
        request == SR_SERVICE_ADDRESS && param == 0x0D && type == SRT_REQ_ITEMS ? PSTR("satnav_show_place_of_interest_address") :

        // * request == 0x09 (SR_SERVICE_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_SERVICE_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x0B (SR_ARCHIVE_IN_DIRECTORY),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list of available characters
        //     -- data[3]: character entered by user. 0x00 if none.

        request == SR_ARCHIVE_IN_DIRECTORY && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr :

        // * request == 0x0B (SR_ARCHIVE_IN_DIRECTORY),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): confirm entered name
        //     -- no further data

        request == SR_ARCHIVE_IN_DIRECTORY && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x0E (SR_LAST_DESTINATION),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11):
        //     -- no further data

        request == SR_LAST_DESTINATION && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x0E (SR_LAST_DESTINATION),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9):
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_LAST_DESTINATION && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr : // PSTR("satnav_show_last_destination") :

        // * request == 0x0F (SR_NEXT_STREET),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9):
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_NEXT_STREET && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x10 (SR_CURRENT_STREET),
        //   param == 0x0D:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select current location for e.g. places of interest
        //     -- no further data

        request == SR_CURRENT_STREET && param == 0x0D && type == SRT_SELECT ? emptyStr :

        // * request == 0x10 (SR_CURRENT_STREET),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get current location during sat nav guidance
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        // Don't switch screen; the screen must either stay "satnav_guidance" in guidance mode, or
        // "satnav_current_location" if the current screen is "clock". Complicated, so let the Javascript logic
        // handle this.
        request == SR_CURRENT_STREET && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr :

        // * request == 0x11 (SR_PERSONAL_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PERSONAL_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x11 (SR_PERSONAL_ADDRESS),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PERSONAL_ADDRESS && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_show_personal_address") :

        // * request == 0x12 (SR_PROFESSIONAL_ADDRESS),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select entry from list
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PROFESSIONAL_ADDRESS && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x12 (SR_PROFESSIONAL_ADDRESS),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get entry
        //     -- data[5] << 8 | data[6]: selected entry (0-based)

        request == SR_PROFESSIONAL_ADDRESS && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_show_business_address") :

        // * request == 0x1B (SR_PERSONAL_ADDRESS_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list
        //     -- data[7] << 8 | data[8]: number of items to retrieve

        request == SR_PERSONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr : // PSTR("satnav_choose_from_list") :
        request == SR_PERSONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :

        // * request == 0x1C (SR_PROFESSIONAL_ADDRESS_LIST),
        //   param == 0xFF:
        //   - type = 0 (SRT_REQ_N_ITEMS) (dataLen = 4): request list length
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): request list 
        //     -- data[5] << 8 | data[6]: offset in list
        //     -- data[7] << 8 | data[8]: number of items to retrieve

        request == SR_PROFESSIONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_N_ITEMS ? emptyStr : // PSTR("satnav_choose_from_list") :
        request == SR_PROFESSIONAL_ADDRESS_LIST && param == 0xFF && type == SRT_REQ_ITEMS ? PSTR("satnav_choose_from_list") :

        // * request == 0x1D (SR_DESTINATION),
        //   param == 0x0E:
        //   - type = 2 (SRT_SELECT) (dataLen = 11): select fastest route ?
        //     -- no further data

        request == SR_DESTINATION && param == 0x0E && type == SRT_SELECT ? emptyStr : // PSTR("satnav_guidance") :

        // * request == 0x1D (SR_DESTINATION),
        //   param == 0xFF:
        //   - type = 1 (SRT_REQ_ITEMS) (dataLen = 9): get current destination. Satnav replies (IDEN 0x6CE) with the last
        //     destination, a city center with GPS coordinate (if no street has been entered yet), or a
        //     full address
        //     -- data[5] << 8 | data[6]: offset in list (always 0)
        //     -- data[7] << 8 | data[8]: number of items to retrieve (always 1)

        request == SR_DESTINATION && param == 0xFF && type == SRT_REQ_ITEMS ? emptyStr :

        emptyStr;

    if (strlen_P(goToScreen) > 0)
    {
        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR
                (
                    ",\n"
                    "\"mfd_to_satnav_go_to_screen\": \"%S\""
                ),

                goToScreen
            );
    } // if

    if (data[3] != 0x00)
    {
        char buffer[2];
        sprintf_P(buffer, PSTR("%c"), data[3]);

        at += at >= n ? 0 :
            snprintf_P(buf + at, n - at, PSTR
                (
                    ",\n"
                    "\"mfd_to_satnav_enter_character\": \"%s\""
                ),

                (data[3] >= 'A' && data[3] <= 'Z') || (data[3] >= '0' && data[3] <= '9') || data[3] == '\'' ? buffer :
                data[3] == ' ' ? "_" : // Space
                data[3] == 0x01 ? "Esc" :
                "?"
            );
    } // if

    if (dataLen >= 9)
    {
        uint16_t selectionOrOffset = (uint16_t)data[5] << 8 | data[6];
        uint16_t length = (uint16_t)data[7] << 8 | data[8];

        //if (selectionOrOffset > 0 && length > 0)
        if (length > 0)
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR
                    (
                        ",\n"
                        "\"mfd_to_satnav_offset\": \"%u\",\n"
                        "\"mfd_to_satnav_length\": \"%u\""
                    ),
                    selectionOrOffset,
                    length
                );
        }
        //else if (selectionOrOffset > 0)
        else
        {
            at += at >= n ? 0 :
                snprintf_P(buf + at, n - at, PSTR
                    (
                        ",\n"
                        "\"mfd_to_satnav_selection\": \"%u\""
                    ),
                    selectionOrOffset
                );
        } // if
    } // if

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdToSatNavPkt

VanPacketParseResult_t ParseSatNavToMfdPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#74E
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#74E

    const uint8_t* data = pkt.Data();
    uint8_t request = data[1];
    sint16_t listSize = (sint16_t)(data[4] << 8 | data[5]);

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"satnav_to_mfd_response\": \"%S\",\n"
            "\"satnav_to_mfd_list_size\": \"%d\",\n"
            "\"satnav_to_mfd_show_characters\": \"";  // TODO - rename to "satnav_to_mfd_available_characters"

    int at = snprintf_P(buf, n, jsonFormatter, SatNavRequestStr(request), listSize);

    // TODO - handle SR_ARCHIVE_IN_DIRECTORY

    // Available letters are bit-coded in bytes 17...20. Print the letter if it is available.
    for (int byte = 0; byte <= 3; byte++)
    {
        for (int bit = 0; bit < (byte == 3 ? 2 : 8); bit++)
        {
            if (data[byte + 17] >> bit & 0x01)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%c"), 65 + 8 * byte + bit);
            } // if
        } // for
    } // for

    if (data[21] >> 6 & 0x01)
    {
        // Special character: single quote (')
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\'"));
    } // if

    // Available numbers are bit-coded in bytes 20...21, starting with '0' at bit 2 of byte 20, ending
    // with '9' at bit 3 of byte 21. Print the number if it is available.
    for (int byte = 0; byte <= 1; byte++)
    {
        for (int bit = (byte == 0 ? 2 : 0); bit < (byte == 1 ? 4 : 8); bit++)
        {
            if (data[byte + 20] >> bit & 0x01)
            {
                at += at >= n ? 0 :
                    snprintf_P(buf + at, n - at, PSTR("%c"), 48 + 8 * byte + bit - 2);
            } // if
        } // for
    } // for

    if (data[22] >> 1 & 0x01)
    {
        // <Space>, will be shown as '_'
        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("_"));
    } // if

    at += at >= n ? 0 :
        snprintf_P(buf + at, n - at, PSTR("\"\n}\n}\n"));

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseSatNavToMfdPkt

VanPacketParseResult_t ParseWheelSpeedPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#744

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"wheel_speed_rear_right\": \"%s\",\n"
            "\"wheel_speed_rear_left\": \"%s\",\n"
            "\"wheel_pulses_rear_right\": \"%u\",\n"
            "\"wheel_pulses_rear_left\": \"%u\"\n"
        "}\n"
    "}\n";

    char floatBuf[2][MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        FloatToStr(floatBuf[0], ((uint16_t)data[0] << 8 | data[1]) / 100.0, 2),
        FloatToStr(floatBuf[1], ((uint16_t)data[2] << 8 | data[3]) / 100.0, 2),
        (uint16_t)data[4] << 8 | data[5],
        (uint16_t)data[6] << 8 | data[7]
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseWheelSpeedPkt

VanPacketParseResult_t ParseOdometerPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8FC
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8FC

    const uint8_t* data = pkt.Data();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"odometer_2\": \"%s\"\n"
        "}\n"
    "}\n";

    char floatBuf[MAX_FLOAT_SIZE];

    int at = snprintf_P(buf, n, jsonFormatter,
        FloatToStr(floatBuf, ((uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3]) / 10.0, 1)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseOdometerPkt

VanPacketParseResult_t ParseCom2000Pkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#450

    const uint8_t* data = pkt.Data();

    // TODO - replace event "display" by "button_press"; JavaScript on served website could react by changing to
    // different screen or displaying popup
    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"com2000_light_switch_auto\": \"%S\",\n"
            "\"com2000_light_switch_fog_light_forward\": \"%S\",\n"
            "\"com2000_light_switch_fog_light_backward\": \"%S\",\n"
            "\"com2000_light_switch_signal_beam\": \"%S\",\n"
            "\"com2000_light_switch_full_beam\": \"%S\",\n"
            "\"com2000_light_switch_all_off\": \"%S\",\n"
            "\"com2000_light_switch_side_lights\": \"%S\",\n"
            "\"com2000_light_switch_low_beam\": \"%S\",\n"
            "\"com2000_right_stalk_button_trip_computer\": \"%S\",\n"
            "\"com2000_right_stalk_rear_window_wash\": \"%S\",\n"
            "\"com2000_right_stalk_rear_window_wiper\": \"%S\",\n"
            "\"com2000_right_stalk_windscreen_wash\": \"%S\",\n"
            "\"com2000_right_stalk_windscreen_wipe_once\": \"%S\",\n"
            "\"com2000_right_stalk_windscreen_wipe_auto\": \"%S\",\n"
            "\"com2000_right_stalk_windscreen_wipe_normal\": \"%S\",\n"
            "\"com2000_right_stalk_windscreen_wipe_fast\": \"%S\",\n"
            "\"com2000_turn_signal_left\": \"%S\",\n"
            "\"com2000_turn_signal_right\": \"%S\",\n"
            "\"com2000_head_unit_stalk_button_src\": \"%S\",\n"
            "\"com2000_head_unit_stalk_button_volume_up\": \"%S\",\n"
            "\"com2000_head_unit_stalk_button_volume_down\": \"%S\",\n"
            "\"com2000_head_unit_stalk_button_seek_backward\": \"%S\",\n"
            "\"com2000_head_unit_stalk_button_seek_forward\": \"%S\",\n"
            "\"com2000_head_unit_stalk_wheel_pos\": \"%d\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        data[1] & 0x01 ? onStr : offStr,
        data[1] & 0x02 ? onStr : offStr,
        data[1] & 0x04 ? onStr : offStr,
        data[1] & 0x08 ? onStr : offStr,
        data[1] & 0x10 ? onStr : offStr,
        data[1] & 0x20 ? onStr : offStr,
        data[1] & 0x40 ? onStr : offStr,
        data[1] & 0x80 ? onStr : offStr,

        data[2] & 0x01 ? onStr : offStr,
        data[2] & 0x02 ? onStr : offStr,
        data[2] & 0x04 ? onStr : offStr,
        data[2] & 0x08 ? onStr : offStr,
        data[2] & 0x10 ? onStr : offStr,
        data[2] & 0x20 ? onStr : offStr,
        data[2] & 0x40 ? onStr : offStr,
        data[2] & 0x80 ? onStr : offStr,

        data[3] & 0x40 ? onStr : offStr,
        data[3] & 0x80 ? onStr : offStr,

        data[5] & 0x02 ? onStr : offStr,
        data[5] & 0x03 ? onStr : offStr,
        data[5] & 0x08 ? onStr : offStr,
        data[5] & 0x40 ? onStr : offStr,
        data[5] & 0x80 ? onStr : offStr,

        (sint8_t)data[6]
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCom2000Pkt

VanPacketParseResult_t ParseCdChangerCmdPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8EC

    const uint8_t* data = pkt.Data();
    uint16_t cdcCommand = (uint16_t)data[0] << 8 | data[1];

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"cd_changer_command\": \"%S\"\n"
        "}\n"
    "}\n";

    int at = snprintf_P(buf, n, jsonFormatter,

        cdcCommand == 0x1101 ? PSTR("POWER_OFF") :
        cdcCommand == 0x2101 ? PSTR("POWER_OFF") :
        cdcCommand == 0x1181 ? PSTR("PAUSE") :
        cdcCommand == 0x1183 ? PSTR("PLAY") :
        cdcCommand == 0x31FE ? PSTR("PREVIOUS_TRACK") :
        cdcCommand == 0x31FF ? PSTR("NEXT_TRACK") :
        cdcCommand == 0x4101 ? PSTR("CD_1") :
        cdcCommand == 0x4102 ? PSTR("CD_2") :
        cdcCommand == 0x4103 ? PSTR("CD_3") :
        cdcCommand == 0x4104 ? PSTR("CD_4") :
        cdcCommand == 0x4105 ? PSTR("CD_5") :
        cdcCommand == 0x4106 ? PSTR("CD_6") :
        cdcCommand == 0x41FE ? PSTR("PREVIOUS_CD") :
        cdcCommand == 0x41FF ? PSTR("NEXT_CD") :
        ToHexStr(cdcCommand)
    );

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseCdChangerCmdPkt

VanPacketParseResult_t ParseMfdToHeadUnitPkt(const char* idenStr, TVanPacketRxDesc& pkt, char* buf, const int n)
{
    // http://graham.auld.me.uk/projects/vanbus/packets.html#8D4
    // http://pinterpeti.hu/psavanbus/PSA-VAN.html#8D4

    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();
    int at = 0;

    // Maybe this is in fact "Head unit to MFD"??

    if (data[0] == 0x11)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_audio_bits_mute\": \"%S\",\n"
                "\"head_unit_update_audio_bits_auto_volume\": \"%S\",\n"
                "\"head_unit_update_audio_bits_loudness\": \"%S\",\n"
                "\"head_unit_update_audio_bits_audio_menu\": \"%S\",\n"
                "\"head_unit_update_audio_bits_power\": \"%S\",\n"
                "\"head_unit_update_audio_bits_contact_key\": \"%S\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] & 0x01 ? onStr : offStr,
            data[1] & 0x02 ? onStr : offStr,
            data[1] & 0x10 ? onStr : offStr,

            // Bug: if CD changer is playing, this one is always "OPEN"...
            data[1] & 0x20 ? openStr : closedStr,

            data[1] & 0x40 ? onStr : offStr,
            data[1] & 0x80 ? onStr : offStr
        );
    }
    else if (data[0] == 0x12)
    {
        if (dataLen != 2 && dataLen != 11) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_switch_to\": \"%S\"";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] == 0x01 ? PSTR("TUNER") :
            data[1] == 0x02 ? PSTR("INTERNAL_CD_OR_TAPE") :
            data[1] == 0x03 ? PSTR("CD_CHANGER") :

            // This is the "default" mode for the head unit, to sit there and listen to the navigation
            // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
            // whenever this source is chosen.
            data[1] == 0x05 ? PSTR("NAVIGATION") :

            ToHexStr(data[1])
        );

        if (dataLen == 11)
        {
            const static char jsonFormatter2[] PROGMEM = ",\n"
                "\"head_unit_update_power\": \"%S\",\n"
                "\"head_unit_update_source\": \"%S\",\n"
                "\"head_unit_update_volume_1\": \"%u%S\",\n"
                "\"head_unit_update_balance\": \"%d%S\",\n"
                "\"head_unit_update_fader\": \"%d%S\",\n"
                "\"head_unit_update_bass\": \"%d%S\",\n"
                "\"head_unit_update_treble\": \"%d%S\"";

            at += at >= n ? 0 : snprintf_P(buf + at, n - at, jsonFormatter2,

                data[2] & 0x01 ? onStr : offStr,

                (data[4] & 0x0F) == 0x00 ? noneStr :  // source
                (data[4] & 0x0F) == 0x01 ? PSTR("TUNER") :
                (data[4] & 0x0F) == 0x02 ? PSTR("INTERNAL_CD_OR_TAPE") :
                (data[4] & 0x0F) == 0x03 ? PSTR("CD_CHANGER") :

                // This is the "default" mode for the head unit, to sit there and listen to the navigation
                // audio. The navigation audio volume is also always set (usually a lot higher than the radio)
                // whenever this source is chosen.
                (data[4] & 0x0F) == 0x05 ? PSTR("NAVIGATION") :

                ToHexStr((uint8_t)(data[4] & 0x0F)),

                data[5] & 0x7F,
                data[5] & 0x80 ? updatedStr : emptyStr,
                (sint8_t)(0x3F) - (data[6] & 0x7F),
                data[6] & 0x80 ? updatedStr : emptyStr,
                (sint8_t)(0x3F) - (data[7] & 0x7F),
                data[7] & 0x80 ? updatedStr : emptyStr,
                (sint8_t)(data[8] & 0x7F) - 0x3F,
                data[8] & 0x80 ? updatedStr : emptyStr,
                (sint8_t)(data[9] & 0x7F) - 0x3F,
                data[9] & 0x80 ? updatedStr : emptyStr
            );
        } // if

        at += at >= n ? 0 : snprintf_P(buf + at, n - at, PSTR("\n}\n}\n"));
    }
    else if (data[0] == 0x13)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_volume_2\": \"%u(%S%S)\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] & 0x1F,
            data[1] & 0x40 ? PSTR("relative: ") : PSTR("absolute"),
            data[1] & 0x40 ?
                data[1] & 0x20 ? PSTR("decrease") : PSTR("increase") :
                emptyStr
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

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_update_audio_levels_balance\": \"%d\",\n"
                "\"head_unit_update_audio_levels_fader\": \"%d\",\n"
                "\"head_unit_update_audio_levels_bass\": \"%d\",\n"
                "\"head_unit_update_audio_levels_treble\": \"%d\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            (sint8_t)(0x3F) - (data[1] & 0x7F),
            (sint8_t)(0x3F) - data[2],
            (sint8_t)data[3] - 0x3F,
            (sint8_t)data[4] - 0x3F
        );
    }
    else if (data[0] == 0x27)
    {
        if (dataLen != 2) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_preset_request_band\": \"%S\",\n"
                "\"head_unit_preset_request_memory\": \"%u\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            TunerBandStr(data[1] >> 4 & 0x07),
            data[1] & 0x0F
        );
    }
    else if (data[0] == 0x61)
    {
        if (dataLen != 4) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_cd_request\": \"%S\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter,
            data[1] == 0x02 ? PSTR("PAUSE") :
            data[1] == 0x03 ? PSTR("PLAY") :
            data[3] == 0xFF ? PSTR("NEXT") :
            data[3] == 0xFE ? PSTR("PREVIOUS") :
            ToHexStr(data[3])
        );
    }
    else if (data[0] == 0xD1)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_tuner_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD2)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_tape_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else if (data[0] == 0xD6)
    {
        if (dataLen != 1) return VAN_PACKET_PARSE_UNEXPECTED_LENGTH;

        const static char jsonFormatter[] PROGMEM =
        "{\n"
            "\"event\": \"display\",\n"
            "\"data\":\n"
            "{\n"
                "\"head_unit_cd_track_info_request\": \"REQUEST\"\n"
            "}\n"
        "}\n";

        at = snprintf_P(buf, n, jsonFormatter);
    }
    else
    {
        return VAN_PACKET_PARSE_TO_BE_DECODED;
    } // if

    // JSON buffer overflow?
    if (at >= n) return VAN_PACKET_PARSE_JSON_TOO_LONG;

    return VAN_PACKET_PARSE_OK;
} // ParseMfdToHeadUnitPkt

// Check if the new packet data differs from the previous.
// Optionally, print the new packet on Serial, highlighting the bytes that differ.
bool IsPacketDataDuplicate(TVanPacketRxDesc& pkt, IdenHandler_t* handler)
{
    uint16_t iden = pkt.Iden();
    int dataLen = pkt.DataLen();
    const uint8_t* data = pkt.Data();

    // Relying on short-circuit boolean evaluation
    if (handler->prevData != NULL && memcmp(data, handler->prevData, dataLen) == 0) return true;  // Duplicate packet

    if ((serialDumpFilter == 0 || iden == serialDumpFilter) && IsPacketSelected(iden, SELECTED_PACKETS))
    {
        Serial.printf_P(PSTR("---> Received: %s packet (0x%03X)\n"), handler->idenStr, iden);

        // The first time, handler->prevData will be NULL, so only the "FULL: " line will be printed
        if (handler->prevData != NULL)
        {
            // First line: print the new packet's data where it differs from the previous packet
            Serial.printf_P(PSTR("DIFF: 0x%03X (%s) "), iden, handler->idenStr);
            for (int i = 0; i < dataLen; i++)
            {
                char diffByte[3] = "  ";
                if (data[i] != handler->prevData[i])
                {
                    snprintf_P(diffByte, sizeof(diffByte), PSTR("%02X"), handler->prevData[i]);
                } // if
                Serial.printf_P(PSTR("%s%c"), diffByte, i < dataLen - 1 ? '-' : '\n');
            } // for
        } // if
    } // if

    if (handler->prevData == NULL) handler->prevData = (uint8_t*) malloc(VAN_MAX_DATA_BYTES);

    if (handler->prevData != NULL)
    {
        // Save packet data to compare against at next packet reception
        memset(handler->prevData, 0, VAN_MAX_DATA_BYTES);
        memcpy(handler->prevData, data, dataLen);
    } // if

    if ((serialDumpFilter == 0 || iden == serialDumpFilter) && IsPacketSelected(iden, SELECTED_PACKETS))
    {
        // Now print the new packet's data in full
        Serial.printf_P(PSTR("FULL: 0x%03X (%s) "), iden, handler->idenStr);
        for (int i = 0; i < dataLen; i++) Serial.printf_P(PSTR("%02X%c"), data[i], i < dataLen - 1 ? '-' : ' ');
        Serial.println();
    } // if

    return false;
} // IsPacketDataDuplicate

// Pretty-print a JSON formatted string, adding indentation
void PrintJsonText(const char* jsonBuffer)
{
    // Number of spaces to add for each indentation level
    #define PRETTY_PRINT_JSON_INDENT 2

    size_t j = 0;
    int indent = 0;
    while (j < strlen(jsonBuffer))
    {
        const char* subString = jsonBuffer + j;

        if (subString[0] == '}' || subString[0] == ']') indent -= PRETTY_PRINT_JSON_INDENT;

        size_t n = strcspn(subString, "\n");
        if (n != strlen(subString)) Serial.printf("%*s%.*s\n", indent, "", n, subString);
        j = j + n + 1;

        if (subString[0] == '{' || subString[0] == '[') indent += PRETTY_PRINT_JSON_INDENT;
    } // while
} // PrintJsonText

static IdenHandler_t handlers[] =
{
    // Columns:
    // IDEN value, IDEN string, number of expected bytes (or -1 if varying/unknown), handler function
    { VIN_IDEN, "vin", 17, &ParseVinPkt },
    { ENGINE_IDEN, "engine", 7, &ParseEnginePkt },
    { HEAD_UNIT_STALK_IDEN, "head_unit_stalk", 2, &ParseHeadUnitStalkPkt },
    { LIGHTS_STATUS_IDEN, "lights_status", -1, &ParseLightsStatusPkt },
    { DEVICE_REPORT, "device_report", -1, &ParseDeviceReportPkt },
    { CAR_STATUS1_IDEN, "car_status_1", 27, &ParseCarStatus1Pkt },
    { CAR_STATUS2_IDEN, "car_status_2", -1, &ParseCarStatus2Pkt },
    { DASHBOARD_IDEN, "dashboard", 7, &ParseDashboardPkt },
    { DASHBOARD_BUTTONS_IDEN, "dashboard_buttons", -1, &ParseDashboardButtonsPkt },
    { HEAD_UNIT_IDEN, "head_unit", -1, &ParseHeadUnitPkt },
    { TIME_IDEN, "time", 5, &ParseTimePkt },
    { AUDIO_SETTINGS_IDEN, "audio_settings", 11, &ParseAudioSettingsPkt },
    { MFD_STATUS_IDEN, "mfd_status", 2, &ParseMfdStatusPkt },
    { AIRCON1_IDEN, "aircon_1", 5, &ParseAirCon1Pkt },
    { AIRCON2_IDEN, "aircon_2", 7, &ParseAirCon2Pkt },
    { CDCHANGER_IDEN, "cd_changer", -1, &ParseCdChangerPkt },
    { SATNAV_STATUS_1_IDEN, "satnav_status_1", 6, &ParseSatNavStatus1Pkt },
    { SATNAV_STATUS_2_IDEN, "satnav_status_2", 20, &ParseSatNavStatus2Pkt },
    { SATNAV_STATUS_3_IDEN, "satnav_status_3", -1, &ParseSatNavStatus3Pkt },
    { SATNAV_GUIDANCE_DATA_IDEN, "satnav_guidance_data", 16, &ParseSatNavGuidanceDataPkt },
    { SATNAV_GUIDANCE_IDEN, "satnav_guidance", -1, &ParseSatNavGuidancePkt },
    { SATNAV_REPORT_IDEN, "satnav_report", -1, &ParseSatNavReportPkt },
    { MFD_TO_SATNAV_IDEN, "mfd_to_satnav", -1, &ParseMfdToSatNavPkt },
    { SATNAV_TO_MFD_IDEN, "satnav_to_mfd", 27, &ParseSatNavToMfdPkt },
    { WHEEL_SPEED_IDEN, "wheel_speed", 5, &ParseWheelSpeedPkt },
    { ODOMETER_IDEN, "odometer", 5, &ParseOdometerPkt },
    { COM2000_IDEN, "com2000", 10, &ParseCom2000Pkt },
    { CDCHANGER_COMMAND_IDEN, "cd_changer_command", 2, &ParseCdChangerCmdPkt },
    { MFD_TO_HEAD_UNIT_IDEN, "display_to_head_unit", -1, &ParseMfdToHeadUnitPkt },
#if 0
    { AIR_CONDITIONER_DIAG_IDEN, "aircon_diag", -1, &DefaultPacketParser },
    { AIR_CONDITIONER_DIAG_COMMAND_IDEN, "aircon_diag_command", -1, &DefaultPacketParser },
#endif
}; // handlers

const IdenHandler_t* const handlers_end = handlers + sizeof(handlers) / sizeof(handlers[0]);

const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt)
{
    if (! pkt.CheckCrcAndRepair())
    {
        Serial.print(F("VAN PACKET CRC ERROR!\n"));

        // Show byte content of packet
        pkt.DumpRaw(Serial);

        #ifdef VAN_RX_ISR_DEBUGGING
        // Fully dump bit timings for packets that have CRC ERROR, for further analysis
        pkt.getIsrDebugPacket().Dump(Serial);
        #endif // VAN_RX_ISR_DEBUGGING

        return ""; // CRC error
    } // if

    int dataLen = pkt.DataLen();
    if (dataLen < 0 || dataLen > VAN_MAX_DATA_BYTES) return ""; // Unexpected packet length

    uint16_t iden = pkt.Iden();
    IdenHandler_t* handler = handlers;

    // Search the correct handler. Relying on short-circuit boolean evaluation.
    while (handler != handlers_end && handler->iden != iden) handler++;

    // Hander found?
    if (handler == handlers_end) return ""; // Unrecognized IDEN value

    if (handler->dataLen >= 0 && dataLen != handler->dataLen) return ""; // Unexpected packet length

    // Only process if packet content differs from previous packet
    // TODO - specify handling modes for duplicate packets in handlers table: not all duplicate packets should
    //   be ignored.
    if (IsPacketDataDuplicate(pkt, handler)) return "";

    int result = handler->parser(handler->idenStr, pkt, jsonBuffer, JSON_BUFFER_SIZE);

    if (result == VAN_PACKET_PARSE_JSON_TOO_LONG)
    {
        Serial.print(F("--> WARNING: JSON BUFFER OVERFLOW!\n"));

        // No use to return the JSON buffer; it is invalid
        return ""; // result != VAN_PACKET_PARSE_OK
    } // if

    if (result != VAN_PACKET_PARSE_OK) return ""; // Parsing result not OK

    #ifdef PRINT_JSON_BUFFERS_ON_SERIAL

    if ((serialDumpFilter == 0 || iden == serialDumpFilter) && IsPacketSelected(iden, SELECTED_PACKETS))
    {
        Serial.print(F("Parsed to JSON object:\n"));
        PrintJsonText(jsonBuffer);
    } // if

    #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return jsonBuffer;
} // ParseVanPacketToJson
