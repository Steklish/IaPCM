#include "./lab_01.hpp"
#include <windows.h>
#include <powrprof.h>
#include <iostream>
#include <string>
// bool batteryMonitor::isCharging(){

// }
// bool EnterSleep(bool hibernate) {
//     return SetSuspendState(hibernate, FALSE, FALSE) != 0;
// }

std::string batteryFlagToString(BYTE flag) {
    std::string result;

    if (flag == 255) return "Unknown status";
    if (flag & 128) result += "No system battery; ";
    if (flag & 8) result += "Charging; ";
    if (flag & 4) result += "Critical (less than 5%); ";
    if (flag & 2) result += "Low (less than 33%); ";
    if (flag & 1) result += "High (more than 66%); ";
    if (flag == 0) result = "Battery status normal";

    if (result.empty())
        result = "Unknown flag value: " + std::to_string(flag);
    else
        result = result.substr(0, result.size() - 2); // Remove last "; "

    return result;
}

std::string batteryMonitor::getStatus(){
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)){
        std::string batteryFlag = batteryFlagToString(status.BatteryFlag);
        return batteryFlag;
    }
    else {
        return "Error";
    }
}

std::string batteryMonitor::getPowerMode(){
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        std::string acLineStatus;
        if ((int)status.ACLineStatus == 1){
            acLineStatus = "Online";
        }
        else if ((int)status.ACLineStatus == 0)
        {
            acLineStatus = "Ofline";
        }
        else {
            acLineStatus = "Unknown" + std::to_string((int)status.ACLineStatus);
        }
        
        return acLineStatus;
    } else {
        return "Failed to get power status.";
    }
}

int batteryMonitor::getCharge(){
    // returns charge in percents
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)){
        return (int)status.BatteryLifePercent;
    }
    else {
        return 255;
    }
}

int batteryMonitor::sleep(){
    return SetSuspendState(false, false, false) != 0;
}

int batteryMonitor::hibernate(){
    return SetSuspendState(true, false, false) != 0;
}