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

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "user32.lib")

// Set console code page to UTF-8 on Windows
void setConsoleUTF8() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

// Structure to represent a USB device
struct UsbDevice {
    std::string deviceID;
    std::string displayName;
    std::string driveLetter;  // Only for drives
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
        
        // Get all devices using SetupAPI, then filter for USB
        HDEVINFO deviceInfoSet = SetupDiGetClassDevs(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);

        SP_DEVINFO_DATA deviceInfoData;
        deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &deviceInfoData); i++) {
            // Get device instance ID
            char deviceInstanceId[1000]; // Using a reasonable buffer size instead of MAX_DEVICE_ID_LEN
            if (SetupDiGetDeviceInstanceId(deviceInfoSet, &deviceInfoData, deviceInstanceId,
                                          sizeof(deviceInstanceId), NULL) == FALSE) {
                continue;
            }

            // Only include USB devices, but be more specific to avoid including connected devices that aren't actual USB devices
            std::string deviceInstanceIdStr = std::string(deviceInstanceId);

            // We'll be more selective - look for actual USB devices and ignore virtual entries
            bool isUsbDevice = deviceInstanceIdStr.find("USB") != std::string::npos;
            bool isStorageDevice = deviceInstanceIdStr.find("USBSTOR") != std::string::npos;
            bool isVirtualDevice = deviceInstanceIdStr.find("SWD\\") != std::string::npos ||
                                  deviceInstanceIdStr.find("STORAGE\\") != std::string::npos;
            bool isHub = deviceInstanceIdStr.find("ROOT_HUB") != std::string::npos ||
                        deviceInstanceIdStr.find("HUB") != std::string::npos;

            // Only process actual USB devices and USB storage devices, skip virtual entries and hubs
            if (!isUsbDevice && !isStorageDevice) {
                continue;
            }
            if (isVirtualDevice || isHub) {
                continue;
            }

            // Get device name/description
            char deviceName[256];
            bool nameFound = false;

            // Try to get name using SPDRP_FRIENDLYNAME first
            if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData,
                                                SPDRP_FRIENDLYNAME, NULL,
                                                (PBYTE)deviceName, sizeof(deviceName), NULL)) {
                nameFound = true;
            } else if (SetupDiGetDeviceRegistryProperty(deviceInfoSet, &deviceInfoData,
                                                      SPDRP_DEVICEDESC, NULL,
                                                      (PBYTE)deviceName, sizeof(deviceName), NULL)) {
                nameFound = true;
            } else {
                // Try to open device registry key
                HKEY deviceKey = SetupDiOpenDevRegKey(deviceInfoSet, &deviceInfoData, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
                if (deviceKey != INVALID_HANDLE_VALUE) {
                    DWORD size = sizeof(deviceName);
                    if (RegQueryValueEx(deviceKey, "DeviceDesc", NULL, NULL,
                                       (LPBYTE)deviceName, &size) == ERROR_SUCCESS) {
                        nameFound = true;
                    }
                    RegCloseKey(deviceKey);
                }
            }

            if (!nameFound) {
                strcpy_s(deviceName, sizeof(deviceName), "Unknown USB Device");
            }

            // Check if this is a storage device/drive
            bool isDrive = false;
            std::string driveLetter = "";
            bool isRemovable = false;
            std::string deviceType = "USB Device";

            // Only consider devices that have USBSTOR in their ID as actual storage devices
            bool isUsbStorageDevice = deviceInstanceIdStr.find("USBSTOR") != std::string::npos;

            if (isUsbStorageDevice) {
                // Find drive letters associated with this particular storage device
                for (char letter = 'A'; letter <= 'Z'; letter++) {
                    std::string drivePath = std::string(1, letter) + ":\\";
                    UINT driveType = GetDriveType(drivePath.c_str());

                    // Only check removable drives for USB storage devices
                    if (driveType == DRIVE_REMOVABLE) {
                        // Open the drive to get its device number
                        HANDLE hDevice = CreateFile(
                            (std::string("\\\\.\\") + std::string(1, letter) + ":").c_str(),
                            0,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            0,
                            NULL
                        );

                        if (hDevice != INVALID_HANDLE_VALUE) {
                            STORAGE_DEVICE_NUMBER sdn;
                            DWORD bytesReturned;
                            if (DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER,
                                              NULL, 0, &sdn, sizeof(sdn),
                                              &bytesReturned, NULL)) {
                                // Since this is a USBSTOR device and we found a removable drive,
                                // we assume it's associated with this USB device
                                isDrive = true;
                                driveLetter = std::string(1, letter);
                                deviceType = "USB Drive";
                                isRemovable = true;
                            }
                            CloseHandle(hDevice);

                            // If we found a match, break out of the loop
                            if (isDrive) {
                                break;
                            }
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
            std::cerr << "Could not open drive: " << driveLetter << std::endl;
            return false;
        }

        DWORD bytesReturned;
        BOOL result = DeviceIoControl(
            hDevice,
            FSCTL_LOCK_VOLUME,
            NULL,
            0,
            NULL,
            0,
            &bytesReturned,
            NULL
        );

        if (!result) {
            std::cerr << "Could not lock volume " << driveLetter << std::endl;
            CloseHandle(hDevice);
            return false;
        }

        result = DeviceIoControl(
            hDevice,
            FSCTL_DISMOUNT_VOLUME,
            NULL,
            0,
            NULL,
            0,
            &bytesReturned,
            NULL
        );

        if (!result) {
            std::cerr << "Could not dismount volume " << driveLetter << std::endl;
            // Still try to unlock the volume
            DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
            CloseHandle(hDevice);
            return false;
        }

        // Eject the volume
        result = DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_EJECT_MEDIA,
            NULL,
            0,
            NULL,
            0,
            &bytesReturned,
            NULL
        );

        // Unlock the volume regardless of eject result
        DeviceIoControl(hDevice, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
        CloseHandle(hDevice);

        if (result) {
            std::cout << "Drive " << driveLetter << ": safely ejected." << std::endl;
            return true;
        } else {
            std::cerr << "Could not eject drive " << driveLetter << std::endl;
            return false;
        }
    }

    // Force eject a USB drive (not safe)
    static bool forceEjectDrive(const std::string& driveLetter) {
        if (driveLetter.empty() || driveLetter.length() != 1) {
            std::cerr << "Invalid drive letter" << std::endl;
            return false;
        }

        std::string command = "powershell -Command \"& {Stop-Service -Name 'ShellHWDetection' -Force; Start-Sleep -Seconds 1; Start-Service -Name 'ShellHWDetection'}\"";
        int result = system(command.c_str());
        
        // Using devcon utility would be better, but for this implementation we'll use a simple approach
        std::cout << "Force ejecting drive " << driveLetter << ": is not fully implemented in this version." << std::endl;
        std::cout << "This requires additional Windows SDK tools." << std::endl;
        return false;
    }

    // Disable a USB device by device ID
    static bool disableUsbDevice(const std::string& deviceID) {
        // This requires more complex interaction with Windows setup API
        // Using DevCon (Device Console) utility would be ideal, but not available by default
        std::string command = "pnputil /disable-device \"" + deviceID + "\"";
        int result = system(command.c_str());
        
        if (result == 0) {
            std::cout << "Device disabled successfully: " << deviceID << std::endl;
            return true;
        } else {
            std::cerr << "Failed to disable device: " << deviceID << std::endl;
            return false;
        }
    }

    // Enable a USB device by device ID
    static bool enableUsbDevice(const std::string& deviceID) {
        std::string command = "pnputil /enable-device \"" + deviceID + "\"";
        int result = system(command.c_str());
        
        if (result == 0) {
            std::cout << "Device enabled successfully: " << deviceID << std::endl;
            return true;
        } else {
            std::cerr << "Failed to enable device: " << deviceID << std::endl;
            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    // Set console code page to UTF-8 for proper character display
    #ifdef _WIN32
    setConsoleUTF8();
    #endif

    if (argc < 2) {
        std::cout << "USB Device Manager" << std::endl;
        std::cout << "Usage: " << argv[0] << " --list" << std::endl;
        std::cout << "       " << argv[0] << " --eject <drive_letter>" << std::endl;
        std::cout << "       " << argv[0] << " --force-eject <drive_letter>" << std::endl;
        std::cout << "       " << argv[0] << " --disable <device_id>" << std::endl;
        std::cout << "       " << argv[0] << " --enable <device_id>" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " --list                  # List all USB devices" << std::endl;
        std::cout << "  " << argv[0] << " --eject E               # Safely eject drive E:" << std::endl;
        std::cout << "  " << argv[0] << " --force-eject F         # Force eject drive F:" << std::endl;
        std::cout << "  " << argv[0] << " --disable USB\\VID_...  # Disable USB device" << std::endl;
        std::cout << "  " << argv[0] << " --enable USB\\VID_...   # Enable USB device" << std::endl;
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
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " --eject <drive_letter>" << std::endl;
            return 1;
        }
        std::string driveLetter = argv[2];
        if (driveLetter.length() != 1) {
            std::cerr << "Invalid drive letter. Use a single letter (e.g., E)" << std::endl;
            return 1;
        }
        if (!UsbManager::safelyEjectDrive(driveLetter)) {
            return 1;
        }
    }
    else if (command == "--force-eject" && argc == 3) {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " --force-eject <drive_letter>" << std::endl;
            return 1;
        }
        std::string driveLetter = argv[2];
        if (driveLetter.length() != 1) {
            std::cerr << "Invalid drive letter. Use a single letter (e.g., E)" << std::endl;
            return 1;
        }
        if (!UsbManager::forceEjectDrive(driveLetter)) {
            return 1;
        }
    }
    else if (command == "--disable" && argc == 3) {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " --disable <device_id>" << std::endl;
            return 1;
        }
        std::string deviceId = argv[2];
        if (!UsbManager::disableUsbDevice(deviceId)) {
            return 1;
        }
    }
    else if (command == "--enable" && argc == 3) {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " --enable <device_id>" << std::endl;
            return 1;
        }
        std::string deviceId = argv[2];
        if (!UsbManager::enableUsbDevice(deviceId)) {
            return 1;
        }
    }
    else {
        std::cerr << "Unknown command or incorrect number of arguments." << std::endl;
        std::cerr << "Use --list, --eject, --force-eject, --disable, or --enable" << std::endl;
        return 1;
    }

    return 0;
}