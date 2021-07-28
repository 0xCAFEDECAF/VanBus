
#include "Config.h"

char* ssid = WIFI_SSID;
char* password = WIFI_PASSWORD;

const char* GetHostname()
{
    return "Car";
} // GetHostname

void SetupWifi()
{
    Serial.printf_P(PSTR("Connecting to Wi-Fi SSID '%s' "), ssid);

    WifiConfig();

    // TODO - does this decrease the jitter on the bit timings?
    wifi_set_sleep_type(NONE_SLEEP_T);

    WiFi.mode(WIFI_STA);  // Otherwise it may be in WIFI_AP_STA mode, broadcasting an SSID like AI_THINKER_XXXXXX
    WiFi.disconnect();  // After reset via HW button sometimes cannot seem to reconnect without this
    WiFi.persistent(false);
    WiFi.setAutoConnect(true);

    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
    WiFi.setOutputPower(20.5);

    // TODO - using Wi-Fi, unfortunately, has a detrimental effect on the packet CRC error rate. It will rise from
    // around 0.006% up to 0.1% or more. Underlying cause is timing failures due to Wi-Fi causing varying interrupt
    // latency. Not sure how to tackle this.
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    } // while
    Serial.println(F(" OK"));

    WiFi.hostname(GetHostname());

    Serial.printf_P(PSTR("Wi-Fi signal strength (RSSI): %ld dB\n"), WiFi.RSSI());

    // Might decrease number of packet CRC errors in case the board was previously using Wi-Fi (Wi-Fi settings are
    // persistent?)
    wifi_set_sleep_type(NONE_SLEEP_T);

    delay(1);
} // SetupWifi
