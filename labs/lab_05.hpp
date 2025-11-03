#ifndef LAB_05_HPP
#define LAB_05_HPP

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <devguid.h>
#include <initguid.h>
#include <hidsdi.h>
#include <iostream>
#include <map>
#include <unordered_map>
#include <fstream>

// Structure to hold USB device information
struct USBDeviceInfo {
    std::string deviceID;
    std::string deviceName;
    std::string driveLetter;
    std::string deviceType;
    bool isMounted;
    bool isRemovable;
    
    USBDeviceInfo() : isMounted(false), isRemovable(false) {}
    USBDeviceInfo(const std::string& id, const std::string& name, const std::string& drive = "", 
                  const std::string& type = "", bool mounted = false, bool removable = false)
        : deviceID(id), deviceName(name), driveLetter(drive), deviceType(type), 
          isMounted(mounted), isRemovable(removable) {}
};

class USBMonitor {
private:
    HDEVNOTIFY hVolumeNotify;  // For volume (drive) notifications
    HDEVNOTIFY hUsbNotify;     // For USB device interface notifications
    HWND hwnd;
    std::thread monitoringThread;
    bool isMonitoring;
    std::vector<USBDeviceInfo> connectedDevices;
    std::function<void(const USBDeviceInfo&)> deviceConnectedCallback;
    std::function<void(const USBDeviceInfo&)> deviceDisconnectedCallback;
    
    // Enhanced state management for robust USB monitoring
    std::unordered_map<char, HANDLE> volHandle;          // X: -> handle
    std::unordered_map<char, HDEVNOTIFY> volNotify;      // X: -> hnotify
    std::unordered_map<HANDLE, char> handleToLetter;     // handle -> X
    std::unordered_map<char, bool> safePending;          // видел QUERYREMOVE(HANDLE)
    std::unordered_map<char, bool> queryFailed;          // был QUERYREMOVEFAILED
    std::unordered_map<char, bool> alreadyReported;      // уже вывели финальный вердикт
    
    // Logging functionality
    static std::ofstream gLog;

public:
    USBMonitor();
    ~USBMonitor();
    
    // Initialize the USB monitoring
    bool initialize();
    
    // Start monitoring USB devices
    void startMonitoring();
    
    // Stop monitoring USB devices
    void stopMonitoring();
    
    // Get list of currently connected USB devices
    std::vector<USBDeviceInfo> getConnectedDevices();
    
    // Safely eject a USB device by device ID or drive letter
    bool safelyEjectDevice(const std::string& deviceIdOrDrive);
    
    // Unsafe eject a USB device (force eject)
    bool unsafeEjectDevice(const std::string& deviceIdOrDrive);
    
    // Set callback for when a device is connected
    void setDeviceConnectedCallback(std::function<void(const USBDeviceInfo&)> callback);
    
    // Set callback for when a device is disconnected
    void setDeviceDisconnectedCallback(std::function<void(const USBDeviceInfo&)> callback);
    
    // Check if a drive letter is a USB device
    static bool isUSBDrive(const std::string& driveLetter);
    
    // Get drive type as string
    static std::string getDriveTypeString(UINT driveType);
    
    // Update the list of connected devices
    void updateDeviceList();
    
    // Process Windows device change messages
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    
    // Get device property
    static std::string getDeviceProperty(HDEVINFO deviceInfoSet, SP_DEVINFO_DATA& deviceInfoData, 
                                        const DEVPROPKEY* property);
    
    // Helper function to perform the actual ejection
    bool performEject(const USBDeviceInfo& device);
    
    // Enhanced USB monitoring functions
    static bool isUSBDriveByBusType(char driveLetter);
    std::vector<char> listRemovableLetters();
    void disarmVolume(char driveLetter, bool keepMapping = false);
    bool armVolume(char driveLetter);
    void armExistingVolumes();
    std::string maskToLetters(DWORD mask);
    bool safelyEjectDriveByLetter(char driveLetter);
    bool safelyEjectDriveViaCM(char driveLetter);
    
    // Enhanced window procedure
    static LRESULT CALLBACK enhancedWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static void logMessage(const std::string& message);
};

// Define the USB device interface GUID if not already defined
#ifndef GUID_DEVINTERFACE_USB_DEVICE
static const GUID GUID_DEVINTERFACE_USB_DEVICE = 
{0xA5DCBF10, 0x6530, 0x11D2, {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};
#endif

#endif // LAB_05_HPP