
#include "Config.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const char* GetHostname()
{
    return HOST_NAME;
} // GetHostname

void SetupWifi()
{
    Serial.printf_P(PSTR("Connecting to Wi-Fi SSID '%s' "), ssid);

    WifiConfig();

    WiFi.hostname(GetHostname());

    // TODO - does this decrease the jitter on the bit timings?
  #ifdef ARDUINO_ARCH_ESP32
    WiFi.setSleep(WIFI_PS_NONE);
  #else // ! ARDUINO_ARCH_ESP32
    wifi_set_sleep_type(NONE_SLEEP_T);
  #endif // ARDUINO_ARCH_ESP32

    WiFi.mode(WIFI_STA);  // Otherwise it may be in WIFI_AP_STA mode, broadcasting an SSID like AI_THINKER_XXXXXX
    WiFi.disconnect();  // After reset via HW button sometimes cannot seem to reconnect without this
    WiFi.persistent(false);
  #ifdef ARDUINO_ARCH_ESP32
   #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.setAutoReconnect(true);
   #else // ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    WiFi.setAutoConnect(true);
   #endif // ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #else // ! ARDUINO_ARCH_ESP32
    WiFi.setAutoConnect(true);
  #endif // ARDUINO_ARCH_ESP32

  #ifndef ARDUINO_ARCH_ESP32
    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.setOutputPower(20.5);
  #endif // ARDUINO_ARCH_ESP32

    // TODO - using Wi-Fi, unfortunately, has a detrimental effect on the packet CRC error rate. It will rise from
    // around 0.006% up to 0.1% or more. Underlying cause is timing failures due to Wi-Fi causing varying interrupt
    // latency. Not sure how to tackle this.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    } // while
    Serial.print(F(" OK\n"));

    Serial.printf_P(PSTR("Wi-Fi signal strength (RSSI): %d dB\n"), WiFi.RSSI());

    delay(1);
} // SetupWifi
