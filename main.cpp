#include "crow.h"
#include "labs/functionality.hpp"
#include "labs/lab_04.hpp"
#include <filesystem>

#include <filesystem>

int main()
{
    crow::SimpleApp app;

    batteryMonitor bMonitor;
    CameraCapture camera;
    
    // Ensure output directory exists
    std::string outputDir = "static/output/";
    if (!std::filesystem::exists(outputDir)) {
        std::filesystem::create_directories(outputDir);
    }
    
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


    CROW_ROUTE(app, "/lab03")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("lab03.html").render(ctx);
        return rendered;
    });

    CROW_ROUTE(app, "/getStorageDevices")([](){
        crow::json::wvalue response;
        response["data"] = RequestInfoStorage("D:\\Study\\ИиУВМ\\WinXP\\shared\\SSD\\SSD\\output.txt");
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/lab04")([](){
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("lab04.html").render(ctx);
        return rendered;
    });

    CROW_ROUTE(app, "/takeFrame")([&camera](){
        crow::json::wvalue response;
        std::string result = camera.takeFrame();
        if (!result.empty()) {
            response["message"] = result;
            response["status"] = 200;
        } else {
            response["message"] = "Failed to take frame";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/startRecording")
    .methods(crow::HTTPMethod::POST, crow::HTTPMethod::GET)
    ([&camera](const crow::request& req){
        crow::json::wvalue response;
        std::string filename = "static/output/recording_" + std::to_string(time(nullptr)) + ".avi";
        double fps = 30.0;
        
        // Handle both query parameters and form data
        auto query_params = crow::query_string(req.url);
        
        // Parse query parameters
        auto filename_param = query_params.get("filename");
        if (filename_param) {
            filename = std::string(filename_param);
        }
        
        auto fps_param = query_params.get("fps");
        if (fps_param) {
            try {
                fps = std::stod(std::string(fps_param));
            } catch (const std::exception&) {
                // Use default FPS if parsing fails
            }
        }
        
        bool result = camera.startRecording(filename, fps);
        if (result) {
            response["message"] = "Recording started: " + filename;
            response["status"] = 200;
        } else {
            response["message"] = "Failed to start recording";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/stopRecording")([&camera](){
        crow::json::wvalue response;
        std::string result = camera.stopRecording();
        if (!result.empty()) {
            response["message"] = "Recording stopped: " + result;
            response["status"] = 200;
        } else {
            response["message"] = "No recording to stop or failed to stop";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/isCameraOpen")([&camera](){
        crow::json::wvalue response;
        bool isOpen = camera.isOpened();
        response["message"] = isOpen ? "true" : "false";
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/getCapturedFiles")([](){
        crow::json::wvalue response;
        std::vector<crow::json::wvalue> files;
        
        // List all files in the static/output directory
        std::string outputDir = "static/output/";
        for (const auto& entry : std::filesystem::directory_iterator(outputDir)) {
            if (entry.is_regular_file()) {
                crow::json::wvalue fileObj;
                fileObj["name"] = entry.path().filename().string();
                fileObj["size"] = entry.file_size();
                fileObj["extension"] = entry.path().extension().string();
                files.push_back(fileObj);
            }
        }
        
        response["files"] = std::move(files);
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/getCameraInfo")([&camera](){
        crow::json::wvalue response;
        CameraInfo info = camera.getCameraInfo();
        response["message"]["index"] = info.index;
        response["message"]["name"] = info.name;
        response["message"]["width"] = info.width;
        response["message"]["height"] = info.height;
        response["message"]["fps"] = info.fps;
        response["message"]["isOpened"] = info.isOpened;
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/listCameras")([](){
        crow::json::wvalue response;
        std::vector<CameraInfo> cameras = CameraCapture::listAvailableCameras();
        std::vector<crow::json::wvalue> cameraArray;
        for (const auto& cam : cameras) {
            crow::json::wvalue camObj;
            camObj["index"] = cam.index;
            camObj["name"] = cam.name;
            camObj["width"] = cam.width;
            camObj["height"] = cam.height;
            camObj["fps"] = cam.fps;
            camObj["isOpened"] = cam.isOpened;
            cameraArray.push_back(camObj);
        }
        response["cameras"] = std::move(cameraArray);
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/toggleCovertMode")([&camera](){
        crow::json::wvalue response;
        response["message"] = "Covert recording started for 1 second";
        response["status"] = 200;
        return response;
    });

    CROW_ROUTE(app, "/startCovertRecording")
    .methods(crow::HTTPMethod::POST, crow::HTTPMethod::GET)
    ([&camera](const crow::request& req){
        crow::json::wvalue response;
        std::string filename = "static/output/covert_recording_" + std::to_string(time(nullptr)) + ".avi";
        double fps = 30.0;
        
        // Handle both query parameters and form data
        auto query_params = crow::query_string(req.url);
        
        // Parse query parameters
        auto filename_param = query_params.get("filename");
        if (filename_param) {
            filename = std::string(filename_param);
        }
        
        auto fps_param = query_params.get("fps");
        if (fps_param) {
            try {
                fps = std::stod(std::string(fps_param));
            } catch (const std::exception&) {
                // Use default FPS if parsing fails
            }
        }
        
        bool result = camera.startCovertRecording(filename, fps);
        if (result) {
            response["message"] = "Covert recording started: " + filename;
            response["status"] = 200;
        } else {
            response["message"] = "Failed to start covert recording";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/stopCovertRecording")([&camera](){
        crow::json::wvalue response;
        std::string result = camera.stopCovertRecording();
        if (!result.empty()) {
            response["message"] = "Covert recording stopped: " + result;
            response["status"] = 200;
        } else {
            response["message"] = "No covert recording to stop";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/getPreviewFrame")([&camera](){
        crow::json::wvalue response;
        std::string result = camera.getCurrentTempFrame(); // Use temporary frame
        if (!result.empty()) {
            // Extract just the filename from the path for the URL
            std::string filename = result.substr(result.find_last_of("/\\") + 1);
            response["message"] = "/static/output/" + filename;
            response["status"] = 200;
        } else {
            response["message"] = "Failed to get preview frame";
            response["status"] = 500;
        }
        return response;
    });

    CROW_ROUTE(app, "/oneSecondCovertRecording")
    .methods(crow::HTTPMethod::POST, crow::HTTPMethod::GET)
    ([&camera](const crow::request& req){
        crow::json::wvalue response;
        std::string filename = "static/output/covert_recording_" + std::to_string(time(nullptr)) + ".avi";
        double fps = 30.0;
        
        // Handle both query parameters and form data
        auto query_params = crow::query_string(req.url);
        
        // Parse query parameters
        auto filename_param = query_params.get("filename");
        if (filename_param) {
            filename = std::string(filename_param);
        }
        
        auto fps_param = query_params.get("fps");
        if (fps_param) {
            try {
                fps = std::stod(std::string(fps_param));
            } catch (const std::exception&) {
                // Use default FPS if parsing fails
            }
        }
        
                // To run this without blocking the main thread, we run it in a separate thread
        // Use the main camera but with proper synchronization if needed
        std::thread([camera_ptr = &camera, filename, fps]() {
            camera_ptr->oneSecondCovertRecording(filename, fps);
        }).detach();
        
        response["message"] = "Started 1-second covert recording: " + filename;
        response["status"] = 200;
        return response;
    });


    app.port(8080).run();
}
