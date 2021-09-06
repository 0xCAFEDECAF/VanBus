#ifndef Config_h
#define Config_h

#include <ESP8266WiFi.h>

#define HOST_NAME "MyCarLive"

#define WIFI_SSID "MyCar"  // Choose yours
#define WIFI_PASSWORD "WiFiPass"  // Fill in yours

// Define when using DHCP; comment out when using a fixed IP address.
#define USE_DHCP

// Define when using a Windows Internet Connection Sharing (ICS) Wi-Fi. Comment out when using Android Wi-Fi hotspot.
// Note: only applicable when using a fixed IP address, not when using DHCP.
//#define WINDOWS_ICS

#ifdef USE_DHCP

  // Using DHCP; ESP will register HOST_NAME via DHCP option 12.
  // Note: Neither Windows ICS nor Android Wi-Fi hotspot seem to support registering the host name on their
  // DHCP server implementation. Moreover, Windows ICS DHCP will NOT assign the previously assigned IP address to
  // the same MAC address upon new connection.

#else // ! USE_DHCP

  // Using fixed IP (not DHCP); hostname will not be registered.

  #ifdef WINDOWS_ICS  // When using a Windows ICS hotspot
    #define IP_ADDR "192.168.137.2"
    #define IP_GATEWAY "192.168.137.1"
    #define IP_SUBNET "255.255.255.0"

  #else  // When using an Android hotspot
    #define IP_ADDR "192.168.43.2"
    #define IP_GATEWAY "192.168.43.1" // Dummy value, actual GW can be on any address within the subnet
    #define IP_SUBNET "255.255.255.0"

  #endif // WINDOWS_ICS

#endif // USE_DHCP

inline void WifiConfig()
{
#ifndef USE_DHCP
    // Fixed IP configuration, e.g. when using Android / Windows ICS Wi-Fi hotspot
    IPAddress ip; ip.fromString(IP_ADDR);
    IPAddress gateway; gateway.fromString(IP_GATEWAY);
    IPAddress subnet; subnet.fromString(IP_SUBNET);
    WiFi.config(ip, gateway, subnet);
#endif // ifndef USE_DHCP
} // WifiConfig

// Which type of packets will be printed on Serial?

#define SELECTED_PACKETS VAN_PACKETS_ALL
//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC
//#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV
//#define SELECTED_PACKETS VAN_PACKETS_NONE

#endif // Config_h
