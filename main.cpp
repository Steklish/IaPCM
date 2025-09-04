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

    CROW_ROUTE(app, "/sleep")([&bMonitor](){
        bMonitor.sleep();
        return "Sleeping...";
    });
    
    CROW_ROUTE(app, "/hibernate")([&bMonitor](){
        bMonitor.hibernate();
        return "Hibernating...";
    });

    app.port(8080).run();
}
