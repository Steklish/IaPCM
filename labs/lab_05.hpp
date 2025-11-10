#ifndef LAB_05_HPP
#define LAB_05_HPP

#include <windows.h>
#include <initguid.h>
#include <devguid.h>
#include <dbt.h>
#include <setupapi.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <cstdio>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <thread>
#include <chrono>

// Structure to hold state information
struct State {
    HWND hwnd{};
    std::unordered_map<char, HANDLE> volHandle;
    std::unordered_map<char, HDEVNOTIFY> volNotify;
    std::unordered_map<HANDLE, char> handleToLetter;
    std::unordered_map<char, bool> safePending;
    std::unordered_map<char, bool> queryFailed;
    std::unordered_map<char, bool> alreadyReported;
};

// USB Device Info structure
struct USBDeviceInfo {
    std::string deviceID;
    std::string deviceName;
    std::string driveLetter;
    std::string deviceType;
    bool isMounted;
    bool isRemovable;
    std::string deviceClass;  // Additional field for device class (e.g., "Disk", "HID", "Input")
};

// Function declarations
void logf(const std::string& s);
bool IsUsbDrive(char letter);
std::vector<char> list_removable_letters();
void Disarm(char L, bool keepMapping = false);
bool Arm(char L);
std::string maskToLetters(DWORD um);
void ArmExisting();
bool SafelyEjectDrive(char driveLetter);
int EjectUsbDriveManual(char driveLetter);
bool SafelyEjectDriveViaCM(char driveLetter);
int DisableUsbMouseManual();
int DisableUsbKeyboardManual();
int EnableUsbDeviceByHardwareId(const char* hardwareId);
bool FindUsbDeviceByHardwareId(const char* hardwareId, DEVINST* devInst);
void UpdateDeviceInRegistry(DEVINST devInst);

// USB Monitor class
class USBMonitor {
public:
    USBMonitor();
    ~USBMonitor();
    
    bool initialize();
    void startMonitoring();
    std::vector<USBDeviceInfo> getConnectedDevices();
    bool safelyEjectDevice(const std::string& deviceId);
    bool unsafeEjectDevice(const std::string& deviceId);
    bool disableInputDevice(const std::string& deviceType); // "mouse" or "keyboard"
    bool enableInputDevice(const std::string& hardwareId); // By hardware ID
    std::vector<USBDeviceInfo> getConnectedInputDevices();
    
private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void monitorThreadFunc();
    std::thread monitorThread;
    bool monitoring;
    HWND messageHwnd;
    static USBMonitor* instance;
};

#endif // LAB_05_HPP