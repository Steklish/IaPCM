#ifndef LAB_02
#define LAB_02

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

#endif // LAB_02
