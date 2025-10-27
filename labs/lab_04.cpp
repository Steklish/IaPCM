#include "lab_04.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

CameraCapture::CameraCapture(int index) : isRecording(false), cameraIndex(index) {
    // Initialize camera capture
    cap.open(index); // Open camera with specified index
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera with index " << index << "!" << std::endl;
    } else {
        std::cout << "Successfully opened camera with index " << index << std::endl;
    }
}

CameraCapture::~CameraCapture() {
    if (videoWriter.isOpened()) {
        videoWriter.release();
    }
    if (cap.isOpened()) {
        cap.release();
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
    
    // Initialize VideoWriter
    videoWriter.open(filename, cv::VideoWriter::fourcc('M','J','P','G'), fps, frame.size());
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Error: Could not create video writer!" << std::endl;
        return false;
    }
    
    videoFilename = filename;
    isRecording = true;
    std::cout << "Started recording to: " << filename << std::endl;
    return true;
}

std::string CameraCapture::stopRecording() {
    if (!isRecording) {
        std::cerr << "Warning: Not currently recording!" << std::endl;
        return "";
    }
    
    videoWriter.release();
    isRecording = false;
    
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
    
    // Initialize VideoWriter
    videoWriter.open(filename, cv::VideoWriter::fourcc('M','J','P','G'), fps, frame.size());
    
    if (!videoWriter.isOpened()) {
        std::cerr << "Error: Could not create video writer!" << std::endl;
        return false;
    }
    
    videoFilename = filename;
    isRecording = true;
    std::cout << "Started covert recording to: " << filename << std::endl;
    return true;
}

std::string CameraCapture::stopCovertRecording() {
    if (!isRecording) {
        std::cerr << "Warning: Not currently recording!" << std::endl;
        return "";
    }
    
    videoWriter.release();
    isRecording = false;
    
    std::cout << "Stopped covert recording. Video saved as: " << videoFilename << std::endl;
    return videoFilename;
}