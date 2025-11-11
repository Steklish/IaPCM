#include "crow.h"
#include "labs/functionality.hpp"
#include "labs/lab_04.hpp"
#include "labs/lab_05.hpp"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <iostream>

#include <filesystem>

// Helper function to URL-decode a string
std::string url_decode(const std::string& encoded) {
    std::string decoded;
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            std::string hex = encoded.substr(i + 1, 2);
            char c = static_cast<char>(std::stoi(hex, nullptr, 16));
            decoded += c;
            i += 2; // Skip the next two characters
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    return decoded;
}

int main()
{
    crow::SimpleApp app;

    batteryMonitor bMonitor;
    CameraCapture camera;
    USBMonitor usbMonitor;
    
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

    CROW_ROUTE(app, "/lab05")([&usbMonitor](){
        // Initialize USB monitor if not already done
        if (!usbMonitor.initialize()) {
            std::cerr << "Failed to initialize USB monitor" << std::endl;
        }
        usbMonitor.startMonitoring();
        
        crow::mustache::context ctx;
        auto rendered = crow::mustache::load("lab05.html").render(ctx);
        return rendered;
    });

    // USB Drive Ejection endpoint (POST only)
    CROW_ROUTE(app, "/ejectUsbDrive")
        .methods(crow::HTTPMethod::POST)
        ([&usbMonitor](const crow::request& req){
            std::cout << "[DEBUG] ejectUsbDrive endpoint called" << std::endl;
            std::cout << "[DEBUG] Request body: " << req.body << std::endl;
            crow::json::wvalue response;
            
            std::string drive_letter;
            
            try {
                // The body contains multipart form data, need to extract the drive parameter manually
                std::string body = req.body;
                std::cout << "[DEBUG] Raw body size: " << body.length() << std::endl;
                
                // Look for the drive parameter in the multipart form data
                size_t drive_pos = body.find("name=\"drive\"");
                if (drive_pos != std::string::npos) {
                    // Find the end of the header, which is followed by two newlines
                    size_t header_end = body.find("\r\n\r\n", drive_pos);
                    if (header_end != std::string::npos) {
                        header_end += 4; // Skip the \r\n\r\n
                        // The value should be after the header until the next boundary
                        size_t boundary_start = body.find("--", header_end);
                        if (boundary_start != std::string::npos) {
                            // Extract the drive letter value
                            std::string value = body.substr(header_end, boundary_start - header_end);
                            // Trim whitespace
                            value.erase(0, value.find_first_not_of(" \t\r\n"));
                            value.erase(value.find_last_not_of(" \t\r\n") + 1);
                            drive_letter = value;
                            std::cout << "[DEBUG] Found drive parameter: '" << drive_letter << "'" << std::endl;
                        } else {
                            // If no boundary found, take until the end
                            std::string value = body.substr(header_end);
                            value.erase(0, value.find_first_not_of(" \t\r\n"));
                            value.erase(value.find_last_not_of(" \t\r\n") + 1);
                            drive_letter = value;
                            std::cout << "[DEBUG] Found drive parameter: '" << drive_letter << "'" << std::endl;
                        }
                    }
                } else {
                    std::cout << "[DEBUG] No drive parameter found in body" << std::endl;
                    
                    // Also try parsing as regular form data in case the content-type is different
                    auto body_query = crow::query_string(body);
                    auto drive_param = body_query.get("drive");
                    if (drive_param) {
                        drive_letter = std::string(drive_param);
                        std::cout << "[DEBUG] Found drive parameter from form parsing: " << drive_letter << std::endl;
                    }
                }
            } catch (...) {
                std::cout << "[DEBUG] Exception occurred while parsing body" << std::endl;
                // If parsing fails for any reason, return error
            }
            
            if (drive_letter.empty()) {
                response["message"] = "Drive letter parameter is required.";
                response["status"] = 400;
                return response;
            }

            // URL decode the drive letter in case it's encoded
            drive_letter = url_decode(drive_letter);
            
            if (drive_letter.length() != 1) {
                response["message"] = "Invalid drive letter format: " + drive_letter;
                response["status"] = 400;
                return response;
            }

            char drive = std::toupper(drive_letter[0]);
            if (drive < 'A' || drive > 'Z') {
                response["message"] = "Invalid drive letter: " + drive_letter;
                response["status"] = 400;
                return response;
            }

            bool result = usbMonitor.ejectUsbDrive(drive);
            if (result) {
                response["message"] = "Successfully ejected drive " + std::string(1, drive);
                response["status"] = 200;
            } else {
                response["message"] = "Failed to eject drive " + std::string(1, drive);
                response["status"] = 500;
            }
            return response;
        });

    // USB Drive Ejection via CM API endpoint (POST only)
    CROW_ROUTE(app, "/ejectUsbDriveManual")
        .methods(crow::HTTPMethod::POST)
        ([&usbMonitor](const crow::request& req){
            std::cout << "[DEBUG] ejectUsbDriveManual endpoint called" << std::endl;
            std::cout << "[DEBUG] Request body: " << req.body << std::endl;
            crow::json::wvalue response;

            std::string drive_letter;

            try {
                // The body contains multipart form data, need to extract the drive parameter manually
                std::string body = req.body;
                std::cout << "[DEBUG] Raw body size: " << body.length() << std::endl;
                
                // Look for the drive parameter in the multipart form data
                size_t drive_pos = body.find("name=\"drive\"");
                if (drive_pos != std::string::npos) {
                    // Find the end of the header, which is followed by two newlines
                    size_t header_end = body.find("\r\n\r\n", drive_pos);
                    if (header_end != std::string::npos) {
                        header_end += 4; // Skip the \r\n\r\n
                        // The value should be after the header until the next boundary
                        size_t boundary_start = body.find("--", header_end);
                        if (boundary_start != std::string::npos) {
                            // Extract the drive letter value
                            std::string value = body.substr(header_end, boundary_start - header_end);
                            // Trim whitespace
                            value.erase(0, value.find_first_not_of(" \t\r\n"));
                            value.erase(value.find_last_not_of(" \t\r\n") + 1);
                            drive_letter = value;
                            std::cout << "[DEBUG] Found drive parameter: '" << drive_letter << "'" << std::endl;
                        } else {
                            // If no boundary found, take until the end
                            std::string value = body.substr(header_end);
                            value.erase(0, value.find_first_not_of(" \t\r\n"));
                            value.erase(value.find_last_not_of(" \t\r\n") + 1);
                            drive_letter = value;
                            std::cout << "[DEBUG] Found drive parameter: '" << drive_letter << "'" << std::endl;
                        }
                    }
                } else {
                    std::cout << "[DEBUG] No drive parameter found in body" << std::endl;
                    
                    // Also try parsing as regular form data in case the content-type is different
                    auto body_query = crow::query_string(body);
                    auto drive_param = body_query.get("drive");
                    if (drive_param) {
                        drive_letter = std::string(drive_param);
                        std::cout << "[DEBUG] Found drive parameter from form parsing: " << drive_letter << std::endl;
                    }
                }
            } catch (...) {
                std::cout << "[DEBUG] Exception occurred while parsing body" << std::endl;
                // If parsing fails for any reason, return error
            }
            
            if (drive_letter.empty()) {
                response["message"] = "Drive letter parameter is required.";
                response["status"] = 400;
                return response;
            }

            // URL decode the drive letter in case it's encoded
            drive_letter = url_decode(drive_letter);
            
            if (drive_letter.length() != 1) {
                response["message"] = "Invalid drive letter format: " + drive_letter;
                response["status"] = 400;
                return response;
            }

            char drive = std::toupper(drive_letter[0]);
            if (drive < 'A' || drive > 'Z') {
                response["message"] = "Invalid drive letter: " + drive_letter;
                response["status"] = 400;
                return response;
            }

            int result = usbMonitor.ejectUsbDriveManual(drive);
            if (result == 0) {
                response["message"] = "Successfully ejected drive " + std::string(1, drive) + " via CM API";
                response["status"] = 200;
            } else {
                response["message"] = "Failed to eject drive " + std::string(1, drive) + " via CM API, error code: " + std::to_string(result);
                response["status"] = 500;
            }
            return response;
        });

    // Disable USB Mouse endpoint
    CROW_ROUTE(app, "/disableUsbMouse")
        .methods(crow::HTTPMethod::POST)
        ([&usbMonitor](){
            std::cout << "[DEBUG] disableUsbMouse endpoint called" << std::endl;
            crow::json::wvalue response;
            int result = usbMonitor.disableUsbMouseManual();
            if (result == 0) {
                response["message"] = "Successfully disabled USB mouse";
                response["status"] = 200;
            } else {
                response["message"] = "Failed to disable USB mouse, error code: " + std::to_string(result);
                response["status"] = 500;
            }
            return response;
        });

    // List USB drives endpoint
    CROW_ROUTE(app, "/listUsbDrives")
        .methods(crow::HTTPMethod::GET)
        ([&usbMonitor](){
            std::cout << "[DEBUG] listUsbDrives endpoint called" << std::endl;
            crow::json::wvalue response;
            std::vector<char> drives = usbMonitor.listRemovableDrives();
            
            std::vector<crow::json::wvalue> driveArray;
            for (char d : drives) {
                crow::json::wvalue driveObj;
                driveObj["letter"] = std::string(1, d);
                driveObj["path"] = std::string(1, d) + ":\\";
                driveArray.push_back(driveObj);
            }
            
            response["drives"] = std::move(driveArray);
            response["status"] = 200;
            return response;
        });

    // List input devices endpoint
    CROW_ROUTE(app, "/listInputDevices")
        .methods(crow::HTTPMethod::GET)
        ([&usbMonitor](){
            std::cout << "[DEBUG] listInputDevices endpoint called" << std::endl;
            crow::json::wvalue response;
            std::vector<InputDevice> devices = usbMonitor.listInputDevices();
            
            std::vector<crow::json::wvalue> deviceArray;
            for (const auto& device : devices) {
                crow::json::wvalue deviceObj;
                deviceObj["name"] = device.name;
                deviceObj["type"] = device.type;
                deviceObj["vid"] = device.vid;
                deviceObj["pid"] = device.pid;
                deviceObj["connected"] = device.connected;
                deviceArray.push_back(deviceObj);
            }
            
            response["devices"] = std::move(deviceArray);
            response["status"] = 200;
            return response;
        });

    app.port(8080).run();
}
