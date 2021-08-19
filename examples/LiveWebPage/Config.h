#ifndef Config_h
#define Config_h
 
#define WIFI_SSID "MyCar"  // Choose yours
#define WIFI_PASSWORD "WiFiPass"  // Fill in yours

inline void WifiConfig()
{
    // Write your Wi-Fi config stuff here, like setting IP stuff, e.g.:
    // IPAddress ip(192, 168, 1, 2);
    // IPAddress gateway(192, 168, 1, 1);
    // IPAddress subnet(255, 255, 255, 0);
    // IPAddress dns(192, 168, 1, 1);
    // WiFi.config(ip, gateway, subnet, dns);
} // WifiConfig

// Which type of packets will be printed on Serial?

#define SELECTED_PACKETS VAN_PACKETS_ALL
//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC
//#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV
//#define SELECTED_PACKETS VAN_PACKETS_NONE

#endif // Config_h
