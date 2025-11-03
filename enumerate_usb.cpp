#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// Проверяем, является ли диск USB устройством
static bool IsUsbDrive(char letter) {
    std::string path = "\\\\.\\";
    path.push_back(letter);
    path.push_back(':');
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    char buffer[1024];
    DWORD bytes;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer, sizeof(buffer), &bytes, nullptr);
    CloseHandle(h);

    if (!ok) return false;
    auto* desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
    return (desc->BusType == BusTypeUsb);
}

// перечисляем буквы съёмных томов И USB дисков (включая те, что помечены как Fixed)
static std::vector<char> list_removable_letters() {
    std::vector<char> out;
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (!(mask & (1 << i))) continue;
        char letter = (char)('A' + i);
        char root[] = { letter, ':', '\\', 0 };
        UINT t = GetDriveTypeA(root);
        // Включаем: (1) явно removable, (2) fixed USB диски
        if (t == DRIVE_REMOVABLE || t == DRIVE_CDROM) {
            out.push_back(letter);
        }
        else if (t == DRIVE_FIXED) {
            bool isUsb = IsUsbDrive(letter);
            if (isUsb) {
                out.push_back(letter);
                std::cout << "  Найден USB диск (помечен как Fixed): " << letter << ":" << std::endl;
            }
        }
    }
    return out;
}

// Программное безопасное извлечение USB по букве диска (из статьи kaimi.io)
static bool SafelyEjectDrive(char driveLetter) {
    std::string path = "\\\\.\\";
    path.push_back(driveLetter);
    path.push_back(':');

    std::cout << "Attempting to safely eject drive " << driveLetter << ":" << std::endl;

    // Открываем том с правами на чтение/запись
    HANDLE hVolume = CreateFileA(
        path.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hVolume == INVALID_HANDLE_VALUE) {
        std::cout << "Failed to open volume for eject: " << GetLastError() << std::endl;
        return false;
    }

    DWORD bytesReturned;
    BOOL result;

    // Шаг 1: Блокируем том
    result = DeviceIoControl(
        hVolume,
        FSCTL_LOCK_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    if (!result) {
        std::cout << "Failed to lock volume: " << GetLastError() << std::endl;
        CloseHandle(hVolume);
        return false;
    }

    std::cout << "Volume locked successfully" << std::endl;

    // Шаг 2: Размонтируем том
    result = DeviceIoControl(
        hVolume,
        FSCTL_DISMOUNT_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    if (!result) {
        std::cout << "Failed to dismount volume: " << GetLastError() << std::endl;
        // Разблокируем том
        DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        CloseHandle(hVolume);
        return false;
    }

    std::cout << "Volume dismounted successfully" << std::endl;

    // Шаг 3: Запрещаем удаление (prevent removal)
    PREVENT_MEDIA_REMOVAL pmr;
    pmr.PreventMediaRemoval = FALSE;

    result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_MEDIA_REMOVAL,
        &pmr, sizeof(pmr),
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    if (!result) {
        std::cout << "Warning: Failed to allow media removal: " << GetLastError() << std::endl;
    }

    // Шаг 4: Извлекаем носитель
    result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_EJECT_MEDIA,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    CloseHandle(hVolume);

    if (!result) {
        DWORD err = GetLastError();
        std::cout << "Failed to eject media: " << err << std::endl;
        return false;
    }

    std::cout << "Drive " << driveLetter << ": ejected successfully!" << std::endl;
    return true;
}

int main() {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);

    std::cout << "=== USB Device Enumeration and Ejection Tool ===" << std::endl;

    // Enumerate removable drives
    std::vector<char> drives = list_removable_letters();
    
    if (drives.empty()) {
        std::cout << "No removable drives found." << std::endl;
    } else {
        std::cout << "Found removable drives: ";
        for (char drive : drives) {
            std::cout << drive << ": ";
        }
        std::cout << std::endl;

        // Ask user which drive to eject
        std::cout << "\nAvailable drives for ejection: ";
        for (char drive : drives) {
            std::cout << drive << ": ";
        }
        std::cout << std::endl;
        std::cout << "Enter drive letter to safely eject (or 'q' to quit): ";
        
        char input;
        std::cin >> input;
        
        input = toupper(input);
        
        bool found = false;
        for (char drive : drives) {
            if (drive == input) {
                found = true;
                break;
            }
        }
        
        if (found) {
            SafelyEjectDrive(input);
        } else if (input != 'Q') {
            std::cout << "Invalid drive letter." << std::endl;
        }
    }

    return 0;
}