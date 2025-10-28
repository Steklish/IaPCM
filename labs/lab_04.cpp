#include "lab_04.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

CameraCapture::CameraCapture(int index) : isRecording(false), cameraIndex(index), lastTempFramePath("") {
    // Initialize camera capture
    cap.open(index); // Open camera with specified index
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera with index " << index << "!" << std::endl;
    } else {
        std::cout << "Successfully opened camera with index " << index << std::endl;
    }
}

CameraCapture::~CameraCapture() {
    // Stop any active recording
    if (isRecording) {
        recordingActive = false;
        if (recordingThread.joinable()) {
            recordingThread.join();
        }
    }
    
    if (videoWriter.isOpened()) {
        videoWriter.release();
    }
    if (cap.isOpened()) {
        cap.release();
    }
    // Delete the last temp frame if it exists
    if (!lastTempFramePath.empty() && std::filesystem::exists(lastTempFramePath)) {
        std::filesystem::remove(lastTempFramePath);
    }
}

std::string CameraCapture::takeFrame() {
    cv::Mat frame;
    if (!cap.read(frame)) {
        std::cerr << "Error: Could not read frame from camera!" << std::endl;
        return "";
    }
    
    // Generate filename with timestamp
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream filename;
    filename << "static/output/frame_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".jpg";
    
    // Save the frame to file
    if (!cv::imwrite(filename.str(), frame)) {
        std::cerr << "Error: Could not save frame to " << filename.str() << std::endl;
        return "";
    }
    
    std::cout << "Frame saved as: " << filename.str() << std::endl;
    return filename.str();
}

bool CameraCapture::startRecording(const std::string& filename, double fps) {
    if (isRecording) {
        std::cerr << "Error: Already recording!" << std::endl;
        return false;
    }
    
    cv::Mat frame;
    if (!cap.read(frame)) {
        std::cerr << "Error: Could not read frame from camera!" << std::endl;
        return false;
    }
    
    // Check if output directory exists
    std::string dir = filename.substr(0, filename.find_last_of("/\\"));
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    
    // Initialize VideoWriter - using MJPG codec as in the reference implementation
    videoWriter.open(filename, cv::VideoWriter::fourcc('M','J','P','G'), fps, frame.size());
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Error: Could not create video writer!" << std::endl;
        return false;
    }
    
    videoFilename = filename;
    isRecording = true;
    recordingActive = true;
    
    // Start continuous recording thread like in the reference implementation
    recordingThread = std::thread([this, fps]() {
        while (recordingActive.load() && isRecording) {
            cv::Mat frame;
            if (cap.read(frame) && !frame.empty()) {
                if (videoWriter.isOpened()) {
                    videoWriter.write(frame);
                } else {
                    std::cerr << "Error: VideoWriter is not open during recording" << std::endl;
                    break;
                }
            }
            // Sleep to maintain frame rate (33ms for ~30fps)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });
    
    std::cout << "Started recording to: " << filename << std::endl;
    return true;
}

std::string CameraCapture::stopRecording() {
    if (!isRecording) {
        std::cerr << "Warning: Not currently recording!" << std::endl;
        return "";
    }
    
    // Stop the recording thread
    recordingActive = false;
    isRecording = false;
    
    // Wait for the recording thread to finish (with timeout)
    if (recordingThread.joinable()) {
        recordingThread.join();  // Wait for thread to complete
    }
    
    // Ensure all frames are written before closing
    if (videoWriter.isOpened()) {
        videoWriter.release();
    }
    
    std::cout << "Stopped recording. Video saved as: " << videoFilename << std::endl;
    return videoFilename;
}

bool CameraCapture::isOpened() const {
    return cap.isOpened();
}

cv::Mat CameraCapture::getCurrentFrame() {
    cv::Mat frame;
    if (!cap.read(frame)) {
        std::cerr << "Error: Could not read frame from camera!" << std::endl;
    }
    return frame;
}

CameraInfo CameraCapture::getCameraInfo() const {
    CameraInfo info;
    info.index = cameraIndex;
    info.isOpened = cap.isOpened();
    
    if (cap.isOpened()) {
        info.name = "Camera " + std::to_string(cameraIndex);
        info.width = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        info.height = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        info.fps = cap.get(cv::CAP_PROP_FPS);
    } else {
        info.name = "Unavailable";
        info.width = 0;
        info.height = 0;
        info.fps = 0.0;
    }
    
    return info;
}

std::vector<CameraInfo> CameraCapture::listAvailableCameras() {
    std::vector<CameraInfo> cameras;
    
    // Try to open up to 10 camera devices
    for (int i = 0; i < 10; i++) {
        cv::VideoCapture testCap(i);
        if (testCap.isOpened()) {
            CameraInfo info;
            info.index = i;
            info.isOpened = true;
            info.name = "Camera " + std::to_string(i);
            info.width = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_WIDTH));
            info.height = static_cast<int>(testCap.get(cv::CAP_PROP_FRAME_HEIGHT));
            info.fps = testCap.get(cv::CAP_PROP_FPS);
            cameras.push_back(info);
            testCap.release();
        } else {
            testCap.release();
        }
    }
    
    return cameras;
}

bool CameraCapture::setCovertMode(bool enabled) {
    // On Windows, we can hide the console window
#ifdef _WIN32
    HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow != NULL) {
        if (enabled) {
            ShowWindow(consoleWindow, SW_HIDE);
        } else {
            ShowWindow(consoleWindow, SW_SHOW);
        }
    }
    return true;
#else
    // On other platforms, we can't hide the console in the same way
    return false;
#endif
}

bool CameraCapture::startCovertRecording(const std::string& filename, double fps) {
    if (isRecording) {
        std::cerr << "Error: Already recording!" << std::endl;
        return false;
    }
    
    cv::Mat frame;
    if (!cap.read(frame)) {
        std::cerr << "Error: Could not read frame from camera!" << std::endl;
        return false;
    }
    
    // Check if output directory exists
    std::string dir = filename.substr(0, filename.find_last_of("/\\"));
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    
    // Initialize VideoWriter - using MJPG codec as in the reference implementation
    videoWriter.open(filename, cv::VideoWriter::fourcc('M','J','P','G'), fps, frame.size());
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Error: Could not create video writer!" << std::endl;
        return false;
    }
    
    videoFilename = filename;
    isRecording = true;
    recordingActive = true;
    
    // Start continuous recording thread like in the reference implementation
    recordingThread = std::thread([this, fps]() {
        while (recordingActive.load() && isRecording) {
            cv::Mat frame;
            if (cap.read(frame) && !frame.empty()) {
                if (videoWriter.isOpened()) {
                    videoWriter.write(frame);
                } else {
                    std::cerr << "Error: VideoWriter is not open during recording" << std::endl;
                    break;
                }
            }
            // Sleep to maintain frame rate (33ms for ~30fps)
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }
    });
    
    std::cout << "Started covert recording to: " << filename << std::endl;
    return true;
}

std::string CameraCapture::stopCovertRecording() {
    if (!isRecording) {
        std::cerr << "Warning: Not currently recording!" << std::endl;
        return "";
    }
    
    // Stop the recording thread
    recordingActive = false;
    isRecording = false;
    
    // Wait for the recording thread to finish (with timeout)
    if (recordingThread.joinable()) {
        recordingThread.join();  // Wait for thread to complete
    }
    
    // Ensure all frames are written before closing
    if (videoWriter.isOpened()) {
        videoWriter.release();
    }
    
    std::cout << "Stopped covert recording. Video saved as: " << videoFilename << std::endl;
    return videoFilename;
}

void CameraCapture::oneSecondCovertRecording(const std::string& filename, double fps) {
    // Start the recording
    if (!startCovertRecording(filename, fps)) {
        std::cerr << "Error: Could not start covert recording!" << std::endl;
        return;
    }
    
    // Record for 1 second
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // Stop the recording
    stopCovertRecording();
}

std::string CameraCapture::getCurrentTempFrame() {
    // Don't create temp frames if we're currently recording
    if (isRecording) {
        return ""; // Return empty string if recording to avoid conflicts
    }
    
    cv::Mat frame = getCurrentFrame();
    
    if (frame.empty()) {
        std::cerr << "Error: Could not get current frame!" << std::endl;
        return "";
    }
    
    // Delete the previous temp frame if it exists
    if (!lastTempFramePath.empty()) {
        std::error_code ec;
        if (std::filesystem::exists(lastTempFramePath, ec)) {
            std::filesystem::remove(lastTempFramePath, ec);
            if (ec) {
                std::cerr << "Error deleting previous temp frame: " << ec.message() << std::endl;
            } else {
                std::cout << "Deleted previous temp frame: " << lastTempFramePath << std::endl;
            }
        }
    }
    
    // Generate a temporary filename with a special prefix
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << "static/output/temp_preview_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << "_" << rand() << ".jpg";
    std::string outputFilename = oss.str();
    
    // Save the frame to file
    if (!cv::imwrite(outputFilename, frame)) {
        std::cerr << "Error: Could not save temporary frame to " << outputFilename << std::endl;
        return "";
    }
    
    // Update the last temp frame path
    lastTempFramePath = outputFilename;
    std::cout << "Created temporary preview frame: " << outputFilename << std::endl;
    return outputFilename;
}