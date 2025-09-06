#pragma once
#include <string>

class batteryMonitor{
public:
    batteryMonitor();
    std::string getStatus();
    int getCharge();
    std::string getPowerMode();
    int hibernate();
    int sleep();
    // info as a string
    std::string getBatteryInfo();
    // time in seconds
    int getTimeLeft(); 

    std::string isEco();
};

