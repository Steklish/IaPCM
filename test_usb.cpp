#include "labs/lab_05.hpp"
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Set console to support Unicode
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    
    std::cout << "Testing USB Ejection Functionality..." << std::endl;
    
    USBMonitor monitor;
    
    if (!monitor.initialize()) {
        std::cerr << "Failed to initialize USB monitor" << std::endl;
        return -1;
    }
    
    std::cout << "USB Monitor initialized successfully!" << std::endl;
    
    // List removable drives
    std::vector<char> drives = monitor.listRemovableLetters();
    std::cout << "Removable drives found: ";
    for (char drive : drives) {
        std::cout << drive << ": ";
    }
    std::cout << std::endl;
    
    if (!drives.empty()) {
        std::cout << "Attempting to safely eject drive " << drives[0] << ":" << std::endl;
        bool result = monitor.safelyEjectDriveByLetter(drives[0]);
        std::cout << "Eject result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
    } else {
        std::cout << "No removable drives found to test ejection." << std::endl;
    }
    
    std::cout << "Starting monitoring thread..." << std::endl;
    monitor.startMonitoring();
    
    std::cout << "Monitor running for 10 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    monitor.stopMonitoring();
    
    std::cout << "Test completed." << std::endl;
    return 0;
}