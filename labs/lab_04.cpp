#include "lab_04.hpp"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

CameraCapture::CameraCapture() : isRecording(false) {
    // Initialize camera capture
    cap.open(0); // Open default camera
    if (!cap.isOpened()) {
        std::cerr << "Error: Could not open camera!" << std::endl;
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