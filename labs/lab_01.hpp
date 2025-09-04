#pragma once
#include <string>

class batteryMonitor{
public:
    // bool isCharging();
    std::string getStatus();
    int getCharge();
    std::string getPowerMode();
    int hibernate();
    int sleep();

};

