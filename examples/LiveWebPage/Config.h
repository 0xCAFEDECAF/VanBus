#ifndef Config_h
#define Config_h

#include <ESP8266WiFi.h>

// -----
// Wi-Fi and IP configuration

#define HOST_NAME "MyCarLive"

#define WIFI_SSID "MyCar"  // Choose yours
#define WIFI_PASSWORD "WiFiPass"  // Fill in yours

// Define when using DHCP; comment out when using a static (fixed) IP address.
// Note: only applicable in Wi-Fi station (client) mode.
#define USE_DHCP

// Define when using a Windows Internet Connection Sharing (ICS) Wi-Fi. Comment out when using Android Wi-Fi hotspot.
// Note: only applicable when using a static (fixed) IP address, not when using DHCP.
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

// -----
// Infrared receiver

// Choose one of these #defines below (or define your own IR_RECV_PIN, IR_VCC and IR_GND)
#define IR_TSOP48XX
//#define IR_TSOP312XX

// TSOP48XX
#ifdef IR_TSOP48XX

  // IR receiver data pin
  #define IR_RECV_PIN D5

  // Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
  // TSOP4838 IR receiver as it typically uses only 0.7 mA (maximum GPIO current is 12 mA;
  // see https://tttapa.github.io/ESP8266/Chap04%20-%20Microcontroller.html for ESP8266 and
  // https://esp32.com/viewtopic.php?f=2&t=2027 for ESP32).
  #define IR_VCC D7
  #define IR_GND D6

#endif // IR_TSOP48XX

// TSOP312XX
#ifdef IR_TSOP312XX

  // IR receiver data pin
  #define IR_RECV_PIN D7

  // Using D7 as VCC and D6 as ground pin for the IR receiver. Should be possible with e.g. the
  // TSOP31238 IR receiver as it typically uses only 0.35 mA.
  #define IR_VCC D5
  #define IR_GND D0

#endif // IR_TSOP312XX

// -----
// Debugging

// Define to see infrared key hash values and timing on the serial port
#define DEBUG_IR_RECV

// Define to see JSON buffers printed on the serial port
// Note: for some reason, having JSON buffers printed on the serial port seems to reduce the number
//   of CRC errors in the received VAN bus packets
#define PRINT_JSON_BUFFERS_ON_SERIAL

// If PRINT_JSON_BUFFERS_ON_SERIAL is defined, which type of VAN-bus packets will be printed on the serial port?
#define SELECTED_PACKETS VAN_PACKETS_ALL_VAN_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_COM2000_ETC_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_HEAD_UNIT_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_SAT_NAV_PKTS
//#define SELECTED_PACKETS VAN_PACKETS_NO_VAN_PKTS

#define PRINT_VAN_CRC_ERROR_PACKETS_ON_SERIAL

#endif // Config_h
