#include "./lab_01.hpp"
#include <windows.h>
#include <powrprof.h>
#include <iostream>
#include <string>
#include <setupapi.h>
#include <batclass.h>  // For battery IOCTLs
#include <initguid.h>
#include <devguid.h>   // For GUID_DEVCLASS_BATTERY
#include <sstream>
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

int batteryMonitor::getTimeLeft(){
    SYSTEM_POWER_STATUS sps;
    if (GetSystemPowerStatus(&sps)) {
        return (int)sps.BatteryLifeTime;
    } else {
        return -1;
    }
}

std::string batteryMonitor::isEco() {
    SYSTEM_POWER_STATUS sps;
    // GetSystemPowerStatus returns a non-zero value on success.
    if (GetSystemPowerStatus(&sps)) {
        // According to documentation, SystemStatusFlag is 1 if battery saver is on.
        if (sps.SystemStatusFlag == 1) {
            return "On";
        }
    }
    // If the function fails or the flag is not set, it's off.
    return "Off";
}


batteryMonitor::batteryMonitor(){

}


std::string batteryMonitor::getBatteryInfo() {
    std::stringstream ss;
    GUID batteryClassGuid = GUID_DEVCLASS_BATTERY;

    HDEVINFO deviceInfoSet = SetupDiGetClassDevs(&batteryClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        ss << "Failed to get device info set\n";
        return ss.str();
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData = {0};
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; ; i++) {
        if (!SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, &batteryClassGuid, i, &deviceInterfaceData)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) {
                // This is the normal exit condition for the loop.
                break;
            }
            // An unexpected error occurred.
            break;
        }

        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, NULL, 0, &requiredSize, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            ss << "Failed to get required size for device interface detail\n";
            continue;
        }

        PSP_DEVICE_INTERFACE_DETAIL_DATA deviceDetailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);
        if (deviceDetailData == NULL) {
            ss << "Failed to allocate memory for device detail data\n";
            continue;
        }
        deviceDetailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(deviceInfoSet, &deviceInterfaceData, deviceDetailData, requiredSize, NULL, NULL)) {
            ss << "Found Battery " << i << ": " << deviceDetailData->DevicePath << "\n";
            HANDLE batteryHandle = CreateFile(deviceDetailData->DevicePath,
                                              GENERIC_READ | GENERIC_WRITE,
                                              FILE_SHARE_READ | FILE_SHARE_WRITE,
                                              NULL, OPEN_EXISTING,
                                              FILE_ATTRIBUTE_NORMAL, NULL);

            if (batteryHandle != INVALID_HANDLE_VALUE) {
                DWORD returned;
                ULONG batteryTag = 0;
                BATTERY_QUERY_INFORMATION bqi = {0};

                if (!DeviceIoControl(batteryHandle, IOCTL_BATTERY_QUERY_TAG,
                                     NULL, 0,
                                     &bqi.BatteryTag, sizeof(bqi.BatteryTag),
                                     &returned, NULL) || bqi.BatteryTag == 0) {
                    ss << "  Failed to get a valid battery tag.\n";
                    CloseHandle(batteryHandle);
                    free(deviceDetailData);
                    continue;
                }
                
                bqi.InformationLevel = BatteryInformation;

                BATTERY_INFORMATION batteryInfo = {0};
                if (!DeviceIoControl(batteryHandle, IOCTL_BATTERY_QUERY_INFORMATION,
                                     &bqi, sizeof(bqi),
                                     &batteryInfo, sizeof(batteryInfo),
                                     &returned, NULL)) {
                    
                    // **** THIS IS THE CRITICAL CHANGE ****
                    DWORD errorCode = GetLastError();
                    LPSTR messageBuffer = nullptr;
                    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                                 NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);
                    
                    ss << "  Failed to query battery information. Error " << errorCode << ": " << messageBuffer << "\n";
                    
                    // Free the buffer allocated by FormatMessageA.
                    LocalFree(messageBuffer);

                } else {
                    char chemistry[5] = {0};
                    memcpy(chemistry, &batteryInfo.Chemistry, 4);

                    ss << "  Chemistry: " << chemistry << "\n";
                    ss << "  Designed Capacity: " << (batteryInfo.DesignedCapacity == -1 ? "Unknown" : std::to_string(batteryInfo.DesignedCapacity)) << "\n";
                    ss << "  Full Charged Capacity: " << (batteryInfo.FullChargedCapacity == -1 ? "Unknown" : std::to_string(batteryInfo.FullChargedCapacity)) << "\n";
                    ss << "  Cycle Count: " << (batteryInfo.CycleCount == -1 ? "Unknown" : std::to_string(batteryInfo.CycleCount)) << "\n";
                }

                CloseHandle(batteryHandle);
            } else {
                ss << "  Failed to open battery device handle.\n";
            }
        }
        free(deviceDetailData);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    if (ss.str().empty()) {
        ss << "No batteries found or could not be queried.\n";
    }
    return ss.str();
}