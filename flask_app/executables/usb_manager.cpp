#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <winioctl.h>
#include <fileapi.h>
#include <shellapi.h>
#include <dbt.h>
#include <cfgmgr32.h>
#include <stdio.h>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "advapi32.lib")

// Set console code page to UTF-8 on Windows
void setConsoleUTF8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

// Structure to represent a USB device
struct UsbDevice {
    std::string deviceID;
    std::string displayName;
    std::string driveLetter;
    bool isDrive;
    bool isRemovable;
    std::string deviceType;
    
    UsbDevice(const std::string& id, const std::string& name, bool drive = false, 
              const std::string& letter = "", bool removable = true, const std::string& type = "USB")
        : deviceID(id), displayName(name), driveLetter(letter), isDrive(drive), 
          isRemovable(removable), deviceType(type) {}
};

class UsbManager {
public:
    // List all USB devices
    static std::vector<UsbDevice> listUsbDevices() {
        std::vector<UsbDevice> devices;
        
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
        if (deviceInfoSet == INVALID_HANDLE_VALUE) return devices;

        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
            char deviceInstanceId[1000];
            if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, deviceInstanceId,
                                          sizeof(deviceInstanceId), NULL) == FALSE) {
                continue;
            }

            std::string deviceInstanceIdStr = std::string(deviceInstanceId);

            // Filter logic
            bool isUsbDevice = deviceInstanceIdStr.find("USB") != std::string::npos;
            bool isStorageDevice = deviceInstanceIdStr.find("USBSTOR") != std::string::npos;
            bool isVirtualDevice = deviceInstanceIdStr.find("SWD\\") != std::string::npos ||
                                  deviceInstanceIdStr.find("STORAGE\\") != std::string::npos;
            bool isHub = deviceInstanceIdStr.find("ROOT_HUB") != std::string::npos ||
                        deviceInstanceIdStr.find("HUB") != std::string::npos;

            if (!isUsbDevice && !isStorageDevice) continue;
            if (isVirtualDevice || isHub) continue;

            // Get Name
            char deviceName[256] = {0};
            bool nameFound = false;

            if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData,
                                                SPDRP_FRIENDLYNAME, NULL,
                                                (PBYTE)deviceName, sizeof(deviceName), NULL)) {
                nameFound = true;
            } else if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData,
                                                      SPDRP_DEVICEDESC, NULL,
                                                      (PBYTE)deviceName, sizeof(deviceName), NULL)) {
                nameFound = true;
            }

            if (!nameFound) {
                strcpy(deviceName, "Unknown USB Device");
            }

            // Check for Drive Letter association
            bool isDrive = false;
            std::string driveLetter = "";
            bool isRemovable = false;
            std::string deviceType = "USB Device";
            bool isUsbStorageDevice = deviceInstanceIdStr.find("USBSTOR") != std::string::npos;

            if (isUsbStorageDevice) {
                // Brute force check drive letters to find matching removable drives
                for (char letter = 'A'; letter <= 'Z'; letter++) {
                    std::string drivePath = std::string(1, letter) + ":\\";
                    UINT driveTypeVal = GetDriveType(drivePath.c_str());

                    if (driveTypeVal == DRIVE_REMOVABLE) {
                        HANDLE hDevice = CreateFile(
                            (std::string("\\\\.\\") + std::string(1, letter) + ":").c_str(),
                            0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL
                        );

                        if (hDevice != INVALID_HANDLE_VALUE) {
                            STORAGE_DEVICE_NUMBER sdn;
                            DWORD bytesReturned;
                            // Check if this drive exists. 
                            // In a real production app, we would match the DeviceNumber to the Parent Instance,
                            // but for this scope, if we find a Removable drive while iterating a USBSTOR, we assume correlation.
                            if (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                              NULL, 0, &sdn, sizeof(sdn),
                                              &bytesReturned, NULL)) {
                                isDrive = true;
                                driveLetter = std::string(1, letter);
                                deviceType = "USB Drive";
                                isRemovable = true;
                            }
                            CloseHandle(hDevice);
                            if (isDrive) break;
                        }
                    }
                }
            }
            devices.emplace_back(deviceInstanceId, deviceName, isDrive, driveLetter, isRemovable, deviceType);
        }

        SetupDiDestroyDeviceInfoList(deviceInfoSet);
        return devices;
    }

    // Safely eject a USB drive
    static bool safelyEjectDrive(const std::string& driveLetter) {
        if (driveLetter.empty() || driveLetter.length() != 1) return false;
        std::string drivePath = "\\\\.\\" + driveLetter + ":";
        
        HANDLE hDevice = CreateFile(drivePath.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

        if (hDevice == INVALID_HANDLE_VALUE) {
            std::cerr << "Could not open drive. It may be in use." << std::endl;
            return false;
        }

        DWORD bytesReturned;
        if (!DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL)) {
            std::cerr << "Could not lock volume (Safe Eject). Files are open." << std::endl;
            CloseHandle(hDevice);
            return false;
        }

        DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
        
        PREVENT_MEDIA_REMOVAL pmr = { FALSE };
        DeviceIoControl(hDevice, IOCTL_STORAGE_MEDIA_REMOVAL, &pmr, sizeof(pmr), NULL, 0, &bytesReturned, NULL);
        
        BOOL result = DeviceIoControl(hDevice, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &bytesReturned, NULL);
        CloseHandle(hDevice);
        
        if(result) std::cout << "Drive " << driveLetter << ": safely ejected." << std::endl;
        else std::cerr << "Could not eject media." << std::endl;
        return result != 0;
    }

    // Force eject a USB drive
    static bool forceEjectDrive(const std::string& driveLetter) {
        if (driveLetter.empty() || driveLetter.length() != 1) {
            std::cerr << "Invalid drive letter" << std::endl;
            return false;
        }

        std::string drivePath = "\\\\.\\" + driveLetter + ":";
        
        HANDLE hDevice = CreateFile(
            drivePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if (hDevice == INVALID_HANDLE_VALUE) {
            std::cerr << "Error: Could not open drive " << driveLetter << "." << std::endl;
            std::cerr << "Run as Administrator." << std::endl;
            return false;
        }

        DWORD bytesReturned;
        BOOL result;

        std::cout << "Forcing dismount on drive " << driveLetter << "..." << std::endl;

        // Force Dismount
        result = DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
        if (!result) {
            std::cerr << "Failed to force dismount. Error: " << GetLastError() << std::endl;
            CloseHandle(hDevice);
            return false;
        }

        // Lock
        DeviceIoControl(hDevice, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

        // Allow Removal
        PREVENT_MEDIA_REMOVAL pmr = { FALSE };
        DeviceIoControl(hDevice, IOCTL_STORAGE_MEDIA_REMOVAL, &pmr, sizeof(pmr), NULL, 0, &bytesReturned, NULL);

        // Eject
        result = DeviceIoControl(hDevice, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &bytesReturned, NULL);

        DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
        CloseHandle(hDevice);

        if (result) {
            std::cout << "Drive " << driveLetter << " was forcibly ejected." << std::endl;
            return true;
        } else {
            std::cerr << "Failed to eject media." << std::endl;
            return false;
        }
    }

    static bool disableUsbDevice(const std::string& deviceID) {
        std::string command = "pnputil /disable-device \"" + deviceID + "\"";
        return system(command.c_str()) == 0;
    }

    static bool enableUsbDevice(const std::string& deviceID) {
        std::string command = "pnputil /enable-device \"" + deviceID + "\"";
        return system(command.c_str()) == 0;
    }
};

int main(int argc, char* argv[]) {
    #ifdef _WIN32
    setConsoleUTF8();
    #endif

    if (argc < 2) {
        std::cout << "USB Device Manager" << std::endl;
        std::cout << "Usage: " << argv[0] << " --list" << std::endl;
        std::cout << "       " << argv[0] << " --eject <drive>" << std::endl;
        std::cout << "       " << argv[0] << " --force-eject <drive>" << std::endl;
        std::cout << "       " << argv[0] << " --disable <id>" << std::endl;
        std::cout << "       " << argv[0] << " --enable <id>" << std::endl;
        return 1;
    }

    std::string command = argv[1];

    if (command == "--list") {
        std::vector<UsbDevice> devices = UsbManager::listUsbDevices();
        
        if (devices.empty()) {
            std::cout << "No USB devices found." << std::endl;
            return 0;
        }
        
        std::cout << "Found " << devices.size() << " USB device(s):" << std::endl;
        std::cout << "--------------------------------------------------" << std::endl;
        
        for (size_t i = 0; i < devices.size(); ++i) {
            const auto& device = devices[i];
            std::cout << "[" << i << "] " << device.displayName << std::endl;
            std::cout << "    Device ID: " << device.deviceID << std::endl;
            std::cout << "    Type: " << device.deviceType;
            if (device.isDrive) {
                std::cout << " (Drive)";
                if (!device.driveLetter.empty()) {
                    std::cout << " - Drive Letter: " << device.driveLetter << ":";
                }
            }
            std::cout << std::endl;
            std::cout << "    Removable: " << (device.isRemovable ? "Yes" : "No") << std::endl;
            std::cout << "--------------------------------------------------" << std::endl;
        }
    }
    else if (command == "--eject" && argc == 3) {
        UsbManager::safelyEjectDrive(argv[2]);
    }
    else if (command == "--force-eject" && argc == 3) {
        UsbManager::forceEjectDrive(argv[2]);
    }
    else if (command == "--disable" && argc == 3) {
        UsbManager::disableUsbDevice(argv[2]);
    }
    else if (command == "--enable" && argc == 3) {
        UsbManager::enableUsbDevice(argv[2]);
    }
    else {
        std::cerr << "Invalid arguments." << std::endl;
        return 1;
    }

    return 0;
}