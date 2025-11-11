#ifndef LAB_05_HPP
#define LAB_05_HPP

#include <string>
#include <vector>
#include <map>

// Function declarations for USB operations
std::vector<char> listRemovableDrives();
bool ejectUsbDrive(char driveLetter);
int ejectUsbDriveManual(char driveLetter);
int disableUsbMouseManual();
std::string getUSBLog();

// Structure to hold input device info
struct InputDevice {
    std::string name;
    std::string type;  // "mouse", "keyboard", etc.
    std::string vid;   // Vendor ID
    std::string pid;   // Product ID
    bool connected;
};

// Function to list input devices
std::vector<InputDevice> listInputDevices();

// For now, keep the existing USBMonitor class as a wrapper
class USBMonitor {
public:
    USBMonitor();
    ~USBMonitor();
    
    bool initialize();
    void startMonitoring();
    void stopMonitoring();
    
    // Function to list removable drives
    std::vector<char> listRemovableDrives();
    
    // Function to eject a USB drive
    bool ejectUsbDrive(char driveLetter);
    
    // Function to eject a USB drive via CM API
    int ejectUsbDriveManual(char driveLetter);
    
    // Function to disable USB mouse
    int disableUsbMouseManual();
    
    // Function to list input devices
    std::vector<InputDevice> listInputDevices();
    
    // Get current log
    std::string getCurrentLog();
    
private:
    bool isMonitoring;
    // Add other private members as needed
};

#endif // LAB_05_HPP