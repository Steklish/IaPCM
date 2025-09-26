#include "./lab_02.hpp"

#include "pci_codes.h"
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>



std::string find_vendor_name(unsigned short id) {
    int n = sizeof(PciVenTable) / sizeof(PciVenTable[0]);
    for (int i = 0; i < n; i++) {
        if (PciVenTable[i].VenId == id) {
            return PciVenTable[i].VenFull;
        }
    }
    return "Unknown Vendor [" + id + std::string("]");
}

static std::pair<std::string, std::string> ExtractVidDid(const std::wstring& hardwareId) {
    std::string vid;
    std::string did;

    std::wstring firstId = hardwareId.c_str();
    std::string hardwareIdStr(firstId.begin(), firstId.end());
    size_t venPos = hardwareIdStr.find("VEN_");
    if (venPos != std::string::npos) {
        vid = hardwareIdStr.substr(venPos + 4, 4);
    }
    vid = std::string("[") + vid + std::string("] ") + find_vendor_name((unsigned short)strtol(vid.c_str(), NULL, 16));

    size_t devPos = hardwareIdStr.find("DEV_");
    if (devPos != std::string::npos) {
        did = hardwareIdStr.substr(devPos + 4, 4);
    }

    return {vid, did};
}

std::vector<std::pair<std::string, std::string>> EnumeratePCIDevices()
{
    std::vector<std::pair<std::string, std::string>> devices;
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(
        NULL,
        L"PCI",
        NULL,
        DIGCF_PRESENT | DIGCF_ALLCLASSES);

    SP_DEVINFO_DATA deviceInfoData;
    deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    DWORD deviceIndex = 0;
    while (SetupDiEnumDeviceInfo(deviceInfoSet, deviceIndex, &deviceInfoData)) {
        wchar_t hardwareId[1024] = {0};
        if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, &deviceInfoData, SPDRP_HARDWAREID, NULL, (PBYTE)hardwareId, sizeof(hardwareId), NULL)) {
            devices.push_back(ExtractVidDid(hardwareId));
        }
        deviceIndex++;
    }
    SetupDiDestroyDeviceInfoList(deviceInfoSet);

    return devices;
}