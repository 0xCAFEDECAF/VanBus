
// Simple packet filtering on IDEN

bool isPacketSelected(uint16_t iden, VanPacketFilter_t filter)
{
    if (filter == VAN_PACKETS_ALL_EXCEPT)
    {
        // Show all packets, but discard the following:
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
            // || iden == TIME_IDEN
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
            // || iden == ECU_IDEN
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_NONE_EXCEPT)
    {
        // Show no packets, except the following:
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
            // && iden != TIME_IDEN
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
            // && iden != ECU_IDEN
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_HEAD_UNIT)
    {
        // Head unit packets (plus anything not recognized)
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
            || iden == TIME_IDEN
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
            || iden == ECU_IDEN
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_AIRCON)
    {
        // Aircon packets (plus anything not recognized)
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
            || iden == TIME_IDEN
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
            || iden == ECU_IDEN
           )
        {
            return false;
        } // if

        return true;
    } // if

    if (filter == VAN_PACKETS_SAT_NAV)
    {
        // SatNav packets (plus anything not recognized)
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
            || iden == TIME_IDEN
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
            || iden == ECU_IDEN
           )
        {
            return false;
        } // if

        return true;
    } // if

    // Should never get here
    return true;
} // isPacketSelected
