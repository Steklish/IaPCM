#include "lab_05.hpp"
#include <iostream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cctype>
#include <winioctl.h>
#include <cfgmgr32.h>

// Define static member
std::ofstream USBMonitor::gLog;

USBMonitor::USBMonitor() : hVolumeNotify(nullptr), hUsbNotify(nullptr), hwnd(nullptr), isMonitoring(false) {
    // Initialize logging
    gLog.open("usb_log.txt", std::ios::app);
}

USBMonitor::~USBMonitor() {
    stopMonitoring();
    // Close the log file
    if (gLog.is_open()) {
        gLog.close();
    }
    // Clean up volume handles
    for (auto& pair : volHandle) {
        if (pair.second && pair.second != INVALID_HANDLE_VALUE) {
            CloseHandle(pair.second);
        }
    }
    volHandle.clear();
    handleToLetter.clear();
    volNotify.clear();
}

bool USBMonitor::initialize() {
    // Initialize logging
    gLog.open("usb_log.txt", std::ios::app);
    
    // Create a hidden window for receiving device notifications
    WNDCLASSA wc = {};
    wc.lpfnWndProc = USBMonitor::enhancedWindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "USBMonitorWindowClass";
    
    if (!RegisterClassA(&wc)) {
        std::cerr << "Failed to register window class for USB monitoring" << std::endl;
        return false;
    }
    
    // Create the hidden window
    hwnd = CreateWindowExA(
        0, "USBMonitorWindowClass", "USB Monitor", 
        0, 0, 0, 0, 0, nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!hwnd) {
        std::cerr << "Failed to create window for USB monitoring" << std::endl;
        return false;
    }
    
    // Set the window user data to this object so we can access it from the static WindowProc
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    // Define the USB device interface GUID manually
    // {A5DCBF10-6530-11D2-901F-00C04FB951ED}
    static const GUID GUID_DEVINTERFACE_USB_DEVICE = 
    {0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
    
    // Register for device notifications - register for both volume and USB interface changes
    // First, register for volume changes (for USB drives)
    DEV_BROADCAST_DEVICEINTERFACE_A volumeNotificationFilter = {0};
    volumeNotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_A);
    volumeNotificationFilter.dbcc_devicetype = DBT_DEVTYP_VOLUME;
    
    HDEVNOTIFY hVolumeNotify = RegisterDeviceNotificationA(
        hwnd,
        &volumeNotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    
    // Then, register for USB device interface changes
    DEV_BROADCAST_DEVICEINTERFACE_A usbNotificationFilter = {0};
    usbNotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_A);
    usbNotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    usbNotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;
    
    HDEVNOTIFY hUsbNotify = RegisterDeviceNotificationA(
        hwnd,
        &usbNotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    
    // Store both notification handles for cleanup
    this->hVolumeNotify = hVolumeNotify;  // This is the volume notification handle
    this->hUsbNotify = hUsbNotify;        // This is the USB interface notification handle
    
    // Also register for handle-based notifications for robust safe removal detection
    armExistingVolumes(); // Arm all existing removable drives for handle notifications
    
    // If we got either notification, consider initialization successful
    if (!hVolumeNotify && !hUsbNotify) {
        std::cerr << "Failed to register for device notifications" << std::endl;
        DestroyWindow(hwnd);
        return false;
    }
    
    if (!hVolumeNotify && !hUsbNotify) {
        std::cerr << "Failed to register for device notifications" << std::endl;
        DestroyWindow(hwnd);
        return false;
    }
    
    updateDeviceList(); // Initialize the device list
    return true;
}

void USBMonitor::startMonitoring() {
    if (isMonitoring) return;
    
    isMonitoring = true;
    
    // We'll use a separate thread to periodically update the device list
    // since WM_DEVICECHANGE messages might not catch all changes
    monitoringThread = std::thread([this]() {
        while (isMonitoring) {
            updateDeviceList();
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Update every 2 seconds
        }
    });
}

void USBMonitor::stopMonitoring() {
    if (!isMonitoring) return;
    
    isMonitoring = false;
    
    if (monitoringThread.joinable()) {
        monitoringThread.join();
    }
    
    if (hVolumeNotify) {
        UnregisterDeviceNotification(hVolumeNotify);
        hVolumeNotify = nullptr;
    }
    
    if (hUsbNotify) {
        UnregisterDeviceNotification(hUsbNotify);
        hUsbNotify = nullptr;
    }
    
    if (hwnd) {
        DestroyWindow(hwnd);
        hwnd = nullptr;
    }
}

std::vector<USBDeviceInfo> USBMonitor::getConnectedDevices() {
    std::vector<USBDeviceInfo> result;
    // Create a copy of the connected devices list
    for (const auto& device : connectedDevices) {
        result.push_back(device);
    }
    return result;
}

void USBMonitor::updateDeviceList() {
    std::vector<USBDeviceInfo> currentDevices;
    
    // 1. Iterate through all drives to find volume-based USB devices
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) { // A-Z drives
        if (drives & (1 << i)) {
            std::string driveLetter = std::string(1, 'A' + i) + ":\\";
            
            UINT driveType = GetDriveTypeA(driveLetter.c_str());
            
            // Check if it's a removable drive (USB flash, etc.) or CD-ROM
            if (driveType == DRIVE_REMOVABLE || driveType == DRIVE_CDROM) {
                USBDeviceInfo device;
                device.driveLetter = driveLetter;
                device.deviceType = getDriveTypeString(driveType);
                device.isMounted = true;
                device.isRemovable = true;
                
                // Try to get more information about the device
                char volumeName[MAX_PATH] = {0};
                char fileSystemName[MAX_PATH] = {0};
                DWORD volumeSerial = 0;
                DWORD maxComponentLength = 0;
                DWORD fileSystemFlags = 0;
                
                if (GetVolumeInformationA(
                    device.driveLetter.c_str(),
                    volumeName, sizeof(volumeName),
                    &volumeSerial,
                    &maxComponentLength,
                    &fileSystemFlags,
                    fileSystemName, sizeof(fileSystemName)
                )) {
                    device.deviceName = std::string(volumeName);
                    if (device.deviceName.empty()) {
                        device.deviceName = device.driveLetter + " Drive";
                    }
                } else {
                    device.deviceName = device.driveLetter + " Drive";
                }
                
                // Generate a unique device ID based on drive letter and volume serial
                device.deviceID = device.driveLetter + "_" + std::to_string(volumeSerial);
                
                currentDevices.push_back(device);
            }
        }
    }
    
    // 2. Enumerate USB devices using SetupAPI to find all USB devices (not just volumes)
    // First, enumerate USB devices using GUID_DEVCLASS_USB
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsA(&GUID_DEVCLASS_USB, NULL, NULL, DIGCF_PRESENT);
    if (deviceInfoSet != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfoData = {0};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        for (DWORD i = 0; SetupDiEnumDeviceInfo(deviceInfoSet, i, &devInfoData); i++) {
            // Get device description
            char deviceDesc[256] = {0};
            if (SetupDiGetDeviceRegistryPropertyA(
                deviceInfoSet, 
                &devInfoData, 
                SPDRP_DEVICEDESC, 
                NULL, 
                (PBYTE)deviceDesc, 
                sizeof(deviceDesc), 
                NULL
            )) {
                // Get hardware ID to determine device type
                char hardwareId[256] = {0};
                SetupDiGetDeviceRegistryPropertyA(
                    deviceInfoSet, 
                    &devInfoData, 
                    SPDRP_HARDWAREID, 
                    NULL, 
                    (PBYTE)hardwareId, 
                    sizeof(hardwareId), 
                    NULL
                );
                
                // Skip if already exists as a volume device (to avoid duplicates)
                bool alreadyExistsAsVolume = false;
                for (const auto& existingDevice : currentDevices) {
                    if (existingDevice.deviceName.find(deviceDesc) != std::string::npos) {
                        alreadyExistsAsVolume = true;
                        break;
                    }
                }
                
                if (!alreadyExistsAsVolume) {
                    // Check if it's a USB device by looking at hardware ID
                    std::string hardwareIdStr = std::string(hardwareId);
                    if (hardwareIdStr.find("USB") != std::string::npos) {
                        USBDeviceInfo device;
                        device.deviceID = std::string("USB_") + std::to_string(i);
                        device.deviceName = std::string(deviceDesc);
                        device.deviceType = "USB Device";
                        
                        // Determine more specific device type
                        if (hardwareIdStr.find("HID") != std::string::npos) {
                            device.deviceType = "HID Device";
                        } else if (hardwareIdStr.find("USBSTOR") != std::string::npos) {
                            device.deviceType = "USB Storage";
                        } else if (hardwareIdStr.find("VID_") != std::string::npos && hardwareIdStr.find("PID_") != std::string::npos) {
                            // Check if this is a known HID device type
                            std::string deviceDescLower = device.deviceName;
                            std::transform(deviceDescLower.begin(), deviceDescLower.end(), deviceDescLower.begin(), ::tolower);
                            
                            if (deviceDescLower.find("mouse") != std::string::npos) {
                                device.deviceType = "USB Mouse";
                            } else if (deviceDescLower.find("keyboard") != std::string::npos) {
                                device.deviceType = "USB Keyboard";
                            } else if (deviceDescLower.find("hub") != std::string::npos) {
                                device.deviceType = "USB Hub";
                            } else {
                                device.deviceType = "USB Device";
                            }
                        }
                        
                        device.isMounted = true;
                        device.isRemovable = true;
                        
                        currentDevices.push_back(device);
                    }
                }
            }
        }
        SetupDiDestroyDeviceInfoList(deviceInfoSet);
    }
    
    // 3. Also enumerate HID devices specifically (which includes Bluetooth mice)
    // GUID_DEVCLASS_HIDCLASS might not be defined in all Windows SDK versions
    // Using the actual GUID value
    static const GUID GUID_DEVCLASS_HIDCLASS = {0x745a17a0, 0x74d3, 0x11d0, {0xb6, 0xfe, 0x00, 0xa0, 0xc9, 0x0f, 0x57, 0xda}};
    
    HDEVINFO hidDeviceInfoSet = SetupDiGetClassDevsA(&GUID_DEVCLASS_HIDCLASS, NULL, NULL, DIGCF_PRESENT);
    if (hidDeviceInfoSet != INVALID_HANDLE_VALUE) {
        SP_DEVINFO_DATA devInfoData = {0};
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
        
        for (DWORD i = 0; SetupDiEnumDeviceInfo(hidDeviceInfoSet, i, &devInfoData); i++) {
            // Get device description
            char deviceDesc[256] = {0};
            if (SetupDiGetDeviceRegistryPropertyA(
                hidDeviceInfoSet, 
                &devInfoData, 
                SPDRP_DEVICEDESC, 
                NULL, 
                (PBYTE)deviceDesc, 
                sizeof(deviceDesc), 
                NULL
            )) {
                // Get hardware ID to determine device type
                char hardwareId[256] = {0};
                SetupDiGetDeviceRegistryPropertyA(
                    hidDeviceInfoSet, 
                    &devInfoData, 
                    SPDRP_HARDWAREID, 
                    NULL, 
                    (PBYTE)hardwareId, 
                    sizeof(hardwareId), 
                    NULL
                );
                
                // Check if already exists
                bool alreadyExists = false;
                for (const auto& existingDevice : currentDevices) {
                    if (existingDevice.deviceName.find(deviceDesc) != std::string::npos) {
                        alreadyExists = true;
                        break;
                    }
                }
                
                if (!alreadyExists) {
                    std::string hardwareIdStr = std::string(hardwareId);
                    std::string deviceDescLower = std::string(deviceDesc);
                    std::transform(deviceDescLower.begin(), deviceDescLower.end(), deviceDescLower.begin(), ::tolower);
                    
                    // Look for mouse-related devices
                    if (deviceDescLower.find("mouse") != std::string::npos || 
                        deviceDescLower.find("bluetooth") != std::string::npos ||
                        hardwareIdStr.find("HID") != std::string::npos) {
                        
                        USBDeviceInfo device;
                        device.deviceID = std::string("HID_") + std::to_string(i);
                        device.deviceName = std::string(deviceDesc);
                        
                        if (deviceDescLower.find("mouse") != std::string::npos) {
                            device.deviceType = "Bluetooth Mouse";
                        } else if (deviceDescLower.find("keyboard") != std::string::npos) {
                            device.deviceType = "Bluetooth Keyboard";
                        } else {
                            device.deviceType = "HID Device";
                        }
                        
                        device.isMounted = true;
                        device.isRemovable = true;
                        
                        currentDevices.push_back(device);
                    }
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hidDeviceInfoSet);
    }
    
    // Find newly connected devices
    for (const auto& currentDevice : currentDevices) {
        bool found = false;
        for (const auto& existingDevice : connectedDevices) {
            if (existingDevice.deviceID == currentDevice.deviceID) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // New device connected
            connectedDevices.push_back(currentDevice);
            if (deviceConnectedCallback) {
                deviceConnectedCallback(currentDevice);
            }
            std::cout << "USB Device connected: " << currentDevice.deviceName 
                      << " (" << currentDevice.deviceType << ")" << std::endl;
        }
    }
    
    // Find disconnected devices
    auto it = connectedDevices.begin();
    while (it != connectedDevices.end()) {
        bool found = false;
        for (const auto& currentDevice : currentDevices) {
            if (currentDevice.deviceID == it->deviceID) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Device disconnected
            USBDeviceInfo disconnectedDevice = *it;
            if (deviceDisconnectedCallback) {
                deviceDisconnectedCallback(disconnectedDevice);
            }
            std::cout << "USB Device disconnected: " << disconnectedDevice.deviceName 
                      << " (" << disconnectedDevice.deviceType << ")" << std::endl;
            it = connectedDevices.erase(it);
        } else {
            ++it;
        }
    }
}

bool USBMonitor::safelyEjectDevice(const std::string& deviceIdOrDrive) {
    std::cout << "Attempting to safely eject device with ID/drive: " << deviceIdOrDrive << std::endl;
    std::cout << "Number of connected devices: " << connectedDevices.size() << std::endl;
    
    // Print all available device IDs for debugging
    for (const auto& device : connectedDevices) {
        std::cout << "Available device - ID: '" << device.deviceID << "', Drive: '" << device.driveLetter << "'" << std::endl;
    }
    
    // Check if deviceIdOrDrive is a drive letter (like "E:" or "E")
    char driveLetter = 0;
    if (deviceIdOrDrive.length() >= 1) {
        char c = std::toupper(deviceIdOrDrive[0]);
        if (c >= 'A' && c <= 'Z') {
            driveLetter = c;
        }
    }
    
    // If it's a drive letter, use the enhanced safe eject method
    if (driveLetter != 0) {
        logMessage("Attempting to safely eject drive " + std::string(1, driveLetter) + ":");
        return safelyEjectDriveByLetter(driveLetter);
    }
    
    // Find the device by ID or drive letter
    for (auto& device : connectedDevices) {
        // Check exact match first
        if (device.deviceID == deviceIdOrDrive || device.driveLetter == deviceIdOrDrive) {
            std::cout << "Found exact matching device: " << device.deviceName << " (" << device.deviceID << ")" << std::endl;
            return performEject(device);
        }
        
        // Check partial match - if device ID starts with the drive letter (for cases like "E:_1792987050")
        if (device.deviceID.length() > 2 && deviceIdOrDrive.length() > 2) {
            // Extract drive letter from device ID (first 2 characters: letter + colon)
            std::string deviceDrivePrefix = device.deviceID.substr(0, 2);
            std::string requestDrivePrefix = deviceIdOrDrive.substr(0, 2);
            
            if (deviceDrivePrefix == requestDrivePrefix && deviceDrivePrefix[1] == ':') {
                std::cout << "Found partial matching device by drive letter: " << device.deviceName << " (" << device.deviceID << ")" << std::endl;
                return performEject(device);
            }
        }
    }
    std::cerr << "Device not found for safe ejection: " << deviceIdOrDrive << std::endl;
    return false;
}

// Helper function to perform the actual ejection
bool USBMonitor::performEject(const USBDeviceInfo& device) {
    // Only eject devices that have a drive letter (volumes)
    if (!device.driveLetter.empty()) {
        // Ensure the drive letter is properly formatted (X:\ to X:)
        std::string drivePath = device.driveLetter;
        if (drivePath.length() >= 3 && drivePath[1] == ':' && drivePath[2] == '\\') {
            // Remove the trailing backslash for volume operations
            drivePath = drivePath.substr(0, 2); // Just keep "X:"
        }
        
        // Try to lock the volume first
        std::string volumePath = "\\\\.\\" + drivePath;
        HANDLE hVolume = CreateFileA(
            volumePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        
        if (hVolume != INVALID_HANDLE_VALUE) {
            DWORD bytesReturned;
            
            // Step 1: Lock the volume to prevent new file operations
            BOOL result = DeviceIoControl(
                hVolume,
                FSCTL_LOCK_VOLUME,
                NULL,
                0,
                NULL,
                0,
                &bytesReturned,
                NULL
            );
            
            if (result) {
                // Step 2: Dismount the volume to prevent further I/O
                result = DeviceIoControl(
                    hVolume,
                    FSCTL_DISMOUNT_VOLUME,
                    NULL,
                    0,
                    NULL,
                    0,
                    &bytesReturned,
                    NULL
                );
                
                if (result) {
                    // Step 3: Prevent media removal (sometimes required for eject)
                    BOOL preventRemoval = FALSE;
                    result = DeviceIoControl(
                        hVolume,
                        IOCTL_STORAGE_MEDIA_REMOVAL,
                        &preventRemoval,
                        sizeof(BOOL),
                        NULL,
                        0,
                        &bytesReturned,
                        NULL
                    );
                    
                    if (result) {
                        // Step 4: Now eject the media
                        result = DeviceIoControl(
                            hVolume,
                            IOCTL_STORAGE_EJECT_MEDIA,
                            NULL,
                            0,
                            NULL,
                            0,
                            &bytesReturned,
                            NULL
                        );
                    }
                }
                
                // Step 5: Always unlock the volume
                DeviceIoControl(
                    hVolume,
                    FSCTL_UNLOCK_VOLUME,
                    NULL,
                    0,
                    NULL,
                    0,
                    &bytesReturned,
                    NULL
                );
            } else {
                std::cerr << "Failed to lock volume for device: " << device.deviceName 
                          << ", Error: " << GetLastError() << std::endl;
                // Continue anyway since locking might fail if files are in use
                // Try to eject without locking
                result = DeviceIoControl(
                    hVolume,
                    IOCTL_STORAGE_EJECT_MEDIA,
                    NULL,
                    0,
                    NULL,
                    0,
                    &bytesReturned,
                    NULL
                );
            }
            
            CloseHandle(hVolume);
            
            if (result) {
                std::cout << "Successfully safely ejected device: " << device.deviceName << std::endl;
                return true;
            } else {
                std::cerr << "Failed to safely eject device: " << device.deviceName 
                          << ", Error: " << GetLastError() << std::endl;
                return false;
            }
        } else {
            std::cerr << "Failed to open device for ejection: " << device.deviceName 
                      << ", Error: " << GetLastError() << std::endl;
            return false;
        }
    } else {
        std::cerr << "Device has no drive letter and cannot be safely ejected: " << device.deviceName << std::endl;
        std::cout << "Device type: " << device.deviceType << " - Only storage devices can be safely ejected." << std::endl;
        return false;
    }
}

bool USBMonitor::unsafeEjectDevice(const std::string& deviceIdOrDrive) {
    std::cout << "Attempting to unsafely eject device with ID/drive: " << deviceIdOrDrive << std::endl;
    
    // Check if deviceIdOrDrive is a drive letter (like "E:" or "E")
    char driveLetter = 0;
    if (deviceIdOrDrive.length() >= 1) {
        char c = std::toupper(deviceIdOrDrive[0]);
        if (c >= 'A' && c <= 'Z') {
            driveLetter = c;
        }
    }
    
    // If it's a drive letter, use the enhanced safe eject method (as unsafe eject)
    if (driveLetter != 0) {
        logMessage("Attempting to unsafely eject drive " + std::string(1, driveLetter) + ":");
        return safelyEjectDriveByLetter(driveLetter);
    }
    
    // Find the device by ID or drive letter
    for (auto& device : connectedDevices) {
        // Check exact match first
        if (device.deviceID == deviceIdOrDrive || device.driveLetter == deviceIdOrDrive) {
            std::cout << "Found exact matching device for unsafe ejection: " << device.deviceName << std::endl;
            return performEject(device); // Reuse the same ejection logic
        }
        
        // Check partial match - if device ID starts with the drive letter (for cases like "E:_1792987050")
        if (device.deviceID.length() > 2 && deviceIdOrDrive.length() > 2) {
            // Extract drive letter from device ID (first 2 characters: letter + colon)
            std::string deviceDrivePrefix = device.deviceID.substr(0, 2);
            std::string requestDrivePrefix = deviceIdOrDrive.substr(0, 2);
            
            if (deviceDrivePrefix == requestDrivePrefix && deviceDrivePrefix[1] == ':') {
                std::cout << "Found partial matching device by drive letter for unsafe ejection: " << device.deviceName << std::endl;
                return performEject(device);
            }
        }
    }
    std::cerr << "Device not found for unsafe ejection: " << deviceIdOrDrive << std::endl;
    return false;
}

void USBMonitor::setDeviceConnectedCallback(std::function<void(const USBDeviceInfo&)> callback) {
    deviceConnectedCallback = callback;
}

void USBMonitor::setDeviceDisconnectedCallback(std::function<void(const USBDeviceInfo&)> callback) {
    deviceDisconnectedCallback = callback;
}

bool USBMonitor::isUSBDrive(const std::string& driveLetter) {
    // Check if the drive is a USB drive
    UINT driveType = GetDriveTypeA(driveLetter.c_str());
    return (driveType == DRIVE_REMOVABLE);
}

std::string USBMonitor::getDriveTypeString(UINT driveType) {
    switch (driveType) {
        case DRIVE_REMOVABLE: return "Removable Drive";
        case DRIVE_FIXED: return "Fixed Drive";
        case DRIVE_REMOTE: return "Network Drive";
        case DRIVE_CDROM: return "CD-ROM";
        case DRIVE_RAMDISK: return "RAM Disk";
        default: return "Unknown Drive Type";
    }
}

bool USBMonitor::isUSBDriveByBusType(char driveLetter) {
    std::string path = "\\\\.\\";
    path.push_back(driveLetter);
    path.push_back(':');
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, 
                          OPEN_EXISTING, 0, nullptr);
    if(h==INVALID_HANDLE_VALUE) return false;
    
    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;
    
    char buffer[1024];
    DWORD bytes;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer, sizeof(buffer), &bytes, nullptr);
    CloseHandle(h);
    
    if(!ok) return false;
    auto* desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
    return (desc->BusType == BusTypeUsb);
}

std::vector<char> USBMonitor::listRemovableLetters() {
    std::vector<char> out;
    DWORD mask = GetLogicalDrives();
    for(int i=0;i<26;i++){
        if(!(mask & (1<<i))) continue;
        char letter = (char)('A'+i);
        char root[]={letter,':','\\',0};
        UINT t = GetDriveTypeA(root);
        // Включаем: (1) явно removable, (2) fixed USB диски
        if(t==DRIVE_REMOVABLE || t==DRIVE_CDROM){
            out.push_back(letter);
        }else if(t==DRIVE_FIXED){
            bool isUsb = isUSBDriveByBusType(letter);
            if(isUsb){
                out.push_back(letter);
                logMessage("  Найден USB диск (помечен как Fixed): " + std::string(1, letter) + ":");
            }
        }
    }
    return out;
}

void USBMonitor::disarmVolume(char driveLetter, bool keepMapping) {
    auto it = volHandle.find(driveLetter);
    if(it!=volHandle.end()){
        HANDLE h = it->second;
        auto itN = volNotify.find(driveLetter);
        if(itN!=volNotify.end() && itN->second){ UnregisterDeviceNotification(itN->second); }
        if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        if(!keepMapping) handleToLetter.erase(h);
        volHandle.erase(it);
        volNotify.erase(driveLetter);
    }
}

bool USBMonitor::armVolume(char driveLetter) {
    if(volHandle.count(driveLetter)) return true;
    std::string path="\\\\.\\"; path.push_back(driveLetter); path.push_back(':');
    HANDLE h = CreateFileA(path.c_str(),
        GENERIC_READ, // достаточно
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(h==INVALID_HANDLE_VALUE){
        logMessage("arm open fail " + std::string(1, driveLetter) + " err=" + std::to_string(GetLastError()));
        return false;
    }
    DEV_BROADCAST_HANDLE dbh{};
    dbh.dbch_size=sizeof(dbh);
    dbh.dbch_devicetype=DBT_DEVTYP_HANDLE;
    dbh.dbch_handle=h;
    HDEVNOTIFY hn = RegisterDeviceNotificationA(hwnd, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(!hn){
        logMessage("arm RDN fail " + std::string(1, driveLetter) + " err=" + std::to_string(GetLastError()));
        CloseHandle(h);
        return false;
    }
    volHandle[driveLetter]=h;
    volNotify[driveLetter]=hn;
    handleToLetter[h]=driveLetter;
    logMessage("✓ Диск " + std::string(1, driveLetter) + ": готов к мониторингу");
    return true;
}

void USBMonitor::armExistingVolumes() {
    for(char L : listRemovableLetters()) armVolume(L);
}

std::string USBMonitor::maskToLetters(DWORD mask) {
    std::string r;
    for(int i=0;i<26;i++) if(mask & (1u<<i)){ if(!r.empty()) r.push_back(','); r.push_back('A'+i); }
    return r;
}

bool USBMonitor::safelyEjectDriveByLetter(char driveLetter) {
    std::string path = "\\\\.\\";
    path.push_back(driveLetter);
    path.push_back(':');
    
    logMessage("Attempting to safely eject drive " + std::string(1, driveLetter) + ":");
    
    // Сначала disarm если было armed
    disarmVolume(driveLetter);
    
    // Открываем том с правами на чтение/запись
    HANDLE hVolume = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hVolume == INVALID_HANDLE_VALUE) {
        logMessage("Failed to open volume for eject: " + std::to_string(GetLastError()));
        return false;
    }
    
    DWORD bytesReturned;
    BOOL result;
    
    // Шаг 1: Блокируем том
    result = DeviceIoControl(
        hVolume,
        FSCTL_LOCK_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );
    
    if (!result) {
        logMessage("Failed to lock volume: " + std::to_string(GetLastError()));
        CloseHandle(hVolume);
        return false;
    }
    
    logMessage("Volume locked successfully");
    
    // Шаг 2: Размонтируем том
    result = DeviceIoControl(
        hVolume,
        FSCTL_DISMOUNT_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );
    
    if (!result) {
        logMessage("Failed to dismount volume: " + std::to_string(GetLastError()));
        // Разблокируем том
        DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        CloseHandle(hVolume);
        return false;
    }
    
    logMessage("Volume dismounted successfully");
    
    // Шаг 3: Запрещаем удаление (prevent removal)
    PREVENT_MEDIA_REMOVAL pmr;
    pmr.PreventMediaRemoval = FALSE;
    
    result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_MEDIA_REMOVAL,
        &pmr, sizeof(pmr),
        nullptr, 0,
        &bytesReturned,
        nullptr
    );
    
    if (!result) {
        logMessage("Warning: Failed to allow media removal: " + std::to_string(GetLastError()));
    }
    
    // Шаг 4: Извлекаем носитель
    result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_EJECT_MEDIA,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );
    
    CloseHandle(hVolume);
    
    if (!result) {
        DWORD err = GetLastError();
        logMessage("Failed to eject media: " + std::to_string(err));
        return false;
    }
    
    logMessage("Drive " + std::string(1, driveLetter) + ": ejected successfully!");
    return true;
}

bool USBMonitor::safelyEjectDriveViaCM(char driveLetter) {
    std::string volumePath = "\\\\.\\";
    volumePath.push_back(driveLetter);
    volumePath.push_back(':');
    
    logMessage("Attempting CM-based eject for drive " + std::string(1, driveLetter) + ":");
    
    // Disarm если было armed
    disarmVolume(driveLetter);
    
    HANDLE hVolume = CreateFileA(
        volumePath.c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hVolume == INVALID_HANDLE_VALUE) {
        logMessage("Failed to open volume: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Получаем device number
    STORAGE_DEVICE_NUMBER sdn;
    DWORD bytesReturned;
    
    BOOL result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        nullptr, 0,
        &sdn, sizeof(sdn),
        &bytesReturned,
        nullptr
    );
    
    CloseHandle(hVolume);
    
    if (!result) {
        logMessage("Failed to get device number: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Формируем путь к устройству
    char devicePath[256];
    sprintf(devicePath, "\\\\.\\PhysicalDrive%lu", sdn.DeviceNumber);
    
    HANDLE hDevice = CreateFileA(
        devicePath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    
    if (hDevice == INVALID_HANDLE_VALUE) {
        logMessage("Failed to open physical drive: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Получаем device instance
    DEVINST devInst = 0;
    DWORD dwSize = 0;
    
    result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        nullptr, 0,
        &sdn, sizeof(sdn),
        &bytesReturned,
        nullptr
    );
    
    CloseHandle(hDevice);
    
    if (!result) {
        logMessage("Failed to get device instance: " + std::to_string(GetLastError()));
        return false;
    }
    
    // Используем Setup API для поиска устройства
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        nullptr,
        "USB",
        nullptr,
        DIGCF_PRESENT | DIGCF_ALLCLASSES
    );
    
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        logMessage("Failed to get device info set: " + std::to_string(GetLastError()));
        return false;
    }
    
    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    
    bool found = false;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
        char buffer[256];
        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, buffer, sizeof(buffer), nullptr)) {
            // Пробуем запросить извлечение
            PNP_VETO_TYPE vetoType;
            char vetoName[MAX_PATH];
            
            CONFIGRET cr = CM_Request_Device_EjectA(
                devInfoData.DevInst,
                &vetoType,
                vetoName,
                sizeof(vetoName),
                0
            );
            
            if (cr == CR_SUCCESS) {
                logMessage("Device ejected successfully via CM API");
                found = true;
                break;
            } else if (cr == CR_REMOVE_VETOED) {
                logMessage("Eject vetoed: " + std::string(vetoName));
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return found;
}

void USBMonitor::logMessage(const std::string& message) {
    SYSTEMTIME st; 
    GetLocalTime(&st);
    char ts[64]; 
    sprintf(ts,"%04u-%02u-%02u %02u:%02u:%02u ",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    gLog << ts << message << "\n"; 
    gLog.flush();
    std::cout << ts << message << std::endl;
}

LRESULT CALLBACK USBMonitor::enhancedWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    USBMonitor* monitor = reinterpret_cast<USBMonitor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    if (!monitor) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    switch(uMsg) {
        case WM_CREATE: 
            return 0;
            
        case WM_TIMER: {
            if(wParam==1) {
                KillTimer(hwnd,1);
                // Сканируем диски после подключения USB
                monitor->logMessage("[DEBUG] Таймер: сканирование дисков...");
                auto currentDisks = monitor->listRemovableLetters();
                if(currentDisks.empty()) {
                    monitor->logMessage("[DEBUG] Таймер: нет съёмных дисков");
                }
                for(char L : currentDisks) {
                    if(!monitor->volHandle.count(L)) {
                        monitor->logMessage("→ USB диск " + std::string(1,L) + ": обнаружен при сканировании");
                        monitor->armVolume(L);
                    } else {
                        monitor->logMessage("[DEBUG] Диск " + std::string(1,L) + ": уже зарегистрирован");
                    }
                }
            }
            else if(wParam==2) {
                KillTimer(hwnd,2);
                // Проверяем диски после QUERYREMOVE
                monitor->logMessage("[DEBUG] Таймер SAFE: проверка дисков после QUERYREMOVE...");
                auto currentDisks = monitor->listRemovableLetters();
                std::vector<char> toRemove;
                std::vector<char> toReportFailed;
                
                for(auto& pair : monitor->safePending) {
                    char L = pair.first;
                    bool stillExists = std::find(currentDisks.begin(), currentDisks.end(), L) != currentDisks.end();
                    
                    if(!stillExists && !monitor->alreadyReported[L]) {
                        // Диск исчез - это безопасное извлечение
                        monitor->logMessage("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск " + std::string(1,L) + ": извлечён через системное меню");
                        monitor->alreadyReported[L] = true;
                        toRemove.push_back(L);
                    } else if(stillExists && !monitor->alreadyReported[L] && !monitor->queryFailed[L]) {
                        // Диск ВСЁ ЕЩЁ существует - это отказ в извлечении!
                        // QUERYREMOVEFAILED мог не прийти, но мы знаем по факту
                        monitor->logMessage("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск " + std::string(1,L) + ": - устройство занято!");
                        monitor->logMessage("  Причина: открыты файлы или идёт запись на диск");
                        monitor->queryFailed[L] = true;
                        monitor->safePending.erase(L);
                        toReportFailed.push_back(L);
                    }
                }
                
                // Обрабатываем безопасные извлечения
                for(char L : toRemove) {
                    monitor->safePending.erase(L);
                    monitor->disarmVolume(L); // Закрываем handle
                    monitor->alreadyReported.erase(L);
                }
                
                // Перерегистрируем диски с отказами для продолжения мониторинга
                for(char L : toReportFailed) {
                    monitor->armVolume(L); // Перевооружаем для продолжения мониторинга
                    monitor->alreadyReported.erase(L);
                }
            }
            return 0;
        }

        case WM_DEVICECHANGE: {
            PDEV_BROADCAST_HDR hdr = (PDEV_BROADCAST_HDR)lParam;
            
            // Логируем события для отладки
            if(hdr) {
                std::string evtName = "UNKNOWN";
                if(wParam==DBT_DEVICEARRIVAL) evtName="ARRIVAL";
                if(wParam==DBT_DEVICEREMOVECOMPLETE) evtName="REMOVECOMPLETE";
                if(wParam==DBT_DEVICEQUERYREMOVE) evtName="QUERYREMOVE";
                if(wParam==DBT_DEVICEQUERYREMOVEFAILED) evtName="QUERYREMOVEFAILED";
                
                char dbgmsg[256];
                sprintf(dbgmsg, "[DEBUG] Event=%s DevType=%d", evtName.c_str(), hdr->dbch_devicetype);
                monitor->logMessage(dbgmsg);
            }

            // 1) Главное: HANDLE-уведомления — единственный надёжный индикатор SAFE
            if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_HANDLE) {
                auto* dh=(PDEV_BROADCAST_HANDLE)hdr;
                auto it = monitor->handleToLetter.find(dh->dbch_handle);
                char L = (it==monitor->handleToLetter.end()? 0 : it->second);
                if(!L) return TRUE;

                if(wParam==DBT_DEVICEQUERYREMOVE) {
                    monitor->logMessage("→ QUERYREMOVE(HANDLE) для диска " + std::string(1,L) + ": - пользователь запросил безопасное извлечение");
                    monitor->safePending[L]=true;
                    monitor->alreadyReported[L]=false;
                    
                    // ОБЯЗАТЕЛЬНО закрываем handle, иначе БЛОКИРУЕМ извлечение!
                    // Но СОХРАНЯЕМ mapping handle->letter для QUERYREMOVEFAILED
                    auto it = monitor->volHandle.find(L);
                    if(it!=monitor->volHandle.end()) {
                        HANDLE h = it->second;
                        auto itN = monitor->volNotify.find(L);
                        if(itN!=monitor->volNotify.end() && itN->second){ 
                            UnregisterDeviceNotification(itN->second); 
                            monitor->volNotify.erase(L);
                        }
                        CloseHandle(h); // ОБЯЗАТЕЛЬНО закрыть!
                        monitor->volHandle.erase(it);
                        // НЕ удаляем handleToLetter - нужен для QUERYREMOVEFAILED!
                    }
                    
                    SetTimer(monitor->hwnd, 2, 3000, nullptr); // Через 3 секунды проверим, исчез ли диск
                    return TRUE;            // согласие - разрешаем извлечение
                }
                if(wParam==DBT_DEVICEQUERYREMOVEFAILED) {
                    monitor->logMessage("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск " + std::string(1,L) + ": - устройство занято!");
                    monitor->logMessage("  Причина: открыты файлы или идёт запись на диск");
                    monitor->queryFailed[L]=true;
                    monitor->safePending.erase(L);
                    monitor->alreadyReported.erase(L);
                    KillTimer(monitor->hwnd, 2); // Отменяем таймер проверки
                    
                    // Перерегистрируем диск для продолжения мониторинга
                    monitor->armVolume(L);
                    return TRUE;
                }
                if(wParam==DBT_DEVICEREMOVECOMPLETE) {
                    // ВАЖНО: для некоторых устройств это единственное событие удаления!
                    if(!monitor->alreadyReported[L]) {
                        bool safe = monitor->safePending.count(L)>0;
                        if(safe) {
                            monitor->logMessage("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск " + std::string(1,L) + ": извлечён через системное меню");
                        } else {
                            monitor->logMessage("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИИЕ: Диск " + std::string(1,L) + ": извлечён без запроса (выдернули флешку)");
                        }
                        monitor->alreadyReported[L]=true;
                    }
                    monitor->safePending.erase(L);
                    monitor->queryFailed.erase(L);
                    monitor->handleToLetter.erase(dh->dbch_handle); // Очищаем mapping
                    KillTimer(monitor->hwnd, 2); // Отменяем таймер
                    monitor->disarmVolume(L); // Закрываем handle при удалении (если ещё не закрыт)
                    return TRUE;
                }
                return TRUE;
            }

            // 2) VOLUME по маске (для букв и финального вердикта - резервный путь)
            if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_VOLUME) {
                auto* dv=(PDEV_BROADCAST_VOLUME)hdr;
                std::string letters = monitor->maskToLetters(dv->dbcv_unitmask);
                for(char L : letters) {
                    if(wParam==DBT_DEVICEARRIVAL) {
                        monitor->logMessage("→ USB диск " + std::string(1,L) + ": подключён");
                        monitor->armVolume(L); // как только появилась буква — мгновенно армируем
                    } else if(wParam==DBT_DEVICEREMOVECOMPLETE) {
                        // Выводим вердикт только если ещё не было HANDLE события
                        if(!monitor->alreadyReported[L]) {
                            bool safe = monitor->safePending.count(L)>0;
                            if(safe) {
                                monitor->logMessage("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск " + std::string(1,L) + ": извлечён через системное меню");
                            } else {
                                monitor->logMessage("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИИЕ: Диск " + std::string(1,L) + ": извлечён без запроса (выдернули флешку)");
                            }
                            monitor->alreadyReported[L]=true;
                        }
                        monitor->safePending.erase(L);
                        monitor->queryFailed.erase(L);
                        monitor->alreadyReported.erase(L);
                        monitor->disarmVolume(L);
                    } else if(wParam==DBT_DEVICEQUERYREMOVEFAILED) {
                        monitor->logMessage("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск " + std::string(1,L) + ": - устройство занято!");
                        monitor->logMessage("  Причина: открыты файлы или идёт запись на диск");
                        monitor->queryFailed[L]=true;
                        monitor->safePending.erase(L);
                        monitor->alreadyReported.erase(L);
                        KillTimer(monitor->hwnd, 2); // Отменяем таймер проверки
                    } else if(wParam==DBT_DEVICEREMOVEPENDING || wParam==DBT_DEVICEQUERYREMOVE) {
                        monitor->logMessage("[DEBUG] QUERYREMOVE(volume hint) " + std::string(1,L));
                    }
                }
                return 0;
            }

            // 3) Интерфейсы — чисто для логов (не решают SAFE)
            if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_DEVICEINTERFACE) {
                auto* di=(PDEV_BROADCAST_DEVICEINTERFACE_A)hdr;
                std::string path = di && di->dbcc_name? di->dbcc_name : "";
                // Упрощаем логи интерфейсов
                if(wParam==DBT_DEVICEARRIVAL && path.find("USB#") != std::string::npos) {
                    monitor->logMessage("[DEBUG] USB устройство подключено: " + path.substr(0,80));
                    
                    // Проверяем новые диски (VOLUME событие может не прийти)
                    // Используем таймер вместо Sleep, чтобы не блокировать поток
                    SetTimer(monitor->hwnd, 1, 2000, nullptr); // Через 2 секунды просканируем диски
                }
                if(wParam==DBT_DEVICEREMOVECOMPLETE && path.find("USB#") != std::string::npos) {
                    monitor->logMessage("[DEBUG] USB устройство отключено: " + path.substr(0,80));
                    
                    // ВАЖНО: Проверяем какие диски исчезли из системы
                    auto currentDisks = monitor->listRemovableLetters();
                    
                    // Проверяем ВСЕ зарегистрированные диски, не только safePending
                    std::vector<char> toRemove;
                    for(auto& pair : monitor->volHandle) {
                        char L = pair.first;
                        bool stillExists = std::find(currentDisks.begin(), currentDisks.end(), L) != currentDisks.end();
                        if(!stillExists && !monitor->alreadyReported[L]) {
                            // Диск исчез!
                            bool safe = (monitor->safePending.count(L) > 0);
                            if(safe) {
                                monitor->logMessage("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск " + std::string(1,L) + ": извлечён через системное меню");
                            } else {
                                monitor->logMessage("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИИЕ: Диск " + std::string(1,L) + ": извлечён без запроса (выдернули флешку)");
                            }
                            monitor->alreadyReported[L]=true;
                            monitor->safePending.erase(L);
                            toRemove.push_back(L);
                        }
                    }
                    // Удаляем исчезнувшие диски
                    for(char L : toRemove) {
                        monitor->disarmVolume(L);
                        monitor->alreadyReported.erase(L);
                    }
                }
                return 0;
            }
            return 0;
        }

        case WM_DESTROY: 
            PostQuitMessage(0); 
            return 0;
        default: 
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK USBMonitor::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    USBMonitor* monitor = reinterpret_cast<USBMonitor*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (uMsg) {
        case WM_DEVICECHANGE:
            if (monitor) {
                PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
                
                switch (wParam) {
                    case DBT_DEVICEARRIVAL:
                        if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                            PDEV_BROADCAST_VOLUME pVol = (PDEV_BROADCAST_VOLUME)pHdr;
                            // A volume (drive) was inserted
                            // Find the drive letter from the unit mask
                            DWORD unitMask = pVol->dbcv_unitmask;
                            int driveIndex = 0;
                            for (int i = 0; i < 26; i++) {
                                if (unitMask & (1 << i)) {
                                    driveIndex = i;
                                    break;
                                }
                            }
                            char driveLetter = 'A' + driveIndex;
                            std::string driveStr = std::string(1, driveLetter) + ":\\";
                            if (isUSBDrive(driveStr)) {
                                std::cout << "USB volume arrived: " << driveStr << std::endl;
                                monitor->updateDeviceList();
                            }
                        } else if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                            // USB device interface arrived (non-volume USB device)
                            std::cout << "USB device interface arrived" << std::endl;
                            monitor->updateDeviceList();
                        }
                        break;
                        
                    case DBT_DEVICEREMOVECOMPLETE:
                        if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                            PDEV_BROADCAST_VOLUME pVol = (PDEV_BROADCAST_VOLUME)pHdr;
                            // A volume (drive) was removed
                            // Find the drive letter from the unit mask
                            DWORD unitMask = pVol->dbcv_unitmask;
                            int driveIndex = 0;
                            for (int i = 0; i < 26; i++) {
                                if (unitMask & (1 << i)) {
                                    driveIndex = i;
                                    break;
                                }
                            }
                            char driveLetter = 'A' + driveIndex;
                            std::string driveStr = std::string(1, driveLetter) + ":\\";
                            if (isUSBDrive(driveStr)) {
                                std::cout << "USB volume removed: " << driveStr << std::endl;
                                monitor->updateDeviceList();
                            }
                        } else if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                            // USB device interface removed (non-volume USB device)
                            std::cout << "USB device interface removed" << std::endl;
                            monitor->updateDeviceList();
                        }
                        break;
                }
            }
            return TRUE;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}