/*
 * VanBus: LiveWebPage - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 0.3.0 - September, 2022
 *
 * MIT license, all text above must be included in any redistribution.
 *
 * -----
 * Wiring
 *
 * See paragraph 'Wiring' in the other example sketch, 'VanBusDump.ino'.
 *
 * -----
 * Details
 *
 * This sketch will host a web page, showing all data that has been received and parsed from the VAN bus. The web
 * page is updated continuously ("live") using a web socket (on port 81). Data from the VAN bus is sent to the
 * web socket in JSON format, which is then picked up on the client side by a JavaScript dispatcher script (served
 * in the same web page). The script uses jQuery to place the incoming data on the correct spot in the DOM.
 *
 * The result is a web page that runs remarkably light-weight, able to quickly handle updates (I've been able to
 * easily see 10 updates per second without queues running full).
 *
 * Note that the presentation of the data is not very slick: it is a simple list. This sketch is just to demonstrate
 * the mechanism; experts in graphic design need to step in from here :-)
 *
 * -----
 * Dependencies
 *
 * In order to compile this sketch, you need to additionally install the following libraries:
 * (Arduino IDE --> Menu 'Sketch' --> 'Include Library' --> 'Manage Libraries...')
 *
 * - A WebSockets library; choose either:
 *   * "WebSockets" by Markus Sattler (https://github.com/Links2004/arduinoWebSockets) 
 *     --> Tested with version 2.2.0, 2.3.3 and 2.3.4 .
 *   * "WebSockets_Generic" by Markus Sattler and Khoi Hoang (https://github.com/khoih-prog/WebSockets_Generic)
 *     --> Tested with version 2.4.0 .
 *
 *   Note: using:
 *       #define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266_ASYNC
 *     in 'WebSockets.h' appears to be very unstable, leading to TCP hiccups and even reboots. Use the default:
 *       #define WEBSOCKETS_NETWORK_TYPE NETWORK_ESP8266
 *     instead.
 *
 * The web site itself, as served by this sketch, uses jQuery, which it downloads from:
 * https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js
 * If you don't like Google tracking your surfing, you could use Firefox and install the 'LocalCDN' extension (see
 * https://addons.mozilla.org/nl/firefox/addon/localcdn-fork-of-decentraleyes ).
 *
 * -----
 * Building
 *
 * - Wi-Fi/TCP communication seems to be best when setting "build.sdk=NONOSDK22x_191122" in the "platform.txt" file
 *   (see "c:\Users\<user_id>\AppData\Local\Arduino15\packages\esp8266\hardware\esp8266\2.X.Y\".
 *   Keeping the default setting "build.sdk=NONOSDK22x_190703" seems to give a lot more communication hiccups.
 *   Note that the line "build.sdk=NONOSDK22x_191122" is not listed in the "platform.txt" file; you will have to
 *   add it yourself.
 *
 * - Make sure to select a "Higher Bandwidth" variant of the embedded IP stack "lwIP": Arduino IDE --> Tools -->
 *   lwIP variant: "v2 Higher Bandwidth" or "v2 Higher Bandwidth (no features)". The "Lower Memory" variants seem
 *   to have hiccups in the TCP traffic, ultimately causing the VAN Rx bus to overrun.
 */

#include <ESP8266WiFi.h>

// Either this for "WebSockets" (https://github.com/Links2004/arduinoWebSockets):
#include <WebSocketsServer.h>

// Or this for "WebSockets_Generic" (https://github.com/khoih-prog/WebSockets_Generic):
//#include <WebSocketsServer_Generic.h>

#include <ESP8266WebServer.h>
#include <VanBusRx.h>

#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
// For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
#define D2 (2)
#endif

// Set to GPIO pin connected to VAN bus transceiver output
int RX_PIN = D2;  // GPIO4 - often used as SDA (I2C)
//int RX_PIN = D3;  // GPIO0 - pulled up - Boot fails
//int RX_PIN = D4;  // GPIO2 - pulled up
//int RX_PIN = D8;  // GPIO15 - pulled to GND - Boot fails

// TODO - reduce size of large JSON packets like the ones containing guidance instruction icons
#define JSON_BUFFER_SIZE 4096
char jsonBuffer[JSON_BUFFER_SIZE];

ESP8266WebServer webServer;

char webpage[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang='en'>
  <head>
    <meta charset='UTF-8' />
    <script>

      // Inspired by https://gist.github.com/ismasan/299789
      var FancyWebSocket = function(url)
      {
          var conn = new WebSocket(url);

          var callbacks = {};

          this.bind = function(event_name, callback)
          {
              callbacks[event_name] = callbacks[event_name] || [];
              callbacks[event_name].push(callback);
              return this; // chainable
          };  // function

          // Dispatch to the right handlers
          conn.onmessage = function(evt)
          {
              var json = JSON.parse(evt.data)
              dispatch(json.event, json.data)
          };  // function

          conn.onclose = function() { dispatch('close', null); }
          conn.onopen = function() { dispatch('open', null); }

          var dispatch = function(event_name, message)
          {
              var chain = callbacks[event_name];
              if (chain == undefined) return; // No callbacks for this event
              for (var i = 0; i < chain.length; i++)
              {
                  chain[i](message)
              } // for
          } // function
      }; // FancyWebSocket

      // WebSocket class instance
      var socket = new FancyWebSocket("ws://" + window.location.hostname + ":81/");

      function writeToDom(jsonObj)
      {
          for (var item in jsonObj)
          {
              // Select by 'id' attribute (must be unique in the DOM)
              var selectorById = '#' + item;

              // Select also by custom attribute 'gid' (need not be unique)
              var selectorByAttr = '[gid="' + item + '"]';

              // jQuery-style loop over merged, unique-element array
              $.each($.unique($.merge($(selectorByAttr), $(selectorById))), function (index, selector)
              {
                  if ($(selector).length <= 0) return; // go to next iteration in .each()

                  if (Array.isArray(jsonObj[item]))
                  {
                      // Handling of multi-line DOM objects to show lists. Example:
                      //
                      // {
                      //   "event": "display",
                      //   "data": {
                      //     "alarm_list":
                      //     [
                      //       "Tyre pressure low",
                      //       "Door open",
                      //       "Water temperature too high",
                      //       "Oil level too low"
                      //     ]
                      //   }
                      // }

                      // Remove current lines
                      $(selector).empty();

                      var len = jsonObj[item].length;
                      for (var i = 0; i < len; i++) $(selector).append(jsonObj[item][i] + (i < len - 1 ? "<br />" : ""));
                  }
                  else if (!!jsonObj[item] && typeof(jsonObj[item]) === "object")
                  {
                      // Handling of "change attribute" events. Examples:
                      //
                      // {
                      //   "event": "display",
                      //   "data": {
                      //     "satnav_curr_heading": { "transform": "rotate(247.5)" }
                      //   }
                      // }
                      //
                      // {
                      //   "event": "display",
                      //   "data": {
                      //     "notification_on_mfd": {
                      //       "style": { "display": "block" }
                      //     }
                      //   }
                      // }

                      var attributeObj = jsonObj[item];
                      for (var attribute in attributeObj)
                      {
                          // Deeper nesting?
                          if (typeof(attributeObj) === "object")
                          {
                              var propertyObj = attributeObj[attribute];
                              for (var property in propertyObj)
                              {
                                  var value = propertyObj[property];
                                  $(selector).get(0)[attribute][property] = value;
                              } // for
                          }
                          else
                          {
                              var attrValue = attributeObj[attribute];
                              $(selector).attr(attribute, attrValue);
                          } // if
                      } // for
                  }
                  else
                  {
                      if ($(selector).hasClass("led"))
                      {
                          // Handle "led" class objects: no text copy, just turn ON or OFF
                          var on = jsonObj[item].toUpperCase() === "ON" || jsonObj[item].toUpperCase() === "YES";

                          $(selector).toggleClass("ledOn", on);
                          $(selector).toggleClass("ledOff", ! on);
                      }
                      else if ($(selector).hasClass("icon") || $(selector).get(0) instanceof SVGElement)
                      {
                          // Handle SVG elements and "icon" class objects: no text copy, just show or hide.
                          // From https://stackoverflow.com/questions/27950151/hide-show-element-with-a-boolean :
                          $(selector).toggle(jsonObj[item].toUpperCase() === "ON" || jsonObj[item].toUpperCase() === "YES");
                      }
                      else
                      {
                          // Handle simple "text" objects
                          $(selector).html(jsonObj[item]);
                      } // if
                  } // if
              }); // each
          } // for
      } // writeToDom

      // Bind to WebSocket to server events
      socket.bind('display', function(data) { writeToDom(data); });

    </script>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
    <style>
      .listbox
      {
        border:1px solid;
        width:700px;
        height:200px;
        overflow:scroll;
        overflow-x:hidden;
        white-space:nowrap;
      }
    </style>
  </head>
  <body>
    <hr/>
    <div>
      <p>ESP</p>
      <p>Boot version: <b id="esp_boot_version">---</b></p>
      <p>CPU speed: <b id="esp_cpu_speed">---</b></p>
      <p>SDK version: <b id="esp_sdk_version">---</b></p>
      <p>Chip ID: <b id="esp_chip_id">---</b></p>
      <p>Flash ID: <b id="esp_flash_id">---</b></p>
      <p>Flash size (real): <b id="esp_flash_size_real">---</b></p>
      <p>Flash size (IDE): <b id="esp_flash_size_ide">---</b></p>
      <p>Flash mode (IDE): <b id="esp_flash_mode_ide">---</b></p>
      <p>MAC address: <b id="esp_mac_address">---</b></p>
      <p>IP address: <b id="esp_ip_address">---</b></p>
      <p>Wi-Fi RSSI: <b id="esp_wifi_rssi">---</b></p>
      <p>Free RAM: <b id="esp_free_ram">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Multi-functional display (MFD)</p>
      <p>Language: <b id="mfd_language">---</b></p>
      <p>Temperature unit: <b id="mfd_temperature_unit">---</b></p>
      <p>Distance unit: <b id="mfd_distance_unit">---</b></p>
      <p>Time unit: <b id="mfd_time_unit">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Remote control</p>
      <p>Button: <b id="mfd_remote_control">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Engine</p>
      <p>Dash light: <b id="dash_light">---</b></p>
      <p>Dash light - actual brightness: <b id="dash_actual_brightness">---</b></p>
      <p>Contact key position: <b id="contact_key_position">---</b></p>
      <p>Engine running: <b id="engine_running">---</b></p>
      <p>Economy mode: <b id="economy_mode">---</b></p>
      <p>In reverse: <b id="in_reverse">---</b></p>
      <p>Trailer: <b id="trailer">---</b></p>

      <p>Coolant temperature: <b id="coolant_temp">---</b> &deg;C</p>

      <div style="height:40px;">
        <!-- Linear gauge -->
        <div id="coolant_temp_perc" style="height:30px; transform:scaleX(0.6); transform-origin:left center;">
          <svg>
            <line style="stroke:rgb(41,55,74); stroke-width:30;" x1="0" y1="15" x2="400" y2="15" />
          </svg>
        </div>
        <!-- Reference bar -->
        <div style="height:10px;">
          <svg>
            <line style="stroke:#dfe7f2; stroke-width:10;" x1="0" y1="5" x2="400" y2="5" />
          </svg>
        </div>
      </div>

      <p>Odometer 1: <b id="odometer_1">---</b></p>
      <p>Odometer 2: <b id="odometer_2">---</b></p>
      <p>Exterior temperature: <b id="exterior_temp">---</b> &deg;C</p>
    </div>
    <hr/>
    <div>
      <p>Head Unit Stalk</p>
      <p>NEXT button: <b id="head_unit_stalk_button_next">---</b></p>
      <p>PREV button: <b id="head_unit_stalk_button_prev">---</b></p>
      <p>VOL_UP button: <b id="head_unit_stalk_button_volume_up">---</b></p>
      <p>VOL_DOWN button: <b id="head_unit_stalk_button_volume_down">---</b></p>
      <p>SRC button: <b id="head_unit_stalk_button_source">---</b></p>
      <p>Wheel: <b id="head_unit_stalk_wheel">---</b></p>
      <p>Wheel rollover: <b id="head_unit_stalk_wheel_rollover">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Lights</p>
      <p>Instrument cluster: <b id="instrument_cluster">---</b></p>
      <p>Speed regulator wheel: <b id="speed_regulator_wheel">---</b></p>
      <p>Hazard lights: <b id="hazard_lights">---</b></p>
      <p>Diesel glow plugs: <b id="diesel_glow_plugs">---</b></p>
      <p>Door open: <b id="door_open">---</b></p>
      <p>Service after km: <b id="distance_to_service">---</b> km</p>
      <p>Service after km (dash): <b id="distance_to_service_dash">---</b></p>
      <p>Lights: <b id="lights">---</b></p>
      <p>Auto gearbox: <b id="auto_gearbox">---</b></p>
      <p>Oil temperature: <b id="oil_temp">---</b> &deg;C</p>
      <p>Fuel level: <b id="fuel_level">---</b></p>

      <p>Oil level (raw): <b id="oil_level_raw">---</b></p>

      <div style="height:40px;">
        <!-- Linear gauge -->
        <div id="oil_level_raw_perc" style="height:30px; transform:scaleX(0.6); transform-origin:left center;">
          <svg>
            <line style="stroke:rgb(41,55,74); stroke-width:30;" x1="0" y1="15" x2="400" y2="15" />
          </svg>
        </div>
        <!-- Reference bar -->
        <div style="height:10px;">
          <svg>
            <line style="stroke:#dfe7f2; stroke-width:10;" x1="0" y1="5" x2="400" y2="5" />
          </svg>
        </div>
      </div>

      <p>Oil level (dash): <b id="oil_level_dash">---</b></p>
      <p>LPG fuel level: <b id="lpg_fuel_level">---</b></p>
      <p>Cruise control: <b id="cruise_control">---</b></p>
      <p>Cruise control speed: <b id="cruise_control_speed">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Car status</p>
      <p>Door front right: <b id="door_front_right">---</b></p>
      <p>Door front left: <b id="door_front_left">---</b></p>
      <p>Door rear right: <b id="door_rear_right">---</b></p>
      <p>Door rear left: <b id="door_rear_left">---</b></p>
      <p>Door boot: <b id="door_boot">---</b></p>
      <p>Right stalk button: <b id="right_stalk_button">---</b></p>
      <p>Trip 1 average speed: <b id="avg_speed_1">---</b> km/h</p>
      <p>Trip 2 average speed: <b id="avg_speed_2">---</b> km/h</p>
      <p>Average speed (EMA): <b id="exp_moving_avg_speed">---</b> km/h</p>
      <p>Trip 1 distance: <b id="distance_1">---</b> km</p>
      <p>Trip 1 average fuel consumption: <b id="avg_consumption_1">---</b> lt/100 km</p>
      <p>Trip 2 distance: <b id="distance_2">---</b> km</p>
      <p>Trip 2 average fuel consumption: <b id="avg_consumption_2">---</b> lt/100 km</p>
      <p>Current fuel consumption: <b id="inst_consumption">---</b> lt/100 km</p>
      <p>Remaining distance in fuel tank: <b id="distance_to_empty">---</b> km</p>
    </div>
    <hr/>
    <div>
      <p>Wheel sensors</p>
      <p>Speed rear right: <b id="wheel_speed_rear_right">---</b></p>
      <p>Speed rear left: <b id="wheel_speed_rear_left">---</b></p>
      <p>Pulses rear right: <b id="wheel_pulses_rear_right">---</b></p>
      <p>Pulses rear right: <b id="wheel_pulses_rear_left">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Warnings and notifications</p>
      <p>Active list</p>
      <div id="alarm_list" class="listbox"></div>
      <p>Message shown on multi-function display: <b id="notification_message_on_mfd">---</b></p>
      <p>Doors locked: <b id="doors_locked">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Dashboard</p>
      <p>Engine RPM: <b id="engine_rpm">---</b> /min</p>
      <p>Vehicle speed: <b id="vehicle_speed">---</b> km/h</p>
    </div>
    <hr/>
    <div>
      <p>Dashboard Buttons</p>
      <p>Hazard lights button: <b id="hazard_lights_button">---</b></p>
      <p>Door lock: <b id="door_lock">---</b></p>
      <p>Dash light - set brightness: <b id="dashboard_programmed_brightness">---</b></p>
      <p>ESP: <b id="esp">---</b></p>

      <p>Fuel level (filtered): <b id="fuel_level_filtered">---</b> litres</p>

      <div style="height:40px;">
        <!-- Linear gauge -->
        <div id="fuel_level_filtered_perc" style="height:30px; transform:scaleX(0.6); transform-origin:left center;">
          <svg>
            <line style="stroke:rgb(41,55,74); stroke-width:30;" x1="0" y1="15" x2="400" y2="15" />
          </svg>
        </div>
        <!-- Reference bar -->
        <div style="height:10px;">
          <svg>
            <line style="stroke:#dfe7f2; stroke-width:10;" x1="0" y1="5" x2="400" y2="5" />
          </svg>
        </div>
      </div>

      <p>Fuel level (raw): <b id="fuel_level_raw">---</b> litres</p>
    </div>
    <hr/>
    <div>
      <p>Head Unit</p>
      <p>Last report: <b id="head_unit_report">---</b></p>
      <p>Power: <b id="head_unit_power">---</b></p>
      <p>Tape present: <b id="tape_present">---</b></p>
      <p>CD present: <b id="cd_present">---</b></p>
      <p>Source: <b id="audio_source">---</b></p>
      <p>External mute: <b id="ext_mute">---</b></p>
      <p>Mute: <b id="mute">---</b></p>

      <p>Volume: <b id="volume">---</b> (updated: <b id="volume_update">---</b>)</p>

      <div style="height:40px;">
        <!-- Linear gauge -->
        <div id="volume_perc" style="height:30px; transform:scaleX(0.6); transform-origin:left center;">
          <svg>
            <line style="stroke:rgb(41,55,74); stroke-width:30;" x1="0" y1="15" x2="400" y2="15" />
          </svg>
        </div>
        <!-- Reference bar -->
        <div style="height:10px;">
          <svg>
            <line style="stroke:#dfe7f2; stroke-width:10;" x1="0" y1="5" x2="400" y2="5" />
          </svg>
        </div>
      </div>

      <p>Audio menu: <b id="audio_menu">---</b></p>
      <p>Bass: <b id="bass">---</b> (updated: <b id="bass_update">---</b>)</p>
      <p>Treble: <b id="treble">---</b> (updated: <b id="treble_update">---</b>)</p>
      <p>Loudness: <b id="loudness">---</b></p>
      <p>Balance: <b id="balance">---</b> (updated: <b id="balance_update">---</b>)</p>
      <p>Fader: <b id="fader">---</b> (updated: <b id="fader_update">---</b>)</p>
      <p>Auto volume: <b id="auto_volume">---</b></p>
      <p>Buttons pressed: <b id="head_unit_button_pressed">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Radio</p>
      <p>Band: <b id="tuner_band">---</b></p>
      <p>Memory: <b id="tuner_memory">---</b></p>
      <p>Frequency: <b id="frequency">---</b></p>
      <p>Signal strength: <b id="signal_strength">---</b></p>
      <p>Tuner sensitivity: <b id="search_sensitivity">---</b></p>
      <p>Searching: <b id="search_mode">---</b></p>
      <p>Search direction: <b id="search_direction">---</b></p>
      <p>PTY selection menu: <b id="pty_selection_menu">---</b></p>
      <p>PTY selected: <b id="selected_pty_full">---</b></p>
      <p>PTY standby mode: <b id="pty_standby_mode">---</b></p>
      <p>PTY match: <b id="pty_match">---</b></p>
      <p>PTY: <b id="pty_full">---</b></p>
      <p>PI: <b id="pi_code">---</b></p>
      <p>PI country: <b id="pi_country">---</b></p>
      <p>PI area coverage: <b id="pi_area_coverage">---</b></p>
      <p>Regional: <b id="regional">---</b></p>
      <p>TA not available: <b id="ta_not_available">---</b></p>
      <p>RDS not available: <b id="rds_not_available">---</b></p>
      <p>TA: <b id="ta_selected">---</b></p>
      <p>RDS: <b id="rds_selected">---</b></p>
      <p>RDS Text: <b id="rds_text">---</b></p>
      <p>Traffic Info: <b id="info_traffic">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Radio presets</p>
      <p>FM1 - 1: <b id="radio_preset_FM1_1">---</b></p>
      <p>FM1 - 2: <b id="radio_preset_FM1_2">---</b></p>
      <p>FM1 - 3: <b id="radio_preset_FM1_3">---</b></p>
      <p>FM1 - 4: <b id="radio_preset_FM1_4">---</b></p>
      <p>FM1 - 5: <b id="radio_preset_FM1_5">---</b></p>
      <p>FM1 - 6: <b id="radio_preset_FM1_6">---</b></p>
      <p>FM2 - 1: <b id="radio_preset_FM2_1">---</b></p>
      <p>FM2 - 2: <b id="radio_preset_FM2_2">---</b></p>
      <p>FM2 - 3: <b id="radio_preset_FM2_3">---</b></p>
      <p>FM2 - 4: <b id="radio_preset_FM2_4">---</b></p>
      <p>FM2 - 5: <b id="radio_preset_FM2_5">---</b></p>
      <p>FM2 - 6: <b id="radio_preset_FM2_6">---</b></p>
      <p>FM_AST - 1: <b id="radio_preset_FMAST_1">---</b></p>
      <p>FM_AST - 2: <b id="radio_preset_FMAST_2">---</b></p>
      <p>FM_AST - 3: <b id="radio_preset_FMAST_3">---</b></p>
      <p>FM_AST - 4: <b id="radio_preset_FMAST_4">---</b></p>
      <p>FM_AST - 5: <b id="radio_preset_FMAST_5">---</b></p>
      <p>FM_AST - 6: <b id="radio_preset_FMAST_6">---</b></p>
      <p>AM - 1: <b id="radio_preset_AM_1">---</b></p>
      <p>AM - 2: <b id="radio_preset_AM_2">---</b></p>
      <p>AM - 3: <b id="radio_preset_AM_3">---</b></p>
      <p>AM - 4: <b id="radio_preset_AM_4">---</b></p>
      <p>AM - 5: <b id="radio_preset_AM_5">---</b></p>
      <p>AM - 6: <b id="radio_preset_AM_6">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Tape</p>
      <p>Status: <b id="tape_status">---</b></p>
      <p>Side: <b id="tape_side">---</b></p>
    </div>
    <hr/>
    <div>
      <p>CD Player</p>
      <p>Status: <b id="cd_status">---</b></p>
      <p>Current track: <b id="cd_current_track">---</b></p>
      <p>Current track time: <b id="cd_track_time">---</b></p>
      <p>Number of tracks: <b id="cd_total_tracks">---</b></p>
      <p>Total duration: <b id="cd_total_time">---</b></p>
    </div>
    <hr/>
    <div>
      <p>CD Changer</p>
      <p>Present: <b id="cd_changer_present">---</b></p>
      <p>Loading: <b id="cd_changer_loading">---</b></p>
      <p>Operational: <b id="cd_changer_operational">---</b></p>
      <p>Searching: <b id="cd_changer_status_searching">---</b></p>
      <p>Status: <b id="cd_changer_status">---</b></p>
      <p>Random mode: <b id="cd_changer_random">---</b></p>
      <p>Cartridge: <b id="cd_changer_cartridge_present">---</b></p>
      <p>Current track time: <b id="cd_changer_track_time">---</b></p>
      <p>Current track: <b id="cd_changer_current_track">---</b></p>
      <p>Number of tracks: <b id="cd_changer_total_tracks">---</b></p>
      <p>Current CD: <b id="cd_changer_current_disc">---</b></p>
      <p>CD 1 present: <b id="cd_changer_disc_1_present">---</b></p>
      <p>CD 2 present: <b id="cd_changer_disc_2_present">---</b></p>
      <p>CD 3 present: <b id="cd_changer_disc_3_present">---</b></p>
      <p>CD 4 present: <b id="cd_changer_disc_4_present">---</b></p>
      <p>CD 5 present: <b id="cd_changer_disc_5_present">---</b></p>
      <p>CD 6 present: <b id="cd_changer_disc_6_present">---</b></p>
      <p>Command: <b id="cd_changer_command">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Air Conditioning</p>
      <p>A/C icon: <b id="ac_icon">---</b></p>
      <p>Air recirculation: <b id="recirc">---</b></p>
      <p>Rear heater 1: <b id="rear_heater_1">---</b></p>
      <p>Reported fan speed: <b id="reported_fan_speed">---</b></p>
      <p>Set fan speed: <b id="set_fan_speed">---</b></p>
      <p>Contact key on: <b id="contact_key_on">---</b></p>
      <p>A/C enabled: <b id="ac_enabled">---</b></p>
      <p>Rear heater 2: <b id="rear_heater_2">---</b></p>
      <p>A/C compressor: <b id="ac_compressor">---</b></p>
      <p>Contact key position: <b id="contact_key_position_ac">---</b></p>
      <p>Condenser temperature: <b id="condenser_temp">---</b> &deg;C</p>
      <p>Evaporator temperature: <b id="evaporator_temp">---</b> &deg;C</p>
    </div>
    <hr/>
    <div>
      <p>Satellite Navigation</p>
      <p>Status 1: <b id="satnav_status_1">---</b></p>
      <p>Status 2: <b id="satnav_status_2">---</b></p>
      <p>Status 3: <b id="satnav_status_3">---</b></p>
      <p>Disc: <b id="satnav_disc_recognized">---</b></p>
      <p>Guidance status: <b id="satnav_guidance_status">---</b></p>
      <p>Guidance preference: <b id="satnav_guidance_preference">---</b></p>
      <p>Route computed: <b id="satnav_route_computed">---</b></p>
      <p>Destination reachable: <b id="satnav_destination_reachable">---</b></p>
      <p>On digitized area (on map): <b id="satnav_on_map">---</b></p>
      <p>CD-ROM download finished: <b id="satnav_download_finished">---</b></p>
      <p>System ID</p>
      <div id="satnav_system_id" class="listbox"></div>
      <p>MFD to sat nav: <b id="mfd_to_satnav_instruction">---</b></p>
      <p>GPS fix: <b id="satnav_gps_fix">---</b></p>
      <p>GPS fix lost: <b id="satnav_gps_fix_lost">---</b></p>
      <p>GPS scanning: <b id="satnav_gps_scanning">---</b></p>
      <p>Language: <b id="satnav_language">---</b></p>
      <p>GPS speed: <b id="satnav_gps_speed">---</b></p>
      <p>ZZZ (?): <b id="satnav_zzz">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav guidance data</p>

      <!-- Rotating round an abstract transform-origin like 'center' is better supported for a <div> than an <svg> element -->
      <div id="satnav_curr_heading_compass_needle" style="width:120px; height:120px; transform:rotate(0deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M60 15 l30 100 l-60 0 Z"></path>
        </svg>
      </div>
      <p>Current heading: <b id="satnav_curr_heading">---</b></p>

      <div id="satnav_heading_to_dest_pointer" style="width:120px; height:120px; transform:rotate(0deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M60 10 l30 100 l-60 0 Z"></path>
        </svg>
      </div>
      <p>Heading to destination: <b id="satnav_heading_to_dest">---</b></p>

      <p>Road distance to destination: <b id="satnav_distance_to_dest_via_road">---</b></p>
      <p>Straight line to destination: <b id="satnav_distance_to_dest_via_straight_line">---</b></p>
      <p>Turn at: <b id="satnav_turn_at">---</b></p>
      <p>Heading on roundabout: <b id="satnav_heading_on_roundabout_as_text">---</b></p>
      <p>Minutes to travel (?): <b id="satnav_minutes_to_travel">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav guidance instruction</p>
      <p>Current turn icon: <b id="satnav_curr_turn_icon">---</b></p>
      <p>Take right exit icon: <b id="satnav_fork_icon_take_right_exit">---</b></p>
      <p>Keep right icon: <b id="satnav_fork_icon_keep_right">---</b></p>
      <p>Take left exit icon: <b id="satnav_fork_icon_take_left_exit">---</b></p>
      <p>Keep left icon: <b id="satnav_fork_icon_keep_left">---</b></p>
      <p>Next turn icon: <b id="satnav_next_turn_icon">---</b></p>
      <p>'Turn around if possible' icon: <b id="satnav_turn_around_if_possible_icon">---</b></p>
      <p>'Follow road' icon: <b id="satnav_follow_road_icon">---</b></p>
      <p>'Not on map' icon: <b id="satnav_not_on_map_icon">---</b></p>

      <div id="satnav_curr_turn_icon_direction" style="width:120px; height:120px; transform:rotate(0deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M60 10 l30 100 l-60 0 Z"></path>
        </svg>
      </div>
      <p>Current turn icon direction: <b id="satnav_curr_turn_icon_direction_as_text">---</b></p>

      <p>Current turn icon leg 22.5 : <b id="satnav_curr_turn_icon_leg_22_5">---</b></p>
      <p>Current turn icon leg 45.0 : <b id="satnav_curr_turn_icon_leg_45_0">---</b></p>
      <p>Current turn icon leg 67.5 : <b id="satnav_curr_turn_icon_leg_67_5">---</b></p>
      <p>Current turn icon leg 90.0 : <b id="satnav_curr_turn_icon_leg_90_0">---</b></p>
      <p>Current turn icon leg 112.5: <b id="satnav_curr_turn_icon_leg_112_5">---</b></p>
      <p>Current turn icon leg 135.0: <b id="satnav_curr_turn_icon_leg_135_0">---</b></p>
      <p>Current turn icon leg 157.5: <b id="satnav_curr_turn_icon_leg_157_5">---</b></p>
      <p>Current turn icon leg 180.0: <b id="satnav_curr_turn_icon_leg_180_0">---</b></p>
      <p>Current turn icon leg 202.5: <b id="satnav_curr_turn_icon_leg_202_5">---</b></p>
      <p>Current turn icon leg 225.0: <b id="satnav_curr_turn_icon_leg_225_0">---</b></p>
      <p>Current turn icon leg 247.5: <b id="satnav_curr_turn_icon_leg_247_5">---</b></p>
      <p>Current turn icon leg 270.0: <b id="satnav_curr_turn_icon_leg_270_0">---</b></p>
      <p>Current turn icon leg 292.5: <b id="satnav_curr_turn_icon_leg_292_5">---</b></p>
      <p>Current turn icon leg 315.0: <b id="satnav_curr_turn_icon_leg_315_0">---</b></p>
      <p>Current turn icon leg 337.5: <b id="satnav_curr_turn_icon_leg_337_5">---</b></p>
      <p>Current turn icon no entry 22.5 : <b id="satnav_curr_turn_icon_no_entry_22_5">---</b></p>
      <p>Current turn icon no entry 45.0 : <b id="satnav_curr_turn_icon_no_entry_45_0">---</b></p>
      <p>Current turn icon no entry 67.5 : <b id="satnav_curr_turn_icon_no_entry_67_5">---</b></p>
      <p>Current turn icon no entry 90.0 : <b id="satnav_curr_turn_icon_no_entry_90_0">---</b></p>
      <p>Current turn icon no entry 112.5: <b id="satnav_curr_turn_icon_no_entry_112_5">---</b></p>
      <p>Current turn icon no entry 135.0: <b id="satnav_curr_turn_icon_no_entry_135_0">---</b></p>
      <p>Current turn icon no entry 157.5: <b id="satnav_curr_turn_icon_no_entry_157_5">---</b></p>
      <p>Current turn icon no entry 180.0: <b id="satnav_curr_turn_icon_no_entry_180_0">---</b></p>
      <p>Current turn icon no entry 202.5: <b id="satnav_curr_turn_icon_no_entry_202_5">---</b></p>
      <p>Current turn icon no entry 225.0: <b id="satnav_curr_turn_icon_no_entry_225_0">---</b></p>
      <p>Current turn icon no entry 247.5: <b id="satnav_curr_turn_icon_no_entry_247_5">---</b></p>
      <p>Current turn icon no entry 270.0: <b id="satnav_curr_turn_icon_no_entry_270_0">---</b></p>
      <p>Current turn icon no entry 292.5: <b id="satnav_curr_turn_icon_no_entry_292_5">---</b></p>
      <p>Current turn icon no entry 315.0: <b id="satnav_curr_turn_icon_no_entry_315_0">---</b></p>
      <p>Current turn icon no entry 337.5: <b id="satnav_curr_turn_icon_no_entry_337_5">---</b></p>

      <div id="satnav_next_turn_icon_direction" style="width:120px; height:120px; transform:rotate(0deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M60 10 l30 100 l-60 0 Z"></path>
        </svg>
      </div>
      <p>Next turn icon direction: <b id="satnav_next_turn_icon_direction_as_text">---</b></p>

      <p>Next turn icon leg 22.5 : <b id="satnav_next_turn_icon_leg_22_5">---</b></p>
      <p>Next turn icon leg 45.0 : <b id="satnav_next_turn_icon_leg_45_0">---</b></p>
      <p>Next turn icon leg 67.5 : <b id="satnav_next_turn_icon_leg_67_5">---</b></p>
      <p>Next turn icon leg 90.0 : <b id="satnav_next_turn_icon_leg_90_0">---</b></p>
      <p>Next turn icon leg 112.5: <b id="satnav_next_turn_icon_leg_112_5">---</b></p>
      <p>Next turn icon leg 135.0: <b id="satnav_next_turn_icon_leg_135_0">---</b></p>
      <p>Next turn icon leg 157.5: <b id="satnav_next_turn_icon_leg_157_5">---</b></p>
      <p>Next turn icon leg 180.0: <b id="satnav_next_turn_icon_leg_180_0">---</b></p>
      <p>Next turn icon leg 202.5: <b id="satnav_next_turn_icon_leg_202_5">---</b></p>
      <p>Next turn icon leg 225.0: <b id="satnav_next_turn_icon_leg_225_0">---</b></p>
      <p>Next turn icon leg 247.5: <b id="satnav_next_turn_icon_leg_247_5">---</b></p>
      <p>Next turn icon leg 270.0: <b id="satnav_next_turn_icon_leg_270_0">---</b></p>
      <p>Next turn icon leg 292.5: <b id="satnav_next_turn_icon_leg_292_5">---</b></p>
      <p>Next turn icon leg 315.0: <b id="satnav_next_turn_icon_leg_315_0">---</b></p>
      <p>Next turn icon leg 337.5: <b id="satnav_next_turn_icon_leg_337_5">---</b></p>
      <p>Next turn icon no entry 22.5 : <b id="satnav_next_turn_icon_no_entry_22_5">---</b></p>
      <p>Next turn icon no entry 45.0 : <b id="satnav_next_turn_icon_no_entry_45_0">---</b></p>
      <p>Next turn icon no entry 67.5 : <b id="satnav_next_turn_icon_no_entry_67_5">---</b></p>
      <p>Next turn icon no entry 90.0 : <b id="satnav_next_turn_icon_no_entry_90_0">---</b></p>
      <p>Next turn icon no entry 112.5: <b id="satnav_next_turn_icon_no_entry_112_5">---</b></p>
      <p>Next turn icon no entry 135.0: <b id="satnav_next_turn_icon_no_entry_135_0">---</b></p>
      <p>Next turn icon no entry 157.5: <b id="satnav_next_turn_icon_no_entry_157_5">---</b></p>
      <p>Next turn icon no entry 180.0: <b id="satnav_next_turn_icon_no_entry_180_0">---</b></p>
      <p>Next turn icon no entry 202.5: <b id="satnav_next_turn_icon_no_entry_202_5">---</b></p>
      <p>Next turn icon no entry 225.0: <b id="satnav_next_turn_icon_no_entry_225_0">---</b></p>
      <p>Next turn icon no entry 247.5: <b id="satnav_next_turn_icon_no_entry_247_5">---</b></p>
      <p>Next turn icon no entry 270.0: <b id="satnav_next_turn_icon_no_entry_270_0">---</b></p>
      <p>Next turn icon no entry 292.5: <b id="satnav_next_turn_icon_no_entry_292_5">---</b></p>
      <p>Next turn icon no entry 315.0: <b id="satnav_next_turn_icon_no_entry_315_0">---</b></p>
      <p>Next turn icon no entry 337.5: <b id="satnav_next_turn_icon_no_entry_337_5">---</b></p>
      <p>'Fork' icon - take right exit: <b id="satnav_fork_icon_take_right_exit">---</b></p>
      <p>'Fork' icon - keep right: <b id="satnav_fork_icon_keep_right">---</b></p>
      <p>'Fork' icon - take left exit: <b id="satnav_fork_icon_take_left_exit">---</b></p>
      <p>'Fork' icon - keep left: <b id="satnav_fork_icon_keep_left">---</b></p>
      <p>'Follow road' icon - next instruction: <b id="satnav_follow_road_next_instruction">---</b></p>

      <div id="satnav_not_on_map_follow_heading" style="width:120px; height:120px; transform:rotate(0deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M60 10 l30 100 l-60 0 Z"></path>
        </svg>
      </div>
      <p>'Not on map' icon - follow heading: <b id="satnav_not_on_map_follow_heading_as_text">---</b></p>

    </div>
    <hr/>
    <div>
      <p>SatNav report</p>
      <p>Report: <b id="satnav_report">---</b></p>
      <p>Current street: <b id="satnav_curr_street">---</b></p>
      <p>Next street: <b id="satnav_next_street">---</b></p>
      <p>Current destination - country: <b id="satnav_current_destination_country">---</b></p>
      <p>Current destination - province: <b id="satnav_current_destination_province">---</b></p>
      <p>Current destination - city: <b id="satnav_current_destination_city">---</b></p>
      <p>Current destination - street: <b id="satnav_current_destination_street">---</b></p>
      <p>Current destination - house number: <b id="satnav_current_destination_house_number">---</b></p>
      <p>Last destination - country: <b id="satnav_last_destination_country">---</b></p>
      <p>Last destination - province: <b id="satnav_last_destination_province">---</b></p>
      <p>Last destination - city: <b id="satnav_last_destination_city">---</b></p>
      <p>Last destination - street: <b id="satnav_last_destination_street">---</b></p>
      <p>Last destination - house number: <b id="satnav_last_destination_house_number">---</b></p>
      <p>Personal address - entry: <b id="satnav_personal_address_entry">---</b></p>
      <p>Personal address - country: <b id="satnav_personal_address_country">---</b></p>
      <p>Personal address - province: <b id="satnav_personal_address_province">---</b></p>
      <p>Personal address - city: <b id="satnav_personal_address_city">---</b></p>
      <p>Personal address - street: <b id="satnav_personal_address_street">---</b></p>
      <p>Personal address - house number: <b id="satnav_personal_address_house_number">---</b></p>
      <p>Professional address - entry: <b id="satnav_professional_address_entry">---</b></p>
      <p>Professional address - country: <b id="satnav_professional_address_country">---</b></p>
      <p>Professional address - province: <b id="satnav_professional_address_province">---</b></p>
      <p>Professional address - city: <b id="satnav_professional_address_city">---</b></p>
      <p>Professional address - street: <b id="satnav_professional_address_street">---</b></p>
      <p>Professional address - house number: <b id="satnav_professional_address_house_number">---</b></p>
      <p>Service address - entry: <b id="satnav_service_address_entry">---</b></p>
      <p>Service address - country: <b id="satnav_service_address_country">---</b></p>
      <p>Service address - province: <b id="satnav_service_address_province">---</b></p>
      <p>Service address - city: <b id="satnav_service_address_city">---</b></p>
      <p>Service address - street: <b id="satnav_service_address_street">---</b></p>
      <p>Service address distance: <b id="satnav_service_address_distance">---</b></p>
      <p>Address list</p>
      <div id="satnav_list" class="listbox"></div>
      <p>House number range: <b id="satnav_house_number_range">---</b></p>
      <p>Software module list</p>
      <div id="satnav_software_modules_list" class="listbox"></div>
    </div>
    <hr/>
    <div>
      <p>Multi-function display to satnav</p>
      <p>Request: <b id="mfd_to_satnav_request">---</b></p>
      <p>Request type: <b id="mfd_to_satnav_request_type">---</b></p>
      <p>Go to screen: <b id="mfd_to_satnav_go_to_screen">---</b></p>
      <p>Entered character: <b id="mfd_to_satnav_enter_character">---</b></p>
      <p>List offset: <b id="mfd_to_satnav_offset">---</b></p>
      <p>List length: <b id="mfd_to_satnav_length">---</b></p>
      <p>List selection: <b id="mfd_to_satnav_selection">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav to multi-function display</p>
      <p>Response: <b id="satnav_to_mfd_response">---</b></p>
      <p>List size: <b id="satnav_to_mfd_list_size">---</b></p>
      <p>List 2 size: <b id="satnav_to_mfd_list_2_size">---</b></p>
      <p>Characters: <b id="satnav_to_mfd_show_characters">---</b></p>
    </div>
    <hr/>
    <div>
      <p>COM 2000</p>
      <p>Light switch 'auto': <b id="com2000_light_switch_auto">---</b></p>
      <p>Light switch fog light forward: <b id="com2000_light_switch_fog_light_forward">---</b></p>
      <p>Light switch fog light backward: <b id="com2000_light_switch_fog_light_backward">---</b></p>
      <p>Light switch signal beam: <b id="com2000_light_switch_signal_beam">---</b></p>
      <p>Light switch full beam: <b id="com2000_light_switch_full_beam">---</b></p>
      <p>Light switch all off: <b id="com2000_light_switch_all_off">---</b></p>
      <p>Light switch side lights: <b id="com2000_light_switch_side_lights">---</b></p>
      <p>Light switch low beam: <b id="com2000_light_switch_low_beam">---</b></p>
      <p>Right stalk trip computer: <b id="com2000_right_stalk_button_trip_computer">---</b></p>
      <p>Right stalk rear window wash: <b id="com2000_right_stalk_rear_window_wash">---</b></p>
      <p>Right stalk rear window wipe: <b id="com2000_right_stalk_rear_window_wiper">---</b></p>
      <p>Right stalk windscreen wash: <b id="com2000_right_stalk_windscreen_wash">---</b></p>
      <p>Right stalk windscreen wipe once: <b id="com2000_right_stalk_windscreen_wipe_once">---</b></p>
      <p>Right stalk windscreen wipe auto: <b id="com2000_right_stalk_windscreen_wipe_auto">---</b></p>
      <p>Right stalk windscreen wipe normal: <b id="com2000_right_stalk_windscreen_wipe_normal">---</b></p>
      <p>Right stalk windscreen wipe fast: <b id="com2000_right_stalk_windscreen_wipe_fast">---</b></p>
      <p>Turn signal left: <b id="com2000_turn_signal_left">---</b></p>
      <p>Turn signal right: <b id="com2000_turn_signal_right">---</b></p>
      <p>Head unit stalk SRC button: <b id="com2000_head_unit_stalk_button_src">---</b></p>
      <p>Head unit stalk VOL UP button: <b id="com2000_head_unit_stalk_button_volume_up">---</b></p>
      <p>Head unit stalk VOL DOWN button: <b id="com2000_head_unit_stalk_button_volume_down">---</b></p>
      <p>Head unit stalk SEET BK button: <b id="com2000_head_unit_stalk_button_seek_backward">---</b></p>
      <p>Head unit stalk SEEK FWD button: <b id="com2000_head_unit_stalk_button_seek_forward">---</b></p>
      <p>Head unit stalk wheel position: <b id="com2000_head_unit_stalk_wheel_pos">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Multi-functional display to head unit</p>
      <p>Audio bits</p>
      <p>Update mute: <b id="head_unit_update_audio_bits_mute">---</b></p>
      <p>Update auto-volume: <b id="head_unit_update_audio_bits_auto_volume">---</b></p>
      <p>Update loudness: <b id="head_unit_update_audio_bits_loudness">---</b></p>
      <p>Update audio menu: <b id="head_unit_update_audio_bits_audio_menu">---</b></p>
      <p>Update power 1: <b id="head_unit_update_audio_bits_power">---</b></p>
      <p>Update contact key: <b id="head_unit_update_audio_bits_contact_key">---</b></p>
      <p>Audio</p>
      <p>Switch to: <b id="head_unit_update_switch_to">---</b></p>
      <p>Update power 2: <b id="head_unit_update_power">---</b></p>
      <p>Update source: <b id="head_unit_update_source">---</b></p>
      <p>Update volume 1: <b id="head_unit_update_volume_1">---</b></p>
      <p>Update balance: <b id="head_unit_update_balance">---</b></p>
      <p>Update fader: <b id="head_unit_update_fader">---</b></p>
      <p>Update bass: <b id="head_unit_update_bass">---</b></p>
      <p>Update treble: <b id="head_unit_update_treble">---</b></p>
      <p>Update volume 2: <b id="head_unit_update_volume_2">---</b></p>
      <p>Audio Levels</p>
      <p>Switch balance: <b id="head_unit_update_audio_levels_balance">---</b></p>
      <p>Update fader: <b id="head_unit_update_audio_levels_fader">---</b></p>
      <p>Update bass: <b id="head_unit_update_audio_levels_bass">---</b></p>
      <p>Update treble: <b id="head_unit_update_audio_levels_treble">---</b></p>
      <p>Tuner preset</p>
      <p>Band: <b id="head_unit_preset_request_band">---</b></p>
      <p>Memory: <b id="head_unit_preset_request_memory">---</b></p>
      <p>CD</p>
      <p>Request: <b id="head_unit_cd_request">---</b></p>
      <p>Track info request: <b id="head_unit_cd_track_info_request">---</b></p>
      <p>Tuner</p>
      <p>Info request: <b id="head_unit_tuner_info_request">---</b></p>
      <p>Tape</p>
      <p>Info request: <b id="head_unit_tape_info_request">---</b></p>
    </div>
    <hr/>
    <div>
      <p>VIN</p>
      <p>Number: <b id="vin">---</b></p>
    </div>
    <hr/>
    <div>
      <p>aircon_diag: <b id="aircon_diag">---</b></p>
      <p>aircon_diag_command: <b id="aircon_diag_command">---</b></p>
    </div>
    <hr/>
  </body>
</html>
)=====";

enum VanPacketFilter_t
{
    VAN_PACKETS_ALL_VAN_PKTS,
    VAN_PACKETS_NO_VAN_PKTS,
    VAN_PACKETS_HEAD_UNIT_PKTS,
    VAN_PACKETS_AIRCON_PKTS,
    VAN_PACKETS_COM2000_ETC_PKTS,
    VAN_PACKETS_SAT_NAV_PKTS
}; // enum VanPacketFilter_t

// serialDumpFilter == 0 means: no filtering; print all
// serialDumpFilter != 0 means: print only the packet + JSON data for the specified IDEN
uint16_t serialDumpFilter;

// Set a simple filter on the dumping of packet + JSON data on Serial.
// Surf to e.g. http://car.lan/dumpOnly?iden=8c4 to have only packets with IDEN 0x8C4 dumped on serial.
// Surf to http://car.lan/dumpOnly?iden=0 to dump all packets.
void HandleDumpFilter()
{
    Serial.print(F("Web server received request from "));
    String ip = webServer.client().remoteIP().toString();
    Serial.print(ip);
    Serial.print(webServer.method() == HTTP_GET ? F(": GET - '") : F(": POST - '"));
    Serial.print(webServer.uri());
    bool found = false;
    if (webServer.args() > 0)
    {
        Serial.print("?");
        for (uint8_t i = 0; i < webServer.args(); i++)
        {
            if (! found && webServer.argName(i) == "iden" && webServer.arg(i).length() <= 3)
            {
                // Invalid conversion results in 0, which is ok: it corresponds to "dump all packets"
                serialDumpFilter = strtol(webServer.arg(i).c_str(), NULL, 16);
                found = true;
            } // if

            Serial.print(webServer.argName(i));
            Serial.print(F("="));
            Serial.print(webServer.arg(i));
            if (i < webServer.args() - 1) Serial.print('&');
        } // for
    } // if
    Serial.print(F("'\n"));

    webServer.send(200, F("text/plain"),
        ! found ? F("NOT OK!") :
            serialDumpFilter == 0 ?
            F("OK: dumping all JSON data") :
            F("OK: filtering JSON data"));
} // HandleDumpFilter

// Defined in Wifi.ino
void SetupWifi();

// Defined in Esp.ino
void PrintSystemSpecs();
const char* EspSystemDataToJson(char* buf, const int n);

// Infrared receiver

// Results returned from the IR decoder
typedef struct
{
    unsigned long value;  // Decoded value
    PGM_P buttonStr;
    int bits;  // Number of bits in decoded value
    volatile unsigned int* rawbuf;  // Raw intervals in 50 usec ticks
    int rawlen;  // Number of records in rawbuf
    bool held;
    unsigned long millis_;
} TIrPacket;

// Defined in IRrecv.ino
void IrSetup();
const char* ParseIrPacketToJson(const TIrPacket& pkt);
bool IrReceive(TIrPacket& irPacket);

// Defined in PacketToJson.ino
extern char jsonBuffer[];
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);
void PrintJsonText(const char* jsonBuffer);

// Create a web socket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

uint8_t websocketNum = 0xFF;

void SendJsonText(const char* json)
{
    if (strlen(json) <= 0) return;
    if (websocketNum == 0xFF) return;

    delay(1); // Give some time to system to process other things?

    unsigned long start = millis();

    //webSocket.broadcastTXT(json);
    // No, serve only the last one connected (the others are probably already dead)
    webSocket.sendTXT(websocketNum, json);

    // Print a message if the websocket transmissino took outrageously long (normally it takes around 1-2 msec).
    // If that takes really long (seconds or more), the VAN bus Rx queue will overrun (remember, ESP8266 is
    // a single-thread system).
    unsigned long duration = millis() - start;
    if (duration > 100)
    {
        Serial.printf_P(
            PSTR("Sending %zu JSON bytes via 'webSocket.sendTXT' took: %lu msec\n"),
            strlen(json),
            duration);

        Serial.print(F("JSON object:\n"));
        PrintJsonText(json);
    } // if
} // SendJsonText

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length)
{
    switch(type)
    {
        case WStype_DISCONNECTED:
        {
            Serial.printf("Websocket [%u] Disconnected!\n", num);
            websocketNum = 0xFF;
        }
        break;

        case WStype_CONNECTED:
        {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("Websocket [%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);

            websocketNum = num;

            // Send ESP system data to client
            SendJsonText(EspSystemDataToJson(jsonBuffer, JSON_BUFFER_SIZE));
        }
        break;
    } // switch
} // webSocketEvent

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.print(F("Starting VAN bus live web page server\n"));

    PrintSystemSpecs();

    SetupWifi();

    webServer.on("/",[](){
        unsigned long start = millis();
        webServer.send_P(200, "text/html", webpage);  
        Serial.printf_P(PSTR("Sending HTML took: %lu msec\n"), millis() - start);
    });
    webServer.on("/dumpOnly", HandleDumpFilter);
    webServer.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    Serial.print(F("Please surf to: http://"));
    Serial.print(WiFi.localIP());
    Serial.print("\n");

#if ! defined VAN_RX_ISR_DEBUGGING && ! defined VAN_RX_IFS_DEBUGGING

    // Having the default VAN packet queue size of 15 (see VanBusRx.h) seems too little given the time that
    // is needed to send a JSON packet over the Wi-Fi; seeing quite some "VAN PACKET QUEUE OVERRUN!" lines.
    // Looks like it should be set to at least 100.
    #define VAN_PACKET_QUEUE_SIZE 100

#else

    // Packet debugging requires a lot of extra memory per slot, so the queue must be small to prevent
    // "out of memory" errors
    #define VAN_PACKET_QUEUE_SIZE 15

#endif

    VanBusRx.Setup(RX_PIN, VAN_PACKET_QUEUE_SIZE);
    Serial.printf_P(PSTR("VanBusRx queue of size %d is set up\n"), VanBusRx.QueueSize());

    IrSetup();
} // setup

void loop()
{
    webSocket.loop();
    webServer.handleClient();

    // IR receiver
    TIrPacket irPacket;
    if (IrReceive(irPacket)) SendJsonText(ParseIrPacketToJson(irPacket));

    // VAN bus receiver
    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;
    if (VanBusRx.Receive(pkt, &isQueueOverrun)) SendJsonText(ParseVanPacketToJson(pkt));
    if (isQueueOverrun) Serial.print(F("VAN PACKET QUEUE OVERRUN!\n"));

    // Print statistics every 5 seconds
    static unsigned long lastUpdate = 0;
    if (millis() - lastUpdate >= 5000UL) // Arithmetic has safe roll-over
    {
        lastUpdate = millis();
        VanBusRx.DumpStats(Serial);
    } // if
} // loop
