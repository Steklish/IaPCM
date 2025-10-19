#include "./lab_03.hpp"
#include <stringapiset.h>

std::wstring StringToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::string RequestInfoStorage(const std::string& filename) {
    std::wstring wfilename = StringToWString(filename);
    std::wifstream file(wfilename.c_str(), std::ios::in | std::ios::binary);
    if (!file) {
        std::wcerr << L"Failed to open file: " << wfilename << std::endl;
        return "";
    }
    std::wostringstream wss;
    wss << file.rdbuf();
    std::wstring wcontent = wss.str();
    // Convert wide string content back to std::string if needed.
    // For example, convert from UTF-16 to UTF-8 here.
    return std::string(wcontent.begin(), wcontent.end()); // Simplified conversion
}