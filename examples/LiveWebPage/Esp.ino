
// ESP system data

const char PROGMEM qioStr[] = "QIO";
const char PROGMEM qoutStr[] = "QOUT";
const char PROGMEM dioStr[] = "DIO";
const char PROGMEM doutStr[] = "DOUT";
const char PROGMEM unknownStr[] = "UNKNOWN";

void printSystemSpecs()
{
    Serial.printf_P(PSTR("CPU Speed: %u MHz\n"), system_get_cpu_freq());
    Serial.printf_P(PSTR("SDK: %s\n"), system_get_sdk_version());

    uint32_t realSize = ESP.getFlashChipRealSize();
    uint32_t ideSize = ESP.getFlashChipSize();

    char floatBuf[MAX_FLOAT_SIZE];
    Serial.printf_P(PSTR("Flash real size: %s MBytes\n"), FloatToStr(floatBuf, realSize/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide size: %s MBytes\n"), FloatToStr(floatBuf, ideSize/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide speed: %s MHz\n"), FloatToStr(floatBuf, ESP.getFlashChipSpeed()/1000000.0, 2));
    FlashMode_t ideMode = ESP.getFlashChipMode();
    Serial.printf_P(PSTR("Flash ide mode: %S\n"),
        ideMode == FM_QIO ? qioStr :
        ideMode == FM_QOUT ? qoutStr :
        ideMode == FM_DIO ? dioStr :
        ideMode == FM_DOUT ? doutStr :
        unknownStr);
    Serial.printf_P(PSTR("Flash chip configuration %S\n"), ideSize != realSize ? PSTR("wrong!") : PSTR("ok."));
} // printSystemSpecs

const char* espDataToJson()
{
    #define ESP_DATA_JSON_BUFFER_SIZE 1024
    static char jsonBuffer[ESP_DATA_JSON_BUFFER_SIZE];

    uint32_t flashSizeReal = ESP.getFlashChipRealSize();
    uint32_t flashSizeIde = ESP.getFlashChipSize();
    FlashMode_t flashModeIde = ESP.getFlashChipMode();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
            "\"esp_boot_version\": \"%u\",\n"
            "\"esp_cpu_speed\": \"%u MHz\",\n"
            "\"esp_sdk_version\": \"%s\",\n"
            "\"esp_chip_id\": \"0x%08X\",\n"

            "\"esp_flash_id\": \"0x%08X\",\n"
            "\"esp_flash_size_real\": \"%s MBytes\",\n"
            "\"esp_flash_size_ide\": \"%s MBytes\",\n"
            "\"esp_flash_speed_ide\": \"%s MHz\",\n"

            "\"esp_flash_mode_ide\": \"%S\",\n"

            "\"esp_mac_address\": \"%s\",\n"
            "\"esp_ip_address\": \"%s\",\n"
            "\"esp_wifi_rssi\": \"%d dB\",\n"

            "\"esp_free_ram\": \"%u bytes\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(jsonBuffer, ESP_DATA_JSON_BUFFER_SIZE, jsonFormatter,

        ESP.getBootVersion(),
        ESP.getCpuFreqMHz(), // system_get_cpu_freq(),
        ESP.getSdkVersion(),
        ESP.getChipId(),

        ESP.getFlashChipId(),
        FloatToStr(floatBuf[0], flashSizeReal/1024.0/1024.0, 2),
        FloatToStr(floatBuf[1], flashSizeIde/1024.0/1024.0, 2),
        FloatToStr(floatBuf[2], ESP.getFlashChipSpeed()/1000000.0, 2),

        flashModeIde == FM_QIO ? qioStr :
        flashModeIde == FM_QOUT ? qoutStr :
        flashModeIde == FM_DIO ? dioStr :
        flashModeIde == FM_DOUT ? doutStr :
        unknownStr,

        WiFi.macAddress().c_str(),
        WiFi.localIP().toString().c_str(),
        WiFi.RSSI(),

        system_get_free_heap_size()
    );

    // JSON buffer overflow?
    if (at >= ESP_DATA_JSON_BUFFER_SIZE) return "";

    #ifdef PRINT_JSON_BUFFERS_ON_SERIAL

    Serial.print(F("ESP data as JSON object:\n"));
    PrintJsonText(jsonBuffer);

    #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return jsonBuffer;
} // espDataToJson

