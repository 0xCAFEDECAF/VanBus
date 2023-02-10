
// Simple packet filtering on IDEN

#include "VanIden.h"
#include "Config.h"

// Filter on specific IDENs
bool IsPacketSelected(uint16_t iden, VanPacketFilter_t filter)
{
    if (filter == VAN_PACKETS_ALL_VAN_PKTS)
    {
        // Show all packets, but discard the following:
        // (Commented-out IDENs will be printed on Serial)
        if (
            false
            // || iden == VIN_IDEN
            // || iden == ENGINE_IDEN
            // || iden == HEAD_UNIT_STALK_IDEN
            // || iden == LIGHTS_STATUS_IDEN
            // || iden == DEVICE_REPORT
            // || iden == CAR_STATUS1_IDEN
            // || iden == CAR_STATUS2_IDEN
            // || iden == DASHBOARD_IDEN
            // || iden == DASHBOARD_BUTTONS_IDEN
            // || iden == HEAD_UNIT_IDEN
            // || iden == MFD_LANGUAGE_UNITS_IDEN
            // || iden == AUDIO_SETTINGS_IDEN
            // || iden == MFD_STATUS_IDEN
            // || iden == AIRCON1_IDEN
            // || iden == AIRCON2_IDEN
            // || iden == CDCHANGER_IDEN
            // || iden == SATNAV_STATUS_1_IDEN
            // || iden == SATNAV_STATUS_2_IDEN
            // || iden == SATNAV_STATUS_3_IDEN
            // || iden == SATNAV_GUIDANCE_DATA_IDEN
            // || iden == SATNAV_GUIDANCE_IDEN
            // || iden == SATNAV_REPORT_IDEN
            // || iden == MFD_TO_SATNAV_IDEN
            // || iden == SATNAV_TO_MFD_IDEN
            // || iden == SATNAV_DOWNLOADING_IDEN
            // || iden == SATNAV_DOWNLOADED1_IDEN
            // || iden == SATNAV_DOWNLOADED2_IDEN
            // || iden == WHEEL_SPEED_IDEN
            // || iden == ODOMETER_IDEN
            // || iden == COM2000_IDEN
            // || iden == CDCHANGER_COMMAND_IDEN
            // || iden == MFD_TO_HEAD_UNIT_IDEN
            // || iden == AIR_CONDITIONER_DIAG_IDEN
            // || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            // || iden == SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_NO_VAN_PKTS)
    {
        // Show no packets, except the following:
        // (Commented-out IDENs will NOT be printed on Serial)
        if (
            true
            // && iden != VIN_IDEN
            // && iden != ENGINE_IDEN
            // && iden != HEAD_UNIT_STALK_IDEN
            // && iden != LIGHTS_STATUS_IDEN
            // && iden != DEVICE_REPORT
            // && iden != CAR_STATUS1_IDEN
            // && iden != CAR_STATUS2_IDEN
            // && iden != DASHBOARD_IDEN
            // && iden != DASHBOARD_BUTTONS_IDEN
            // && iden != HEAD_UNIT_IDEN
            // && iden != MFD_LANGUAGE_UNITS_IDEN
            // && iden != AUDIO_SETTINGS_IDEN
            // && iden != MFD_STATUS_IDEN
            // && iden != AIRCON1_IDEN
            // && iden != AIRCON2_IDEN
            // && iden != CDCHANGER_IDEN
            // && iden != SATNAV_STATUS_1_IDEN
            // && iden != SATNAV_STATUS_2_IDEN
            // && iden != SATNAV_STATUS_3_IDEN
            // && iden != SATNAV_GUIDANCE_DATA_IDEN
            // && iden != SATNAV_GUIDANCE_IDEN
            // && iden != SATNAV_REPORT_IDEN
            // && iden != MFD_TO_SATNAV_IDEN
            // && iden != SATNAV_TO_MFD_IDEN
            // && iden != SATNAV_DOWNLOADING_IDEN
            // && iden != SATNAV_DOWNLOADED1_IDEN
            // && iden != SATNAV_DOWNLOADED2_IDEN
            // && iden != WHEEL_SPEED_IDEN
            // && iden != ODOMETER_IDEN
            // && iden != COM2000_IDEN
            // && iden != CDCHANGER_COMMAND_IDEN
            // && iden != MFD_TO_HEAD_UNIT_IDEN
            // && iden != AIR_CONDITIONER_DIAG_IDEN
            // && iden != AIR_CONDITIONER_DIAG_COMMAND_IDEN
            // && iden != SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_HEAD_UNIT_PKTS)
    {
        // Head unit packets (plus anything not recognized)
        // (Commented-out IDENs will be printed on Serial)
        if (
            false
            || iden == VIN_IDEN
            || iden == ENGINE_IDEN
            // || iden == HEAD_UNIT_STALK_IDEN
            || iden == LIGHTS_STATUS_IDEN
            // || iden == DEVICE_REPORT
            || iden == CAR_STATUS1_IDEN
            || iden == CAR_STATUS2_IDEN
            || iden == DASHBOARD_IDEN
            || iden == DASHBOARD_BUTTONS_IDEN
            // || iden == HEAD_UNIT_IDEN
            || iden == MFD_LANGUAGE_UNITS_IDEN
            // || iden == AUDIO_SETTINGS_IDEN
            // || iden == MFD_STATUS_IDEN
            || iden == AIRCON1_IDEN
            || iden == AIRCON2_IDEN
            // || iden == CDCHANGER_IDEN
            || iden == SATNAV_STATUS_1_IDEN
            || iden == SATNAV_STATUS_2_IDEN
            || iden == SATNAV_STATUS_3_IDEN
            || iden == SATNAV_GUIDANCE_DATA_IDEN
            || iden == SATNAV_GUIDANCE_IDEN
            || iden == SATNAV_REPORT_IDEN
            || iden == MFD_TO_SATNAV_IDEN
            || iden == SATNAV_TO_MFD_IDEN
            || iden == SATNAV_DOWNLOADING_IDEN
            || iden == SATNAV_DOWNLOADED1_IDEN
            || iden == SATNAV_DOWNLOADED2_IDEN
            || iden == WHEEL_SPEED_IDEN
            || iden == ODOMETER_IDEN
            // || iden == COM2000_IDEN
            // || iden == CDCHANGER_COMMAND_IDEN
            // || iden == MFD_TO_HEAD_UNIT_IDEN
            || iden == AIR_CONDITIONER_DIAG_IDEN
            || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            || iden == SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_AIRCON_PKTS)
    {
        // Aircon packets (plus anything not recognized)
        // (Commented-out IDENs will be printed on Serial)
        if (
            false
            || iden == VIN_IDEN
            // || iden == ENGINE_IDEN
            || iden == HEAD_UNIT_STALK_IDEN
            || iden == LIGHTS_STATUS_IDEN
            || iden == DEVICE_REPORT
            // || iden == CAR_STATUS1_IDEN
            || iden == CAR_STATUS2_IDEN
            || iden == DASHBOARD_IDEN
            || iden == DASHBOARD_BUTTONS_IDEN
            || iden == HEAD_UNIT_IDEN
            || iden == MFD_LANGUAGE_UNITS_IDEN
            || iden == AUDIO_SETTINGS_IDEN
            || iden == MFD_STATUS_IDEN
            // || iden == AIRCON1_IDEN
            // || iden == AIRCON2_IDEN
            || iden == CDCHANGER_IDEN
            || iden == SATNAV_STATUS_1_IDEN
            || iden == SATNAV_STATUS_2_IDEN
            || iden == SATNAV_STATUS_3_IDEN
            || iden == SATNAV_GUIDANCE_DATA_IDEN
            || iden == SATNAV_GUIDANCE_IDEN
            || iden == SATNAV_REPORT_IDEN
            || iden == MFD_TO_SATNAV_IDEN
            || iden == SATNAV_TO_MFD_IDEN
            || iden == SATNAV_DOWNLOADING_IDEN
            || iden == SATNAV_DOWNLOADED1_IDEN
            || iden == SATNAV_DOWNLOADED2_IDEN
            || iden == WHEEL_SPEED_IDEN
            || iden == ODOMETER_IDEN
            || iden == COM2000_IDEN
            || iden == CDCHANGER_COMMAND_IDEN
            || iden == MFD_TO_HEAD_UNIT_IDEN
            // || iden == AIR_CONDITIONER_DIAG_IDEN
            // || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            || iden == SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_COM2000_ETC_PKTS)
    {
        // COM2000 packets and surroundings (plus anything not recognized)
        // (Commented-out IDENs will be printed on Serial)
        if (
            false
            || iden == VIN_IDEN
            // || iden == ENGINE_IDEN
            // || iden == HEAD_UNIT_STALK_IDEN
            // || iden == LIGHTS_STATUS_IDEN
            || iden == DEVICE_REPORT
            // || iden == CAR_STATUS1_IDEN
            || iden == CAR_STATUS2_IDEN
            // || iden == DASHBOARD_IDEN
            // || iden == DASHBOARD_BUTTONS_IDEN
            || iden == HEAD_UNIT_IDEN
            || iden == MFD_LANGUAGE_UNITS_IDEN
            || iden == AUDIO_SETTINGS_IDEN
            // || iden == MFD_STATUS_IDEN
            || iden == AIRCON1_IDEN
            || iden == AIRCON2_IDEN
            || iden == CDCHANGER_IDEN
            || iden == SATNAV_STATUS_1_IDEN
            || iden == SATNAV_STATUS_2_IDEN
            || iden == SATNAV_STATUS_3_IDEN
            || iden == SATNAV_GUIDANCE_DATA_IDEN
            || iden == SATNAV_GUIDANCE_IDEN
            || iden == SATNAV_REPORT_IDEN
            || iden == MFD_TO_SATNAV_IDEN
            || iden == SATNAV_TO_MFD_IDEN
            || iden == SATNAV_DOWNLOADING_IDEN
            || iden == SATNAV_DOWNLOADED1_IDEN
            || iden == SATNAV_DOWNLOADED2_IDEN
            || iden == WHEEL_SPEED_IDEN
            // || iden == ODOMETER_IDEN
            // || iden == COM2000_IDEN
            || iden == CDCHANGER_COMMAND_IDEN
            || iden == MFD_TO_HEAD_UNIT_IDEN
            || iden == AIR_CONDITIONER_DIAG_IDEN
            || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            || iden == SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_SAT_NAV_PKTS)
    {
        // SatNav packets (plus anything not recognized)
        // (Commented-out IDENs will be printed on Serial)
        if (
            false
            || iden == VIN_IDEN
            || iden == ENGINE_IDEN
            || iden == HEAD_UNIT_STALK_IDEN
            || iden == LIGHTS_STATUS_IDEN
            // || iden == DEVICE_REPORT
            || iden == CAR_STATUS1_IDEN
            || iden == CAR_STATUS2_IDEN
            || iden == DASHBOARD_IDEN
            || iden == DASHBOARD_BUTTONS_IDEN
            || iden == HEAD_UNIT_IDEN
            // || iden == MFD_LANGUAGE_UNITS_IDEN
            || iden == AUDIO_SETTINGS_IDEN
            || iden == MFD_STATUS_IDEN
            || iden == AIRCON1_IDEN
            || iden == AIRCON2_IDEN
            || iden == CDCHANGER_IDEN
            // || iden == SATNAV_STATUS_1_IDEN
            // || iden == SATNAV_STATUS_2_IDEN
            // || iden == SATNAV_STATUS_3_IDEN
            // || iden == SATNAV_GUIDANCE_DATA_IDEN
            // || iden == SATNAV_GUIDANCE_IDEN
            // || iden == SATNAV_REPORT_IDEN
            // || iden == MFD_TO_SATNAV_IDEN
            // || iden == SATNAV_TO_MFD_IDEN
            // || iden == SATNAV_DOWNLOADING_IDEN
            // || iden == SATNAV_DOWNLOADED1_IDEN
            // || iden == SATNAV_DOWNLOADED2_IDEN
            || iden == WHEEL_SPEED_IDEN
            || iden == ODOMETER_IDEN
            || iden == COM2000_IDEN
            || iden == CDCHANGER_COMMAND_IDEN
            || iden == MFD_TO_HEAD_UNIT_IDEN
            || iden == AIR_CONDITIONER_DIAG_IDEN
            || iden == AIR_CONDITIONER_DIAG_COMMAND_IDEN
            || iden == SATNAV_GPS_INFO
           )
        {
            return false;
        } // if

        return true;
    } // if

    // Should never get here
    return true;
} // IsPacketSelected
