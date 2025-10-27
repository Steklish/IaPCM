#ifndef LAB_04_HPP
#define LAB_04_HPP

#include <string>
#include <opencv2/opencv.hpp>

class CameraCapture {
private:
    cv::VideoCapture cap;
    cv::VideoWriter videoWriter;
    bool isRecording;
    std::string videoFilename;
    
public:
    CameraCapture();
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
};

#endif // LAB_04_HPP