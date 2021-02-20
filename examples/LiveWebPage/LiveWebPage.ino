/*
 * VanBus: LiveWebPage - Show all received information from the VAN bus on a "live" web page.
 *
 * Written by Erik Tromp
 *
 * Version 0.2.0 - December, 2020
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
 * page is updated continuously ("live") using a web socket (on port 81). Data is broadcast by the VAN bus receiver, in
 * JSON format, which is then picked up on the client side by a JavaScript dispatcher script (served by the same web
 * page). The script uses jQuery to place the incoming data on the correct spot in the DOM.
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
 * - "WebSockets" by Markus Sattler (https://github.com/Links2004/arduinoWebSockets) 
 *   --> Tested with version 2.2.0 and 2.3.3 .
 *
 * The web site itself, as served by this sketch, uses jQuery, which it downloads from:
 * https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js
 * If you don't like Google tracking your surfing, you could use Firefox and install the 'LocalCDN' extension (see
 * https://addons.mozilla.org/nl/firefox/addon/localcdn-fork-of-decentraleyes ).
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <VanBusRx.h>

#if defined ARDUINO_ESP8266_GENERIC || defined ARDUINO_ESP8266_ESP01
// For ESP-01 board we use GPIO 2 (internal pull-up, keep disconnected or high at boot time)
#define D2 (2)
#endif

int RX_PIN = D2; // Set to GPIO pin connected to VAN bus transceiver output

ESP8266WebServer webServer;
WebSocketsServer webSocket = WebSocketsServer(81);

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
          };

          // Dispatch to the right handlers
          conn.onmessage = function(evt)
          {
              var json = JSON.parse(evt.data)
              dispatch(json.event, json.data)
          };

          conn.onclose = function(){dispatch('close',null)}
          conn.onopen = function(){dispatch('open',null)}

          var dispatch = function(event_name, message)
          {
              var chain = callbacks[event_name];
              if (typeof chain == 'undefined') return; // No callbacks for this event
              for (var i = 0; i < chain.length; i++)
              {
                  chain[i](message)
              } // for
          } // function
      };

      var socket = new FancyWebSocket('ws://' + window.location.hostname + ':81/');

      function writeToDom(jsonObj)
      {
          for (var item in jsonObj)
          {
              console.log(item, jsonObj[item]);

              if (Array.isArray(jsonObj[item]))
              {
                  // Handling of "text area" DOM objects to show lists

                  var selector = '#' + item;
                  if($(selector).length > 0)
                  {
                      // Remove current lines
                      $(selector).val("");

                      var len = jsonObj[item].length;
                      for (var i = 0; i < len; i++)
                      {
                          //console.log("appended", jsonObj[item][i]);

                          $(selector).val($(selector).val() + jsonObj[item][i] + "\n");
                      } // for
                  } // if
              }
              else if (!!jsonObj[item] && typeof(jsonObj[item]) == "object")
              {
                  // Handling of "change attribute" events. Examples:
                  //
                  // {
                  //   "event": "display",
                  //   "data": {
                  //     "satnav_curr_heading_svg": { "transform": "rotate(247.5)" }
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

                  var selector = '#' + item;
                  if($(selector).length > 0)
                  {
                      var attributeObj = jsonObj[item];
                      for (var attribute in attributeObj)
                      {
                          var attrValue = attributeObj[attribute];

                          // Deeper nesting?
                          if (typeof(attributeObj) == "object")
                          {
                              var propertyObj = attributeObj[attribute];
                              for (var property in propertyObj)
                              {
                                  var value = propertyObj[property];
                                  document.getElementById(item)[attribute][property] = value;
                              } // for
                          }
                          else
                          {
                              //document.getElementById(item).setAttribute(attribute, attrValue);
                              $(selector).attr(attribute, attrValue);
                          } // if
                      } // for
                  } // if
              }
              else
              {
                  // Using jQuery: life's too short for raw DOM scripting

                  var selector = '#' + item;
                  if($(selector).length > 0)
                  {
                      $(selector).text(jsonObj[item]);
                  } // if
              } // if
          } // for
      } // writeToDom

      // Bind to server events
      socket.bind(
          'display',
          function(data)
          {
              writeToDom(data);
          } // function
      );

    </script>
    <script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script>
  </head>
  <body>
    <div>
      <p>Engine</p>
      <p>Dash light: <b id="dash_light">---</b></p>
      <p>Dash light - actual brightness: <b id="dash_actual_brightness">---</b></p>
      <p>Contact key position: <b id="contact_key_position">---</b></p>
      <p>Engine running: <b id="engine_running">---</b></p>
      <p>Economy mode: <b id="economy_mode">---</b></p>
      <p>In reverse: <b id="in_reverse">---</b></p>
      <p>Trailer: <b id="trailer">---</b></p>

      <p>Coolant temperature: <b id="water_temp">---</b> deg C</p>

      <div style="height:40px;">
        <!-- Linear gauge -->
        <div id="water_temp_perc" style="height:30px; transform:scaleX(0.6); transform-origin:left center;">
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
      <p>Exterior temperature: <b id="exterior_temperature">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Head Unit Stalk</p>
      <p>Buttons: <b id="head_unit_stalk_buttons">---</b></p>
      <p>Wheel: <b id="head_unit_stalk_wheel">---</b></p>
      <p>Wheel rollover: <b id="head_unit_stalk_wheel_rollover">---</b></p>
    </div>
    <hr/>
    <div>
      <p>Lights</p>
      <p>Instrument cluster: <b id="instrument_cluster">---</b></p>
      <p>Speed regulator wheel: <b id="speed_regulator_wheel">---</b></p>
      <p>Warning LED: <b id="warning_led">---</b></p>
      <p>Diesel glow plugs: <b id="diesel_glow_plugs">---</b></p>
      <p>Door open: <b id="door_open">---</b></p>
      <p>Service after km: <b id="remaining_km_to_service">---</b></p>
      <p>Service after km (dash): <b id="remaining_km_to_service_dash">---</b></p>
      <p>Lights: <b id="lights">---</b></p>
      <p>Auto gearbox: <b id="auto_gearbox">---</b></p>
      <p>Oil temperature: <b id="oil_temperature">---</b></p>
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
      <p>Trip 1 average fuel consumption: <b id="avg_consumption_lt_100_1">---</b> lt/100 km</p>
      <p>Trip 2 distance: <b id="distance_2">---</b> km</p>
      <p>Trip 2 average fuel consumption: <b id="avg_consumption_lt_100_2">---</b> lt/100 km</p>
      <p>Current fuel consumption: <b id="inst_consumption_lt_100">---</b> lt/100 km</p>
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
      <textarea id="alarm_list" rows="10" cols="80"></textarea>
      <div id="notification_on_mfd" style="display:none">
        <p>Message shown on multi-function display: <b id="message_displayed_on_mfd">---</b></p>
      </div>
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
      <p>Hazard lights: <b id="hazard_lights">---</b></p>
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
      <p>Report: <b id="head_unit_report">---</b></p>
      <p>Power: <b id="power">---</b></p>
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
      <p>Frequency: <b id="frequency">---</b><b id="frequency_h">-</b> <b id="frequency_unit"></b></p>
      <p>Signal strength: <b id="signal_strength">---</b></p>
      <p>Tuner sensitivity: <b id="search_sensitivity">---</b></p>
      <p>Searching: <b id="search_mode">---</b></p>
      <p>Search direction: <b id="search_direction">---</b></p>
      <p>PTY selection menu: <b id="pty_selection_menu">---</b></p>
      <p>PTY selected: <b id="selected_pty">---</b></p>
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
      <p>Command: <b id="cd_changer_command">---</b></p>
      <p>Status: <b id="cd_changer_status">---</b></p>
      <p>Random mode: <b id="cd_changer_random">---</b></p>
      <p>Cartridge: <b id="cd_changer_cartridge_present">---</b></p>
      <p>Current track time: <b id="cd_changer_track_time">---</b></p>
      <p>Current track: <b id="cd_changer_current_track">---</b></p>
      <p>Number of tracks: <b id="cd_changer_total_tracks">---</b></p>
      <p>Current CD: <b id="cd_changer_current_cd">---</b></p>
      <p>CD 1 present: <b id="cd_changer_disc_1_present">---</b></p>
      <p>CD 2 present: <b id="cd_changer_disc_2_present">---</b></p>
      <p>CD 3 present: <b id="cd_changer_disc_3_present">---</b></p>
      <p>CD 4 present: <b id="cd_changer_disc_4_present">---</b></p>
      <p>CD 5 present: <b id="cd_changer_disc_5_present">---</b></p>
      <p>CD 6 present: <b id="cd_changer_disc_6_present">---</b></p>
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
      <p>Condenser temperature: <b id="condenser_temperature">---</b> deg C</p>
      <p>Evaporator temperature: <b id="evaporator_temperature">---</b> deg C</p>
    </div>
    <hr/>
    <div>
      <p>Satellite Navigation</p>
      <p>Status 1: <b id="satnav_status_1">---</b></p>
      <p>Status 2: <b id="satnav_status_2">---</b></p>
      <p>Status 3: <b id="satnav_status_3">---</b></p>
      <p>Disc: <b id="satnav_disc_present">---</b></p>
      <p>Disc status: <b id="satnav_disc_status">---</b></p>
      <p>System ID</p>
      <textarea id="satnav_system_id" rows="5" cols="80"></textarea>
      <p>GPS fix: <b id="satnav_gps_fix">---</b></p>
      <p>GPS fix list: <b id="satnav_gps_fix_lost">---</b></p>
      <p>GPS scanning: <b id="satnav_gps_scanning">---</b></p>
      <p>GPS speed: <b id="satnav_gps_speed">---</b></p>
      <p>ZZZ (?): <b id="satnav_zzz">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav guidance data</p>

      <p>Current heading: <b id="satnav_curr_heading_as_text">---</b></p>

      <!-- Rotating round an abstract transform-origin like 'center' is better supported for a <div> than an <svg> element -->
      <div id="satnav_curr_heading" style="width:80px; height:120px; transform:rotate(65deg); transform-origin:center;">
        <svg>
          <path style="stroke-width:8;" d="M40 15 l30 100 l-60 0 Z"/>
        </svg>
      </div>

      <p>Heading to destination: <b id="satnav_heading_to_dest_as_text">---</b></p>
      <svg>
        <path id="satnav_heading_to_dest" style="stroke-width:8;" d="M100 10 l30 100 l-60 0 Z" transform="rotate(22.5)" transform-origin="100 65"/>
      </svg>

      <p>Road distance to destination: <b id="satnav_distance_to_dest_via_road">---</b> (unit is meters: <b id="satnav_distance_to_dest_via_road_m">---</b>, (unit is kilometers: <b id="satnav_distance_to_dest_via_road_km">---</b>)</p>
      <p>Straight line to destination: <b id="satnav_distance_to_dest_via_straight_line">---</b> (unit is meters: <b id="satnav_distance_to_dest_via_straight_line_m">---</b>, (unit is kilometers: <b id="satnav_distance_to_dest_via_straight_line_km">---</b>)</p>
      <p>Turn at: <b id="satnav_turn_at">---</b> (unit is meters: <b id="satnav_turn_at_m">---</b>, (unit is kilometers: <b id="satnav_turn_at_km">---</b>)</p>
      <p>Heading on roundabout: <b id="satnav_heading_on_roundabout_as_text">---</b></p>
      <p>Minutes to travel (?): <b id="satnav_minutes_to_travel">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav guidance instruction</p>
      <p>Current turn icon: <b style="display:none;" id="satnav_curr_turn_icon">VISIBLE</b></p>
      <p>Next turn icon: <b style="display:none;" d="satnav_next_turn_icon">VISIBLE</b></p>
      <p>'Turn around if possible' icon: <b style="display:none;" id="satnav_turn_around_if_possible_icon">VISIBLE</b></p>
      <p>'Follow road' icon: <b style="display:none;" id="satnav_follow_road_icon">VISIBLE</b></p>
      <p>'Not on map' icon: <b style="display:none;" id="satnav_not_on_map_icon">VISIBLE</b></p>
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
      <p>'Not on map' icon - follow heading: <b id="satnav_not_on_map_follow_heading">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav report</p>
      <p>Report: <b id="satnav_report">---</b></p>
      <p>Current street: <b id="satnav_curr_street">---</b></p>
      <p>Next street: <b id="satnav_next_street">---</b></p>
      <p>Destination address: <b id="satnav_destination_address">---</b></p>
      <p>Current address: <b id="satnav_current_address">---</b></p>
      <p>Private address entry: <b id="satnav_private_address_entry">---</b></p>
      <p>Private address: <b id="satnav_private_address">---</b></p>
      <p>Business address entry: <b id="satnav_business_address_entry">---</b></p>
      <p>Business address: <b id="satnav_business_address">---</b></p>
      <p>Place of interest address entry: <b id="satnav_place_of_interest_address_entry">---</b></p>
      <p>Place of interest address: <b id="satnav_place_of_interest_address">---</b></p>
      <p>Place of interest address distance: <b id="satnav_place_of_interest_address_distance">---</b></p>
      <p>Address list</p>
      <textarea id="satnav_list" rows="5" cols="80"></textarea>
      <p>House number range: <b id="satnav_house_number_range">---</b></p>
      <p>Places of interest category list</p>
      <textarea id="satnav_place_of_interest_category_list" rows="10" cols="80"></textarea>
      <p>Software module list</p>
      <textarea id="satnav_software_modules_list" rows="5" cols="80"></textarea>
    </div>
    <hr/>
    <div>
      <p>Multi-function display to satnav</p>
      <p>Request: <b id="mfd_to_satnav_request">---</b></p>
      <p>Request type: <b id="mfd_to_satnav_request_type">---</b></p>
      <p>Character: <b id="mfd_to_satnav_character">---</b></p>
      <p>List offset: <b id="mfd_to_satnav_offset">---</b></p>
      <p>List length: <b id="mfd_to_satnav_length">---</b></p>
      <p>List selection: <b id="mfd_to_satnav_selection">---</b></p>
    </div>
    <hr/>
    <div>
      <p>SatNav to multi-function display</p>
      <p>Response: <b id="satnav_to_mfd_response">---</b></p>
      <p>List size: <b id="satnav_to_mfd_list_size">---</b></p>
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
      <p>Turn signal: <b id="com2000_turn_signal_left">---</b></p>
      <p>Turn signal: <b id="com2000_turn_signal_right">---</b></p>
      <p>Head unit stalk: <b id="com2000_head_unit_stalk_button_src">---</b></p>
      <p>Head unit stalk: <b id="com2000_head_unit_stalk_button_volume_up">---</b></p>
      <p>Head unit stalk: <b id="com2000_head_unit_stalk_button_volume_down">---</b></p>
      <p>Head unit stalk: <b id="com2000_head_unit_stalk_button_seek_backward">---</b></p>
      <p>Head unit stalk: <b id="com2000_head_unit_stalk_button_seek_forward">---</b></p>
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

// Defined in Wifi.ino
void setupWifi();

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.println(F("Starting VAN bus live web page server"));

    setupWifi();

    webServer.on("/",[](){
        unsigned long start = millis();
        webServer.send_P(200, "text/html", webpage);  
        Serial.printf_P(PSTR("Sending HTML took: %lu msec\n"), millis() - start);
    });
    webServer.begin();
    webSocket.begin();

    Serial.print(F("Please surf to: http://"));
    Serial.println(WiFi.localIP());

    VanBusRx.Setup(RX_PIN);
} // setup

// Defined in PacketToJson.ino
const char* ParseVanPacketToJson(TVanPacketRxDesc& pkt);

void loop()
{
    webSocket.loop();
    webServer.handleClient();

    TVanPacketRxDesc pkt;
    bool isQueueOverrun = false;

    if (VanBusRx.Receive(pkt, &isQueueOverrun))
    {
        const char* json = ParseVanPacketToJson(pkt);
        if (strlen(json) > 0)
        {
            unsigned long start = millis();
            webSocket.broadcastTXT(json);
            Serial.printf_P(
                PSTR("Sending %zu JSON bytes via 'webSocket.broadcastTXT' took: %lu msec\n"),
                strlen(json),
                millis() - start);
        } // if
    } // if

    if (isQueueOverrun) Serial.print(F("QUEUE OVERRUN!\n"));

    delay(1); // Give some time to system to process other things?
} // loop
