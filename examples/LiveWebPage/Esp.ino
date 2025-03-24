
// ESP system data

const char PROGMEM qioStr[] = "QIO";
const char PROGMEM qoutStr[] = "QOUT";
const char PROGMEM dioStr[] = "DIO";
const char PROGMEM doutStr[] = "DOUT";
const char PROGMEM unknownStr[] = "UNKNOWN";

void PrintSystemSpecs()
{
  #ifdef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("CPU Speed: %" PRIu32 " MHz (CPU_F_FACTOR = %ld)\n"), ESP.getCpuFreqMHz(), CPU_F_FACTOR);
  #else // ! ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("CPU Speed: %u MHz (CPU_F_FACTOR = %ld)\n"), ESP.getCpuFreqMHz(), CPU_F_FACTOR);
  #endif // ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("SDK: %s\n"), ESP.getSdkVersion());

  #ifndef ARDUINO_ARCH_ESP32
    uint32_t realSize = ESP.getFlashChipRealSize();
  #endif // ARDUINO_ARCH_ESP32
    uint32_t ideSize = ESP.getFlashChipSize();

    char floatBuf[MAX_FLOAT_SIZE];
  #ifndef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash real size: %s MBytes\n"), FloatToStr(floatBuf, realSize/1024.0/1024.0, 2));
  #endif // ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash ide size: %s MBytes\n"), FloatToStr(floatBuf, ideSize/1024.0/1024.0, 2));
    Serial.printf_P(PSTR("Flash ide speed: %s MHz\n"), FloatToStr(floatBuf, ESP.getFlashChipSpeed()/1000000.0, 2));
    FlashMode_t ideMode = ESP.getFlashChipMode();
    Serial.printf_P(PSTR("Flash ide mode: %s\n"),
        ideMode == FM_QIO ? qioStr :
        ideMode == FM_QOUT ? qoutStr :
        ideMode == FM_DIO ? dioStr :
        ideMode == FM_DOUT ? doutStr :
        unknownStr);
  #ifndef ARDUINO_ARCH_ESP32
    Serial.printf_P(PSTR("Flash chip configuration %s\n"), ideSize != realSize ? PSTR("wrong!") : PSTR("ok."));
  #endif // ARDUINO_ARCH_ESP32

    Serial.print(F("Wi-Fi MAC address: "));
    Serial.print(WiFi.macAddress());
    Serial.print("\n");
} // PrintSystemSpecs

const char* EspSystemDataToJson(char* buf, const int n)
{
  #ifndef ARDUINO_ARCH_ESP32
    uint32_t flashSizeReal = ESP.getFlashChipRealSize();
  #endif // ARDUINO_ARCH_ESP32
    uint32_t flashSizeIde = ESP.getFlashChipSize();
    FlashMode_t flashModeIde = ESP.getFlashChipMode();

    const static char jsonFormatter[] PROGMEM =
    "{\n"
        "\"event\": \"display\",\n"
        "\"data\":\n"
        "{\n"
          #ifndef ARDUINO_ARCH_ESP32
            "\"esp_last_reset_reason\": \"%s\",\n"
            "\"esp_last_reset_info\": \"%s\",\n"
            "\"esp_boot_version\": \"%u\",\n"
          #endif // ARDUINO_ARCH_ESP32
          #ifdef ARDUINO_ARCH_ESP32
            "\"esp_cpu_speed\": \"%" PRIu32 " MHz\",\n"
          #else // ! ARDUINO_ARCH_ESP32
            "\"esp_cpu_speed\": \"%u MHz\",\n"
          #endif // ARDUINO_ARCH_ESP32
            "\"esp_sdk_version\": \"%s\",\n"
          #ifndef ARDUINO_ARCH_ESP32
            "\"esp_chip_id\": \"0x%08X\",\n"
            "\"esp_flash_id\": \"0x%08X\",\n"
            "\"esp_flash_size_real\": \"%s MBytes\",\n"
          #endif // ARDUINO_ARCH_ESP32
            "\"esp_flash_size_ide\": \"%s MBytes\",\n"
            "\"esp_flash_speed_ide\": \"%s MHz\",\n"

            "\"esp_flash_mode_ide\": \"%s\",\n"

            "\"esp_mac_address\": \"%s\",\n"
            "\"esp_ip_address\": \"%s\",\n"
            "\"esp_wifi_rssi\": \"%d dB\",\n"

            "\"esp_free_ram\": \"%" PRIu32 " bytes\"\n"
        "}\n"
    "}\n";

    char floatBuf[3][MAX_FLOAT_SIZE];
    int at = snprintf_P(buf, n, jsonFormatter,

      #ifndef ARDUINO_ARCH_ESP32
        ESP.getResetReason().c_str(),
        ESP.getResetInfo().c_str(),

        ESP.getBootVersion(),
      #endif // ARDUINO_ARCH_ESP32
        ESP.getCpuFreqMHz(),
        ESP.getSdkVersion(),
      #ifndef ARDUINO_ARCH_ESP32
        ESP.getChipId(),

        ESP.getFlashChipId(),
        FloatToStr(floatBuf[0], flashSizeReal/1024.0/1024.0, 2),
      #endif // ARDUINO_ARCH_ESP32
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

      #ifdef ARDUINO_ARCH_ESP32
        esp_get_free_heap_size()
      #else
        system_get_free_heap_size()
      #endif // ARDUINO_ARCH_ESP32
    );

    // JSON buffer overflow?
    if (at >= n) return "";

  #ifdef PRINT_JSON_BUFFERS_ON_SERIAL
    Serial.print(F("Parsed to JSON object:\n"));
    PrintJsonText(buf);
  #endif // PRINT_JSON_BUFFERS_ON_SERIAL

    return buf;
} // EspSystemDataToJson
