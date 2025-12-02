#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#ifdef _WIN32
    #include <windows.h>
    #include <mmsystem.h>
    #include <mmdeviceapi.h>
    #include <endpointvolume.h>

    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "oleaut32.lib")
    #pragma comment(lib, "winmm.lib")

    // Simplified function to list audio devices using waveOut API
    UINT getAudioDeviceCount() {
        return waveOutGetNumDevs();
    }

    bool getAudioDeviceInfo(UINT deviceIndex, std::string& name) {
        WAVEOUTCAPS caps;
        MMRESULT result = waveOutGetDevCaps(deviceIndex, &caps, sizeof(caps));
        if (result == MMSYSERR_NOERROR) {
            // WAVEOUTCAPS.szPname is already a char array, just copy it
            name = std::string(caps.szPname);
            return true;
        }
        return false;
    }

    bool isBluetoothDevice(const std::string& name) {
        std::string lowerName = name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

        return (lowerName.find("bluetooth") != std::string::npos) ||
               (lowerName.find("headphone") != std::string::npos) ||
               (lowerName.find("headset") != std::string::npos) ||
               (lowerName.find("wireless") != std::string::npos);
    }
#endif

// Function to play audio file using Windows API
bool playAudioFile(const std::string& filename, const std::string& deviceId = "") {
    #ifdef _WIN32
        // On Windows, MCI is used to play audio files
        std::string command = "open \"" + filename + "\" type mpegvideo alias audio_file";
        MCIERROR error = mciSendStringA(command.c_str(), NULL, 0, NULL);
        
        if (error != 0) {
            char errorMsg[256];
            mciGetErrorStringA(error, errorMsg, sizeof(errorMsg));
            std::cerr << "Error opening audio file: " << errorMsg << std::endl;
            return false;
        }
        
        // Play the file
        error = mciSendStringA("play audio_file wait", NULL, 0, NULL);
        
        if (error != 0) {
            char errorMsg[256];
            mciGetErrorStringA(error, errorMsg, sizeof(errorMsg));
            std::cerr << "Error playing audio file: " << errorMsg << std::endl;
            mciSendStringA("close audio_file", NULL, 0, NULL); // Clean up
            return false;
        }
        
        // Clean up
        error = mciSendStringA("close audio_file", NULL, 0, NULL);
        if (error != 0) {
            char errorMsg[256];
            mciGetErrorStringA(error, errorMsg, sizeof(errorMsg));
            std::cout << "Warning: Error closing audio file: " << errorMsg << std::endl;
        }
        
        std::cout << "Successfully played: " << filename << std::endl;
        return true;
    #else
        // For other platforms, use system command (this is a placeholder)
        std::string command = "which ffplay > /dev/null 2>&1 && ffplay -nodisp -autoexit \"" + filename + "\" 2>/dev/null";
        command += " || which mpg123 > /dev/null 2>&1 && mpg123 \"" + filename + "\" 2>/dev/null";
        command += " || which aplay > /dev/null 2>&1 && aplay \"" + filename + "\" 2>/dev/null";
        
        int result = system(command.c_str());
        return (result == 0);
    #endif
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Bluetooth Audio Player" << std::endl;
        std::cout << "Usage: " << argv[0] << " --list" << std::endl;
        std::cout << "       " << argv[0] << " <audio_file>" << std::endl;
        std::cout << "       " << argv[0] << " <audio_file> <device_index>" << std::endl;
        std::cout << std::endl;
        std::cout << "Examples:" << std::endl;
        std::cout << "  " << argv[0] << " --list                    # List all audio devices" << std::endl;
        std::cout << "  " << argv[0] << " music.mp3                 # Play on default device" << std::endl;
        std::cout << "  " << argv[0] << " music.mp3 2               # Play on device #2 from list" << std::endl;
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "--list") {
        #ifdef _WIN32
            UINT deviceCount = getAudioDeviceCount();

            std::cout << "Available audio devices:" << std::endl;
            std::cout << "----------------------------------------" << std::endl;

            for (UINT i = 0; i < deviceCount; i++) {
                std::string name;
                if (getAudioDeviceInfo(i, name)) {
                    std::cout << i << ": " << name;
                    if (isBluetoothDevice(name)) {
                        std::cout << " [BLUETOOTH]";
                    }
                    std::cout << std::endl;
                    std::cout << "----------------------------------------" << std::endl;
                }
            }

            if (deviceCount == 0) {
                std::cout << "No audio devices found." << std::endl;
            }
        #else
            std::cout << "Listing audio devices is not implemented for this platform." << std::endl;
            std::cout << "Please use system-specific tools like 'aplay -l' on Linux." << std::endl;
        #endif
    }
    else {
        std::string filename = command;
        int deviceIndex = -1; // Use default device
        
        if (argc >= 3) {
            try {
                deviceIndex = std::stoi(argv[2]);
            } catch (const std::exception& e) {
                std::cerr << "Device index must be a number: " << e.what() << std::endl;
                return 1;
            }
        }
        
        // Note: On Windows, the MCI API doesn't easily allow specifying a specific output device
        // The full implementation would require more complex WASAPI programming
        // This is a simplified version that plays on the default audio device
        
        std::cout << "Playing audio file: " << filename << std::endl;
        if (deviceIndex >= 0) {
            std::cout << "Target device index: " << deviceIndex << " (Note: Device selection is limited in this version)" << std::endl;
        }
        
        if (!playAudioFile(filename)) {
            std::cerr << "Failed to play audio file." << std::endl;
            return 1;
        }
    }
    
    return 0;
}