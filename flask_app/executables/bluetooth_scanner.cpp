#include <iostream>
#include <vector>
#include <string>

#ifdef _WIN32
    #include <windows.h>
    #include <bluetoothapis.h>
    #include <ws2bth.h>
    #include <ws2def.h>
    #pragma comment(lib, "Bthprops.lib")
    #pragma comment(lib, "Ws2_32.lib")
#endif

struct BluetoothDevice {
    std::string name;
    std::string address;
    int rssi; // Signal strength
    
    BluetoothDevice(const std::string& n, const std::string& a, int r = 0) 
        : name(n), address(a), rssi(r) {}
};

#ifdef _WIN32
std::vector<BluetoothDevice> scan_bluetooth_devices() {
    std::vector<BluetoothDevice> devices;

    // Initialize Windows Socket for Bluetooth
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "WSAStartup failed: " << result << std::endl;
        return devices;
    }

    // Scan for all types of devices: connected, paired, and unpaired
    BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams = { 0 };
    searchParams.dwSize = sizeof(searchParams);
    searchParams.fReturnConnected = TRUE;    // Return connected devices
    searchParams.fReturnRemembered = TRUE;   // Return remembered/paired devices
    searchParams.fReturnAuthenticated = TRUE; // Return authenticated/paired devices
    searchParams.fReturnUnknown = TRUE;      // Also return unknown/new devices
    searchParams.fIssueInquiry = TRUE;       // Actively scan for discoverable devices
    searchParams.cTimeoutMultiplier = 4;     // 40 seconds timeout for comprehensive discovery

    BLUETOOTH_DEVICE_INFO deviceInfo = { 0 };
    deviceInfo.dwSize = sizeof(deviceInfo);

    // Find the first new device
    HBLUETOOTH_DEVICE_FIND hDeviceFind = BluetoothFindFirstDevice(&searchParams, &deviceInfo);
    if (hDeviceFind) {
        do {
            // Only process if the device has a name
            if (deviceInfo.szName[0] != L'\0') {
                std::string deviceName;
                // Convert wide string name to UTF-8 string with better encoding handling
                int bufferSize = WideCharToMultiByte(CP_UTF8, 0, deviceInfo.szName, -1, NULL, 0, NULL, NULL);
                if (bufferSize > 0) {
                    char* tempBuffer = new char[bufferSize];
                    int result = WideCharToMultiByte(CP_UTF8, 0, deviceInfo.szName, -1, tempBuffer, bufferSize, NULL, NULL);
                    if (result > 0) {
                        deviceName = std::string(tempBuffer);
                    } else {
                        // If UTF-8 conversion fails, fallback to system default
                        int fallbackSize = WideCharToMultiByte(CP_ACP, 0, deviceInfo.szName, -1, NULL, 0, NULL, NULL);
                        if (fallbackSize > 0) {
                            char* fallbackBuffer = new char[fallbackSize];
                            WideCharToMultiByte(CP_ACP, 0, deviceInfo.szName, -1, fallbackBuffer, fallbackSize, NULL, NULL);
                            deviceName = std::string(fallbackBuffer);
                            delete[] fallbackBuffer;
                        } else {
                            // Last resort: convert character by character
                            for (int i = 0; deviceInfo.szName[i] != L'\0' && i < 255; ++i) {
                                if (deviceInfo.szName[i] < 128) {
                                    deviceName += static_cast<char>(deviceInfo.szName[i]);
                                } else {
                                    deviceName += '?'; // Replace non-ASCII characters with '?'
                                }
                            }
                        }
                    }
                    delete[] tempBuffer;
                }

                // Format Bluetooth address
                BLUETOOTH_ADDRESS* btAddr = &deviceInfo.Address;
                char addrStr[32];
                snprintf(addrStr, sizeof(addrStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                         btAddr->rgBytes[5], btAddr->rgBytes[4], btAddr->rgBytes[3],
                         btAddr->rgBytes[2], btAddr->rgBytes[1], btAddr->rgBytes[0]);

                // Create a device entry with info about connection status
                std::string fullDeviceName = deviceName;
                if (deviceInfo.fConnected) {
                    fullDeviceName += " [CONNECTED]";
                } else if (deviceInfo.fRemembered) {
                    fullDeviceName += " [PAIRED]";
                } else if (deviceInfo.fAuthenticated) {
                    fullDeviceName += " [PAIRED]";
                } else {
                    fullDeviceName += " [DISCOVERABLE]";
                }

                devices.emplace_back(fullDeviceName, addrStr);
            }

        } while (BluetoothFindNextDevice(hDeviceFind, &deviceInfo));

        BluetoothFindDeviceClose(hDeviceFind);
    }

    WSACleanup();
    return devices;
}
#else
// Placeholder for non-Windows platforms
std::vector<BluetoothDevice> scan_bluetooth_devices() {
    std::vector<BluetoothDevice> devices;
    std::cerr << "Bluetooth scanning is not implemented for this platform." << std::endl;
    return devices;
}
#endif

int main(int argc, char* argv[]) {
    #ifdef _WIN32
        // Set console code page to UTF-8 to properly display Unicode characters
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
    #endif

    std::cout << "Scanning for Bluetooth devices..." << std::endl;

    std::vector<BluetoothDevice> devices = scan_bluetooth_devices();

    if (devices.empty()) {
        std::cout << "No Bluetooth devices found or scanning failed." << std::endl;
    } else {
        std::cout << "Found " << devices.size() << " Bluetooth device(s):" << std::endl;
        std::cout << "----------------------------------------" << std::endl;

        for (const auto& device : devices) {
            std::cout << "Name: " << device.name << std::endl;
            std::cout << "Address: " << device.address << std::endl;
            std::cout << "----------------------------------------" << std::endl;
        }
    }

    #ifdef _WIN32
        // Provide additional information for users with Unicode issues
        std::cout << "\nNote: If Unicode characters appear incorrectly," << std::endl;
        std::cout << "try running this program in a terminal that supports UTF-8." << std::endl;
    #endif

    return 0;
}