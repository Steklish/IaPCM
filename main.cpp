#include "crow.h"
#include "labs/functionality.hpp"

int main()
{
    crow::SimpleApp app;

    batteryMonitor bMonitor;
    
    // Set the base folder for templates
    crow::mustache::set_base("templates");

    CROW_ROUTE(app, "/")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("index.html").render(ctx);
        return rendered;
    });

    // LAB 01 - battery infj
    
    CROW_ROUTE(app, "/lab01")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("lab01.html").render(ctx);
        return rendered;
    });

    CROW_ROUTE(app, "/getCharge")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.getCharge();
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/getStatus")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.getStatus();
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/getPowerMode")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.getPowerMode();
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/getInfo")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.getBatteryInfo();
        response["status"] = 200;
        return response;
    });
    
    //retutns time left in seconds
    CROW_ROUTE(app, "/getTimeLeft")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.getTimeLeft();
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/sleep")([&bMonitor](){
        bMonitor.sleep();
        return "Sleeping...";
    });
    
    CROW_ROUTE(app, "/hibernate")([&bMonitor](){
        bMonitor.hibernate();
        return "Hibernating...";
    });

    CROW_ROUTE(app, "/isEco")([&bMonitor](){
        crow::json::wvalue response;
        response["message"] = bMonitor.isEco();
        response["status"] = 200;
        return response;
    });
    // LAB 01 - battery infj end


    
    CROW_ROUTE(app, "/lab02")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("lab02.html").render(ctx);
        return rendered;
    });

    // New endpoint: returns PCI devices info (VID/DID)
    CROW_ROUTE(app, "/getPCIDevices")([](){
        crow::json::wvalue response;
        std::vector<std::pair<std::string, std::string>> devices = EnumeratePCIDevices();
        std::vector<crow::json::wvalue> deviceArray;
        int id = 1;
        for (const auto& dev : devices) {
            crow::json::wvalue devObj;
            devObj["id"] = id++;
            devObj["VenID"] = dev.first;
            devObj["DevID"] = dev.second;
            deviceArray.push_back(devObj);
        }
        response["devices"] = std::move(deviceArray);
        response["status"] = 200;
        return response;
    });


    app.port(8080).run();
}
