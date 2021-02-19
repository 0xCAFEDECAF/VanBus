
// TODO - use AP mode?
char* ssid = "MyCar"; // Choose yours
char* password = "MyCar"; // Fill in yours

const char* getHostname()
{
    return "Car";
} // getHostname

void setupWifi()
{
    Serial.printf_P(PSTR("Connecting to Wifi SSID '%s' "), ssid);

    // TODO - move to after WiFi.status() == WL_CONNECTED ?
    WiFi.hostname(getHostname());

    WiFi.mode(WIFI_STA);  // Otherwise it may be in WIFI_AP_STA mode, broadcasting an SSID like AI_THINKER_XXXXXX
    WiFi.disconnect();  // After reset via HW button sometimes cannot seem to reconnect without this
    WiFi.persistent(false);
    WiFi.setAutoConnect(true);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println(F(" OK"));

    Serial.printf_P(PSTR("Wifi signal strength (RSSI): %ld dB\n"), WiFi.RSSI());
} // wifiSetup
