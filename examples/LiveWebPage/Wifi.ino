
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

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println(F(" OK"));
} // wifiSetup
