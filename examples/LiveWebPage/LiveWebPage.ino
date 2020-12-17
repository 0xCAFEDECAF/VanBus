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
 * - "WebSockets" by Markus Sattler (tested with version 2.2.0)
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

// TODO - use AP mode?
char* ssid = "MyCar"; // Choose yours
char* password = "MyCar"; // Fill in yours

char webpage[] PROGMEM = R"=====(
<html>
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

        // dispatch to the right handlers
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
            if(typeof chain == 'undefined') return; // no callbacks for this event
            for(var i = 0; i < chain.length; i++)
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
            if (!!jsonObj[item] && typeof(jsonObj[item]) == "object")
            {
                //console.log(item, jsonObj[item]);
                writeToDom(jsonObj[item]);
            }
            else
            {
                //console.log(item, jsonObj[item]);
                // Using jQuery: life's too short for raw DOM scripting

                if($('#' + item).length > 0)
                {
                    $('#' + item).text(jsonObj[item]);
                } // if
            } // if
        } // for
    } // writeToDom

    // bind to server events
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
    <p>Engine: <b id="engine">---</b></p>
    <p>Economy mode: <b id="economy_mode">---</b></p>
    <p>In reverse: <b id="in_reverse">---</b></p>
    <p>Trailer: <b id="trailer">---</b></p>
    <p>Coolant temperature: <b id="water_temp">---</b> deg C</p>
    <p>Odometer 1: <b id="odometer_1">---</b></p>
    <p>Odometer 2: <b id="odometer_2">---</b></p>
    <p>Exterior temperature: <b id="exterior_temperature">---</b> deg C</p>
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
    <p>Oil level (dash): <b id="oil_level_dash">---</b></p>
    <p>LPG fuel level: <b id="lpg_fuel_level">---</b></p>
    <p>Cruise control: <b id="cruise_control">---</b></p>
    <p>Cruise control speed: <b id="cruise_control_speed">---</b></p>
  </div>
  <hr/>
  <div>
    <p>Car status</p>
    <p>Doors: <b id="doors">---</b></p>
    <p>Right stalk button: <b id="right_stalk_button">---</b></p>
    <p>Trip 1 average speed: <b id="avg_speed_1">---</b> km/h</p>
    <p>Trip 2 average speed: <b id="avg_speed_2">---</b> km/h</p>
    <p>Average speed (EMA): <b id="exp_moving_avg_speed">---</b> km/h</p>
    <p>Trip 1 range: <b id="range_1">---</b> km</p>
    <p>Trip 1 average fuel consumption: <b id="avg_consumption_1">---</b> lt/100 km</p>
    <p>Trip 2 range: <b id="range_2">---</b> km</p>
    <p>Trip 2 average fuel consumption: <b id="avg_consumption_2">---</b> lt/100 km</p>
    <p>Current fuel consumption: <b id="inst_consumption">---</b> lt/100 km</p>
    <p>Remaining distance in fuel tank: <b id="mileage">---</b> km</p>
  </div>
  <hr/>
  <div>
    <p>Dashboard</p>
    <p>Engine RPM: <b id="rpm">---</b> /min</p>
    <p>Vehicle speed: <b id="speed">---</b> km/h</p>
  </div>
  <hr/>
  <div>
    <p>Dashboard Buttons</p>
    <p>Hazard lights: <b id="hazard_lights">---</b></p>
    <p>Door lock: <b id="door_lock">---</b></p>
    <p>Dash light - set brightness: <b id="dashboard_programmed_brightness">---</b></p>
    <p>ESP: <b id="esp">---</b></p>
    <p>Fuel level (filtered): <b id="fuel_level_filtered">---</b> litres</p>
    <p>Fuel level (raw): <b id="fuel_level_raw">---</b> litres</p>
  </div>
  <hr/>
  <div>
    <p>Head Unit</p>
    <p>Power: <b id="power">---</b></p>
    <p>Tape: <b id="tape">---</b></p>
    <p>CD: <b id="cd">---</b></p>
    <p>Source: <b id="source">---</b></p>
    <p>External mute: <b id="ext_mute">---</b></p>
    <p>Mute: <b id="mute">---</b></p>
    <p>Volume: <b id="volume">---</b></p>
    <p>Audio menu: <b id="audio_menu">---</b></p>
    <p>Bass: <b id="bass">---</b></p>
    <p>Treble: <b id="treble">---</b></p>
    <p>Loudness: <b id="loudness">---</b></p>
    <p>Balance: <b id="balance">---</b></p>
    <p>Fader: <b id="fader">---</b></p>
    <p>Auto volume: <b id="auto_volume">---</b></p>
    <p>Buttons pressed: <b id="head_unit_button_pressed">---</b></p>
  </div>
  <hr/>
  <div>
    <p>Radio</p>
    <p>Band: <b id="band">---</b></p>
    <p>Preset: <b id="preset">---</b></p>
    <p>Frequency: <b id="frequency">---</b><b id="frequency_h">-</b> <b id="frequency_unit"></b></p>
    <p>Signal strength: <b id="signal_strength">---</b></p>
    <p>Scanning: <b id="scan_mode">---</b></p>
    <p>Scan sensitivity: <b id="scan_sensitivity">---</b></p>
    <p>Scan direction: <b id="scan_direction">---</b></p>
    <p>PTY selection menu: <b id="pty_selection_menu">---</b></p>
    <p>PTY selected: <b id="selected_pty">---</b></p>
    <p>PTY standby mode: <b id="pty_standby_mode">---</b></p>
    <p>PTY match: <b id="pty_match">---</b></p>
    <p>PTY: <b id="pty">---</b></p>
    <p>PI: <b id="pi">---</b></p>
    <p>Regional: <b id="regional">---</b></p>
    <p>TA available: <b id="ta_available">---</b></p>
    <p>RDS available: <b id="rds_available">---</b></p>
    <p>TA: <b id="ta_selected">---</b></p>
    <p>RDS: <b id="rds_selected">---</b></p>
    <p>RDS Text: <b id="rds_text">---</b></p>
    <p>Traffic Info: <b id="info_trafic">---</b></p>
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
    <p>Current track - minutes: <b id="cd_track_min">---</b></p>
    <p>Current track - seconds: <b id="cd_track_sec">---</b></p>
    <p>Number of tracks: <b id="cd_total_tracks">---</b></p>
    <p>Total duration: <b id="cd_total_mins">---</b> minutes</p>
  </div>
  <hr/>
  <div>
    <p>CD Changer</p>
    <p>Command: <b id="cd_changer_command">---</b></p>
    <p>Status: <b id="cdc_state">---</b></p>
    <p>Random mode: <b id="cdc_random">---</b></p>
    <p>Cartridge: <b id="cdc_cartridge">---</b></p>
    <p>Current track: <b id="cdc_current_track">---</b></p>
    <p>Current track - minutes: <b id="cdc_track_time_min">---</b></p>
    <p>Current track - seconds: <b id="cdc_track_time_sec">---</b></p>
    <p>Number of tracks: <b id="cdc_total_tracks">---</b></p>
    <p>Current CD: <b id="cdc_current_cd">---</b></p>
    <p>CD 1 present: <b id="cdc_disc_1_present">---</b></p>
    <p>CD 2 present: <b id="cdc_disc_2_present">---</b></p>
    <p>CD 3 present: <b id="cdc_disc_3_present">---</b></p>
    <p>CD 4 present: <b id="cdc_disc_4_present">---</b></p>
    <p>CD 5 present: <b id="cdc_disc_5_present">---</b></p>
    <p>CD 6 present: <b id="cdc_disc_6_present">---</b></p>
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
    <p>Contact key position: <b id="contact_key_position">---</b></p>
    <p>Condenser temperature: <b id="condenser_temperature">---</b> deg C</p>
    <p>Evaporator temperature: <b id="evaporator_temperature">---</b> deg C</p>
  </div>
  <hr/>
  <div>
    <p>Satellite Navigation</p>
    <p>Status 1: <b id="satnav_status_1">---</b></p>
    <p>Status 2: <b id="satnav_status_2">---</b></p>
    <p>Status 3: <b id="satnav_status_3">---</b></p>
    <p>Disc: <b id="satnav_disc">---</b></p>
    <p>Disc status: <b id="satnav_disc_status">---</b></p>
    <p>GPS fix: <b id="satnav_gps_fix">---</b></p>
    <p>GPS fix list: <b id="satnav_gps_fix_lost">---</b></p>
    <p>GPS scanning: <b id="satnav_gps_scanning">---</b></p>
    <p>GPS speed: <b id="satnav_gps_speed">---</b></p>
    <p>ZZZ: <b id="satnav_zzz">---</b></p>
  </div>
  <hr/>
  <div>
    <p>Guidance data</p>
    <p>Current heading: <b id="satnav_curr_heading">---</b></p>
    <p>Heading to destination: <b id="satnav_heading_to_dest">---</b></p>
    <p>Distance to destination: <b id="satnav_distance_to_dest">---</b></p>
    <p>Straight line to destination: <b id="satnav_distance_to_dest_straight_line">---</b></p>
    <p>Turn at: <b id="satnav_turn_at">---</b></p>
    <p>Heading on roundabout: <b id="satnav_heading_on_roundabout">---</b></p>
    <p>Minutes to travel (?): <b id="satnav_minutes_to_travel">---</b></p>
  </div>
  <hr/>
  <div>
    <p>COM 2000</p>
    <p>Light switch: <b id="com2000_light_switch">---</b></p>
    <p>Right stalk: <b id="com2000_right_stalk">---</b></p>
    <p>Turn signal: <b id="com2000_turn_signal">---</b></p>
    <p>Head unit stalk: <b id="com2000_head_unit_stalk">---</b></p>
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
    <p>Update volume 1: <b id="head_unit_update_volume">---</b></p>
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
    <p>engine: <b id="engine">---</b></p>
    <p>audio_stalk: <b id="audio_stalk">---</b></p>
    <p>lights_status: <b id="lights_status">---</b></p>
    <p>head_unit_buttons: <b id="head_unit_buttons">---</b></p>
    <p>car_status_1: <b id="car_status_1">---</b></p>
    <p>car_status_2: <b id="car_status_2">---</b></p>
    <p>dashboard: <b id="dashboard">---</b></p>
    <p>dashboard_buttons: <b id="dashboard_buttons">---</b></p>
    <p>head_unit: <b id="head_unit">---</b></p>
    <p>time: <b id="time">---</b></p>
    <p>audio_settings: <b id="audio_settings">---</b></p>
    <p>display_status: <b id="display_status">---</b></p>
    <p>stalk_or_satnav: <b id="stalk_or_satnav">---</b></p>
    <p>aircon_1: <b id="aircon_1">---</b></p>
    <p>aircon_2: <b id="aircon_2">---</b></p>
    <p>cd_changer: <b id="cd_changer">---</b></p>
    <p>satnav_1: <b id="satnav_1">---</b></p>
    <p>satnav_2: <b id="satnav_2">---</b></p>
    <p>satnav_3: <b id="satnav_3">---</b></p>
    <p>satnav_4: <b id="satnav_4">---</b></p>
    <p>satnav_5: <b id="satnav_5">---</b></p>
    <p>satnav_6: <b id="satnav_6">---</b></p>
    <p>satnav_7: <b id="satnav_7">---</b></p>
    <p>wheel_speed: <b id="wheel_speed">---</b></p>
    <p>odometer: <b id="odometer">---</b></p>
    <p>com2000: <b id="com2000">---</b></p>
    <p>cd_changer_command: <b id="cd_changer_command">---</b></p>
    <p>display_to_head_unit: <b id="display_to_head_unit">---</b></p>
    <p>aircon_diag: <b id="aircon_diag">---</b></p>
    <p>aircon_diag_command: <b id="aircon_diag_command">---</b></p>
  </div>
  <hr/>
</body>
</html>
)=====";

const char* getHostname()
{
    return "Car";
} // getHostname

void setup()
{
    delay(1000);
    Serial.begin(115200);
    Serial.println(F("Starting VAN bus live web page server"));

    Serial.printf_P(PSTR("Connecting to Wifi SSID '%s' "), ssid);

    // TODO - move to after WiFi.status() == WL_CONNECTED ?
    WiFi.hostname(getHostname());

    WiFi.begin(ssid,password);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial.println(F(" OK"));

    webServer.on("/",[](){
        webServer.send_P(200, "text/html", webpage);  
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
    if (VanBusRx.Receive(pkt))
    {
        const char* json = ParseVanPacketToJson(pkt);
        if (strlen(json) > 0) webSocket.broadcastTXT(json);
    } // if

    delay(50);
} // loop
