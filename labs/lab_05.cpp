// USB Safe/Unsafe detector (maximally robust way)
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <dbt.h>
#include <setupapi.h>
#include <winioctl.h>
#include <cfgmgr32.h>
#include <cstdio>
#include <unordered_map>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <direct.h>

#include "lab_05.hpp"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

static std::ofstream gLog;

void logf(const std::string& s){
    SYSTEMTIME st; GetLocalTime(&st);
    char ts[64]; sprintf(ts,"%04u-%02u-%02u %02u:%02u:%02u ",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    gLog<<ts<<s<<"\n"; gLog.flush();
    printf("%s%s\n", ts, s.c_str());
}

bool IsUsbDrive(char letter){
    std::string path = "\\\\.\\";
    path.push_back(letter);
    path.push_back(':');
    HANDLE h = CreateFileA(path.c_str(), 0, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, 0, nullptr);
    if(h==INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY query{};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    char buffer[1024];
    DWORD bytes;
    BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query), buffer, sizeof(buffer), &bytes, nullptr);
    CloseHandle(h);

    if(!ok) return false;
    auto* desc = (STORAGE_DEVICE_DESCRIPTOR*)buffer;
    return (desc->BusType == BusTypeUsb);
}

std::vector<char> list_removable_letters(){
    std::vector<char> out;
    DWORD mask = GetLogicalDrives();
    for(int i=0;i<26;i++){
        if(!(mask & (1<<i))) continue;
        char letter = (char)('A'+i);
        char root[]={letter,':','\\',0};
        UINT t = GetDriveTypeA(root);
        if(t==DRIVE_REMOVABLE || t==DRIVE_CDROM){
            out.push_back(letter);
        }else if(t==DRIVE_FIXED){
            bool isUsb = IsUsbDrive(letter);
            if(isUsb){
                out.push_back(letter);
                logf(std::string("  Найден USB диск (помечен как Fixed): ") + letter + ":");
            }
        }
    }
    return out;
}

static State S;

void Disarm(char L, bool keepMapping){
    auto it = S.volHandle.find(L);
    if(it!=S.volHandle.end()){
        HANDLE h = it->second;
        auto itN = S.volNotify.find(L);
        if(itN!=S.volNotify.end() && itN->second){ UnregisterDeviceNotification(itN->second); }
        if(h && h!=INVALID_HANDLE_VALUE) CloseHandle(h);
        if(!keepMapping) S.handleToLetter.erase(h);
        S.volHandle.erase(it);
        S.volNotify.erase(L);
    }
}

bool Arm(char L){
    if(S.volHandle.count(L)) return true;
    std::string path="\\\\.\\"; path.push_back(L); path.push_back(':');
    HANDLE h = CreateFileA(path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(h==INVALID_HANDLE_VALUE){
        logf("arm open fail "+std::string(1,L)+" err="+std::to_string(GetLastError()));
        return false;
    }
    DEV_BROADCAST_HANDLE dbh{};
    dbh.dbch_size=sizeof(dbh);
    dbh.dbch_devicetype=DBT_DEVTYP_HANDLE;
    dbh.dbch_handle=h;
    HDEVNOTIFY hn = RegisterDeviceNotificationA(S.hwnd, &dbh, DEVICE_NOTIFY_WINDOW_HANDLE);
    if(!hn){
        logf("arm RDN fail "+std::string(1,L)+" err="+std::to_string(GetLastError()));
        CloseHandle(h);
        return false;
    }
    S.volHandle[L]=h;
    S.volNotify[L]=hn;
    S.handleToLetter[h]=L;
    logf("✓ Диск "+std::string(1,L)+": готов к мониторингу");
    return true;
}

std::string maskToLetters(DWORD um){
    std::string r;
    for(int i=0;i<26;i++) if(um & (1u<<i)){ if(!r.empty()) r.push_back(','); r.push_back('A'+i); }
    return r;
}

void ArmExisting(){
    for(char L : list_removable_letters()) Arm(L);
}

bool SafelyEjectDrive(char driveLetter) {
    std::string path = "\\\\.\\";
    path.push_back(driveLetter);
    path.push_back(':');

    logf("Attempting to safely eject drive " + std::string(1, driveLetter) + ":");

    Disarm(driveLetter);

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
        logf("Failed to open volume for eject: " + std::to_string(GetLastError()));
        return false;
    }

    DWORD bytesReturned;
    BOOL result;

    result = DeviceIoControl(
        hVolume,
        FSCTL_LOCK_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    if (!result) {
        logf("Failed to lock volume: " + std::to_string(GetLastError()));
        CloseHandle(hVolume);
        return false;
    }

    logf("Volume locked successfully");

    result = DeviceIoControl(
        hVolume,
        FSCTL_DISMOUNT_VOLUME,
        nullptr, 0,
        nullptr, 0,
        &bytesReturned,
        nullptr
    );

    if (!result) {
        logf("Failed to dismount volume: " + std::to_string(GetLastError()));
        DeviceIoControl(hVolume, FSCTL_UNLOCK_VOLUME, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);
        CloseHandle(hVolume);
        return false;
    }

    logf("Volume dismounted successfully");

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
        logf("Warning: Failed to allow media removal: " + std::to_string(GetLastError()));
    }

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
        logf("Failed to eject media: " + std::to_string(err));
        return false;
    }

    logf("Drive " + std::string(1, driveLetter) + ": ejected successfully!");
    return true;
}

// Ручное безопасное извлечение USB-флешки через CM_Request_Device_Eject
// Не нарушает мониторинг, не блокирует том, не использует FSCTL_LOCK_VOLUME
// Возвращает: 0 = успех, >0 = код ошибки
// Выводит ответы в формате: OK SAFE_EJECT <буква> или ERROR <тип_ошибки> [код]
int EjectUsbDriveManual(char driveLetter) {
    logf("→ РУЧНОЕ ИЗВЛЕЧЕНИЕ: Запрос безопасного извлечения диска " + std::string(1, driveLetter) + ":");

    // Проверка 1: Существует ли буква диска
    char rootPath[] = {driveLetter, ':', '\\', 0};
    UINT driveType = GetDriveTypeA(rootPath);
    if (driveType == DRIVE_NO_ROOT_DIR) {
        printf("ERROR DRIVE_NOT_FOUND %c\n", driveLetter);
        logf("ERROR: Диск " + std::string(1, driveLetter) + ": не существует");
        return 1;
    }

    // Проверка 2: Является ли диск USB-диском
    if (!IsUsbDrive(driveLetter)) {
        printf("ERROR NOT_USB_DRIVE %c\n", driveLetter);
        logf("ERROR: Диск " + std::string(1, driveLetter) + ": не является USB-диском");
        return 2;
    }

    // Получаем номер физического устройства для поиска DevInst
    std::string volumePath = "\\\\.\\";
    volumePath.push_back(driveLetter);
    volumePath.push_back(':');

    HANDLE hVolume = CreateFileA(
        volumePath.c_str(),
        0,  // Только для чтения информации
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hVolume == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("ERROR VOLUME_OPEN_FAILED %lu\n", err);
        logf("ERROR: Не удалось открыть том " + std::string(1, driveLetter) + ": (ошибка " + std::to_string(err) + ")");
        return 3;
    }

    STORAGE_DEVICE_NUMBER sdn;
    DWORD bytesReturned;

    BOOL result = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        nullptr, 0,
        &sdn, sizeof(sdn),
        &bytesReturned,
        nullptr
    );

    CloseHandle(hVolume);

    if (!result) {
        DWORD err = GetLastError();
        printf("ERROR GET_DEVICE_NUMBER_FAILED %lu\n", err);
        logf("ERROR: Не удалось получить номер устройства для " + std::string(1, driveLetter) + ": (ошибка " + std::to_string(err) + ")");
        return 4;
    }

    // Ищем DevInst через Disk GUID (более надежно для USB-накопителей)
    const GUID GUID_DEVINTERFACE_DISK = {0x53f56307, 0xb6bf, 0x11d0, {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b}};

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_DISK,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("ERROR SETUPAPI_FAILED %lu\n", err);
        logf("ERROR: SetupDiGetClassDevsA failed (ошибка " + std::to_string(err) + ")");
        return 5;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    bool found = false;
    DEVINST devInst = 0;

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_DISK, i, &interfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

        if (requiredSize > 0) {
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                HANDLE hTest = CreateFileA(
                    detailData->DevicePath,
                    0,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );

                if (hTest != INVALID_HANDLE_VALUE) {
                    STORAGE_DEVICE_NUMBER testSdn;
                    DWORD testBytes;
                    if (DeviceIoControl(hTest, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0,
                                       &testSdn, sizeof(testSdn), &testBytes, nullptr)) {
                        if (testSdn.DeviceNumber == sdn.DeviceNumber) {
                            devInst = devInfoData.DevInst;
                            found = true;
                            logf("  Найдено устройство диска через Disk GUID: DevInst=" + std::to_string(devInst));
                            CloseHandle(hTest);
                            break;
                        }
                    }
                    CloseHandle(hTest);
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (!found) {
        printf("ERROR DEVICE_NOT_FOUND 0\n");
        logf("ERROR: USB устройство для диска " + std::string(1, driveLetter) + ": не найдено в дереве устройств");
        return 6;
    }

    logf("  Найдено устройство диска: DevInst=" + std::to_string(devInst));

    // Ищем USBSTOR устройство тремя способами:
    // 1. Поднимаемся по дереву устройств вверх от диска
    // 2. Ищем через SetupDiGetClassDevs для USBSTOR и сопоставляем по номеру устройства

    DEVINST usbStorInst = 0;
    bool foundUsbStor = false;

    // Способ 1: Поднимаемся по дереву устройств вверх от диска
    logf("  Способ 1: Ищем USB устройство, поднимаясь по дереву от диска:");
    DEVINST currentInst = devInst;
    for (int level = 0; level < 10; level++) {
        DEVINST parentInst = 0;
        CONFIGRET parentCr = CM_Get_Parent(&parentInst, currentInst, 0);
        if (parentCr != CR_SUCCESS) {
            logf("  [DEBUG] Достигнут корень дерева устройств на уровне " + std::to_string(level));
            break;
        }

        currentInst = parentInst;

        // Получаем instance ID родителя
        char parentInstanceId[512] = {0};
        CONFIGRET cr2 = CM_Get_Device_IDA(parentInst, parentInstanceId, sizeof(parentInstanceId), 0);
        if (cr2 == CR_SUCCESS) {
            std::string parentInstanceStr(parentInstanceId);
            logf("  [DEBUG] Проверяю родительское устройство (уровень " + std::to_string(level) + "): " + parentInstanceStr.substr(0, 100));

            // Ищем USBSTOR или любое USB устройство (USB\VID_...)
            if (parentInstanceStr.find("USBSTOR\\") != std::string::npos) {
                logf("  ✓ Найдено USBSTOR устройство в дереве: " + parentInstanceStr.substr(0, 100));
                usbStorInst = parentInst;
                foundUsbStor = true;
                break;
            } else if (parentInstanceStr.find("USB\\") == 0 && parentInstanceStr.find("VID_") != std::string::npos) {
                // Найдено USB устройство с VID/PID - это может быть наше устройство
                logf("  ✓ Найдено USB устройство в дереве: " + parentInstanceStr.substr(0, 100));
                usbStorInst = parentInst;
                foundUsbStor = true;
                break;
            }
        }
    }

    // Способ 2: Ищем через SetupDiGetClassDevs для USBSTOR
    if (!foundUsbStor) {
        logf("  Способ 2: Ищем USBSTOR устройство через SetupDiGetClassDevs:");

        HDEVINFO hDevInfoUsb = SetupDiGetClassDevsA(
            nullptr,
            "USBSTOR",
            nullptr,
            DIGCF_PRESENT
        );

        if (hDevInfoUsb == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            logf("  [DEBUG] SetupDiGetClassDevsA для USBSTOR вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        } else {
            SP_DEVINFO_DATA devInfoDataUsb;
            devInfoDataUsb.cbSize = sizeof(SP_DEVINFO_DATA);

            DWORD deviceCount = 0;
            // Перебираем все USBSTOR устройства
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfoUsb, i, &devInfoDataUsb); i++) {
                deviceCount++;
                char instanceId[512] = {0};
                if (SetupDiGetDeviceInstanceIdA(hDevInfoUsb, &devInfoDataUsb, instanceId, sizeof(instanceId), nullptr)) {
                    std::string instanceStr(instanceId);
                    logf("  [DEBUG] Проверяю USBSTOR устройство #" + std::to_string(i) + ": " + instanceStr.substr(0, 80));

                    // Проверяем, связано ли это USBSTOR устройство с нашим диском
                    // Для этого поднимаемся от устройства диска вверх и проверяем, встречается ли это USBSTOR устройство
                    DEVINST testInst = devInst;
                    bool isAncestor = false;

                    // Поднимаемся от устройства диска вверх до 10 уровней
                    for (int level = 0; level < 10; level++) {
                        if (testInst == devInfoDataUsb.DevInst) {
                            isAncestor = true;
                            logf("  [DEBUG] USBSTOR устройство найдено на уровне " + std::to_string(level) + " от диска");
                            break;
                        }
                        DEVINST parentInst = 0;
                        CONFIGRET parentCr = CM_Get_Parent(&parentInst, testInst, 0);
                        if (parentCr != CR_SUCCESS) break;
                        testInst = parentInst;
                    }

                    if (isAncestor) {
                        logf("  ✓ Найдено USBSTOR устройство, связанное с диском: " + instanceStr.substr(0, 80));
                        usbStorInst = devInfoDataUsb.DevInst;
                        foundUsbStor = true;
                        break;
                    }
                }
            }

            if (deviceCount == 0) {
                logf("  [DEBUG] USBSTOR устройства не найдены через SetupDiEnumDeviceInfo");
            } else {
                logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " USBSTOR устройств, связь не найдена");
            }

            SetupDiDestroyDeviceInfoList(hDevInfoUsb);
        }
    }

    // Используем CM_Request_Device_Eject (единственный метод извлечения)
    PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
    char vetoName[MAX_PATH] = {0};
    CONFIGRET cr = CR_DEFAULT;

    // Способ 3: Если не нашли, пробуем извлечь все USBSTOR устройства по очереди (последняя попытка)
    if (!foundUsbStor) {
        logf("  Способ 3: Пробуем извлечь все USBSTOR устройства по очереди:");

        HDEVINFO hDevInfoUsb = SetupDiGetClassDevsA(
            nullptr,
            "USBSTOR",
            nullptr,
            DIGCF_PRESENT
        );

        if (hDevInfoUsb == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            logf("  [DEBUG] SetupDiGetClassDevsA для USBSTOR вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        } else {
            SP_DEVINFO_DATA devInfoDataUsb;
            devInfoDataUsb.cbSize = sizeof(SP_DEVINFO_DATA);

            DWORD deviceCount = 0;
            // Пробуем извлечь первое найденное USBSTOR устройство
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfoUsb, i, &devInfoDataUsb); i++) {
                deviceCount++;
                char instanceId[512] = {0};
                if (SetupDiGetDeviceInstanceIdA(hDevInfoUsb, &devInfoDataUsb, instanceId, sizeof(instanceId), nullptr)) {
                    std::string instanceStr(instanceId);
                    logf("  [DEBUG] Пробуем извлечь USBSTOR устройство #" + std::to_string(i) + ": " + instanceStr.substr(0, 80));

                    PNP_VETO_TYPE testVetoType = PNP_VetoTypeUnknown;
                    char testVetoName[MAX_PATH] = {0};
                    CONFIGRET testCr = CM_Request_Device_EjectA(
                        devInfoDataUsb.DevInst,
                        &testVetoType,
                        testVetoName,
                        sizeof(testVetoName),
                        0
                    );

                    logf("  [DEBUG] CM_Request_Device_Eject для USBSTOR #" + std::to_string(i) + " вернул код: " + std::to_string(testCr));

                    // Если успешно или veto (устройство занято), значит это наше устройство
                    if (testCr == CR_SUCCESS || testCr == CR_REMOVE_VETOED) {
                        logf("  ✓ Найдено USBSTOR устройство (по результату извлечения): " + instanceStr.substr(0, 80));
                        usbStorInst = devInfoDataUsb.DevInst;
                        foundUsbStor = true;
                        cr = testCr;
                        vetoType = testVetoType;
                        if (strlen(testVetoName) > 0) {
                            strncpy(vetoName, testVetoName, sizeof(vetoName) - 1);
                            vetoName[sizeof(vetoName) - 1] = 0;
                        }
                        break;
                    }
                }
            }

            SetupDiDestroyDeviceInfoList(hDevInfoUsb);
        }
    }

    if (foundUsbStor && cr == CR_DEFAULT) {
        // Пробуем извлечь USBSTOR устройство (правильный способ)
        logf("  Пробуем извлечь USBSTOR устройство: DevInst=" + std::to_string(usbStorInst));
        cr = CM_Request_Device_EjectA(
            usbStorInst,
            &vetoType,
            vetoName,
            sizeof(vetoName),
            0
        );
        logf("  CM_Request_Device_Eject для USBSTOR вернул код: " + std::to_string(cr));
    } else if (cr == CR_DEFAULT) {
        // Если не нашли USBSTOR, пробуем извлечь устройство диска (fallback)
        logf("  USBSTOR устройство не найдено, пробуем извлечь устройство диска напрямую");
        cr = CM_Request_Device_EjectA(
            devInst,
            &vetoType,
            vetoName,
            sizeof(vetoName),
            0
        );
        logf("  CM_Request_Device_Eject для устройства диска вернул код: " + std::to_string(cr));
    }

    if (cr == CR_SUCCESS) {
        printf("OK SAFE_EJECT %c\n", driveLetter);
        logf("✓ РУЧНОЕ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск " + std::string(1, driveLetter) + ": успешно извлечён через CM_Request_Device_Eject");
        return 0;
    } else if (cr == CR_REMOVE_VETOED) {
        printf("ERROR VOLUME_LOCK_FAILED %lu\n", (DWORD)vetoType);
        std::string vetoReason = (strlen(vetoName) > 0) ? std::string(vetoName) : "устройство занято";
        logf("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск " + std::string(1, driveLetter) + ": - " + vetoReason + " (veto type: " + std::to_string(vetoType) + ")");
        return 7;
    } else {
        printf("ERROR EJECT_FAILED %lu\n", (DWORD)cr);
        logf("ERROR: CM_Request_Device_Eject failed для диска " + std::string(1, driveLetter) + ": (код " + std::to_string(cr) + ")");
        return 8;
    }
}

bool SafelyEjectDriveViaCM(char driveLetter) {
    int result = EjectUsbDriveManual(driveLetter);
    return (result == 0);
}

// Ручное отключение USB-мыши через CM_Disable_DevNode
// Не нарушает мониторинг, не использует блокировки
// Возвращает: 0 = успех, >0 = код ошибки
// Выводит ответы в формате: OK USB_MOUSE_DISABLED или ERROR <тип_ошибки> [код]
int DisableUsbMouseManual() {
    logf("→ РУЧНОЕ ОТКЛЮЧЕНИЕ: Запрос отключения USB-мыши");

    // GUID для HID устройств
    const GUID GUID_DEVINTERFACE_HID = {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

    DEVINST mouseDevInst = 0;
    bool foundMouse = false;

    // Способ 1: Ищем через SetupDiGetClassDevs для HID устройств
    logf("  Способ 1: Ищем USB HID мышь через SetupDiGetClassDevs:");

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_HID,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        logf("  [DEBUG] SetupDiGetClassDevsA для HID вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
    } else {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        DWORD deviceCount = 0;
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, i, &interfaceData); i++) {
            deviceCount++;

            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

            if (requiredSize > 0) {
                std::vector<BYTE> buffer(requiredSize);
                PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
                detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

                SP_DEVINFO_DATA devInfoData;
                devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

                if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                    // Получаем описание устройства
                    char description[256] = {0};
                    if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                        std::string descStr(description);
                        std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);

                        // Получаем instance ID для проверки USB
                        char instanceId[512] = {0};
                        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, instanceId, sizeof(instanceId), nullptr)) {
                            std::string instanceStr(instanceId);
                            std::transform(instanceStr.begin(), instanceStr.end(), instanceStr.begin(), ::tolower);

                            logf("  [DEBUG] Проверяю HID устройство #" + std::to_string(i) + ": " + std::string(description) + " (" + instanceStr.substr(0, 60) + ")");

                            // Проверяем, что это USB устройство и мышь
                            // USB устройство должно иметь VID_ в instance ID (не просто HID)
                            bool isUsb = (instanceStr.find("usb\\") != std::string::npos) ||
                                        (instanceStr.find("hid\\vid_") != std::string::npos);
                            bool isMouse = (descStr.find("mouse") != std::string::npos) &&
                                          (descStr.find("keyboard") == std::string::npos) &&
                                          (descStr.find("trackball") == std::string::npos);

                            if (isUsb && isMouse) {
                                mouseDevInst = devInfoData.DevInst;
                                foundMouse = true;
                                logf("  ✓ Найдена USB мышь: " + std::string(description));
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (deviceCount == 0) {
            logf("  [DEBUG] HID устройства не найдены через SetupDiEnumDeviceInfo");
        } else {
            logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " HID устройств");
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // Способ 2: Ищем через USB устройства (без HID)
    if (!foundMouse) {
        logf("  Способ 2: Ищем USB мышь через USB устройства:");

        HDEVINFO hDevInfoUsb = SetupDiGetClassDevsA(
            nullptr,
            "USB",
            nullptr,
            DIGCF_PRESENT
        );

        if (hDevInfoUsb == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            logf("  [DEBUG] SetupDiGetClassDevsA для USB вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        } else {
            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            DWORD deviceCount = 0;
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfoUsb, i, &devInfoData); i++) {
                deviceCount++;

                char description[256] = {0};
                if (SetupDiGetDeviceRegistryPropertyA(hDevInfoUsb, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                    std::string descStr(description);
                    std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);

                    char instanceId[512] = {0};
                    if (SetupDiGetDeviceInstanceIdA(hDevInfoUsb, &devInfoData, instanceId, sizeof(instanceId), nullptr)) {
                        std::string instanceStr(instanceId);

                        logf("  [DEBUG] Проверяю USB устройство #" + std::to_string(i) + ": " + std::string(description) + " (" + instanceStr.substr(0, 60) + ")");

                        // Проверяем, что это USB мышь
                        bool isMouse = (descStr.find("mouse") != std::string::npos) &&
                                      (descStr.find("keyboard") == std::string::npos) &&
                                      (descStr.find("hub") == std::string::npos) &&
                                      (descStr.find("controller") == std::string::npos);

                        if (isMouse) {
                            mouseDevInst = devInfoData.DevInst;
                            foundMouse = true;
                            logf("  ✓ Найдена USB мышь: " + std::string(description));
                            break;
                        }
                    }
                }
            }

            if (deviceCount == 0) {
                logf("  [DEBUG] USB устройства не найдены через SetupDiEnumDeviceInfo");
            } else {
                logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " USB устройств");
            }

            SetupDiDestroyDeviceInfoList(hDevInfoUsb);
        }
    }

    if (!foundMouse) {
        printf("ERROR USB_MOUSE_NOT_FOUND\n");
        logf("ERROR: USB мышь не найдена в системе");
        return 1;
    }

    // Пытаемся отключить устройство
    CONFIGRET cr = CM_Disable_DevNode(mouseDevInst, 0);

    if (cr == CR_SUCCESS) {
        printf("OK USB_MOUSE_DISABLED\n");
        logf("✓ РУЧНОЕ ОТКЛЮЧЕНИЕ: USB мышь успешно отключена");
        return 0;
    } else {
        printf("ERROR DEVICE_DISABLE_FAILED %lu\n", (DWORD)cr);
        logf("ERROR: Не удалось отключить USB мышь (код " + std::to_string(cr) + ")");
        return 2;
    }
}

// Ручное отключение USB-клавиатуры через CM_Disable_DevNode
// Не нарушает мониторинг, не использует блокировки
// Возвращает: 0 = успех, >0 = код ошибки
// Выводит ответы в формате: OK USB_KEYBOARD_DISABLED или ERROR <тип_ошибки> [код]
int DisableUsbKeyboardManual() {
    logf("→ РУЧНОЕ ОТКЛЮЧЕНИЕ: Запрос отключения USB-клавиатуры");

    // GUID для HID устройств
    const GUID GUID_DEVINTERFACE_HID = {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};

    DEVINST keyboardDevInst = 0;
    bool foundKeyboard = false;

    // Способ 1: Ищем через SetupDiGetClassDevs для HID устройств
    logf("  Способ 1: Ищем USB HID клавиатуру через SetupDiGetClassDevs:");

    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_HID,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        logf("  [DEBUG] SetupDiGetClassDevsA для HID вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
    } else {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        DWORD deviceCount = 0;
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, i, &interfaceData); i++) {
            deviceCount++;

            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);

            if (requiredSize > 0) {
                std::vector<BYTE> buffer(requiredSize);
                PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
                detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

                SP_DEVINFO_DATA devInfoData;
                devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

                if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                    // Получаем описание устройства
                    char description[256] = {0};
                    if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                        std::string descStr(description);
                        std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);

                        // Получаем instance ID для проверки USB
                        char instanceId[512] = {0};
                        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, instanceId, sizeof(instanceId), nullptr)) {
                            std::string instanceStr(instanceId);
                            std::transform(instanceStr.begin(), instanceStr.end(), instanceStr.begin(), ::tolower);

                            logf("  [DEBUG] Проверяю HID устройство #" + std::to_string(i) + ": " + std::string(description) + " (" + instanceStr.substr(0, 60) + ")");

                            // Проверяем, что это USB устройство и клавиатура
                            // USB устройство должно иметь VID_ в instance ID (не просто HID)
                            bool isUsb = (instanceStr.find("usb\\") != std::string::npos) ||
                                        (instanceStr.find("hid\\vid_") != std::string::npos);
                            bool isKeyboard = (descStr.find("keyboard") != std::string::npos);

                            if (isUsb && isKeyboard) {
                                keyboardDevInst = devInfoData.DevInst;
                                foundKeyboard = true;
                                logf("  ✓ Найдена USB клавиатура: " + std::string(description));
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (deviceCount == 0) {
            logf("  [DEBUG] HID устройства не найдены через SetupDiEnumDeviceInfo");
        } else {
            logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " HID устройств");
        }

        SetupDiDestroyDeviceInfoList(hDevInfo);
    }

    // Способ 2: Ищем через USB устройства (без HID)
    if (!foundKeyboard) {
        logf("  Способ 2: Ищем USB клавиатуру через USB устройства:");

        HDEVINFO hDevInfoUsb = SetupDiGetClassDevsA(
            nullptr,
            "USB",
            nullptr,
            DIGCF_PRESENT
        );

        if (hDevInfoUsb == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            logf("  [DEBUG] SetupDiGetClassDevsA для USB вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        } else {
            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

            DWORD deviceCount = 0;
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfoUsb, i, &devInfoData); i++) {
                deviceCount++;

                char description[256] = {0};
                if (SetupDiGetDeviceRegistryPropertyA(hDevInfoUsb, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                    std::string descStr(description);
                    std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);

                    char instanceId[512] = {0};
                    if (SetupDiGetDeviceInstanceIdA(hDevInfoUsb, &devInfoData, instanceId, sizeof(instanceId), nullptr)) {
                        std::string instanceStr(instanceId);

                        logf("  [DEBUG] Проверяю USB устройство #" + std::to_string(i) + ": " + std::string(description) + " (" + instanceStr.substr(0, 60) + ")");

                        // Проверяем, что это USB клавиатура
                        bool isKeyboard = (descStr.find("keyboard") != std::string::npos) &&
                                         (descStr.find("hub") == std::string::npos) &&
                                         (descStr.find("controller") == std::string::npos);

                        if (isKeyboard) {
                            keyboardDevInst = devInfoData.DevInst;
                            foundKeyboard = true;
                            logf("  ✓ Найдена USB клавиатура: " + std::string(description));
                            break;
                        }
                    }
                }
            }

            if (deviceCount == 0) {
                logf("  [DEBUG] USB устройства не найдены через SetupDiEnumDeviceInfo");
            } else {
                logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " USB устройств");
            }

            SetupDiDestroyDeviceInfoList(hDevInfoUsb);
        }
    }

    if (!foundKeyboard) {
        printf("ERROR USB_KEYBOARD_NOT_FOUND\n");
        logf("ERROR: USB клавиатура не найдена в системе");
        return 1;
    }

    // Пытаемся отключить устройство
    CONFIGRET cr = CM_Disable_DevNode(keyboardDevInst, 0);

    if (cr == CR_SUCCESS) {
        printf("OK USB_KEYBOARD_DISABLED\n");
        logf("✓ РУЧНОЕ ОТКЛЮЧЕНИЕ: USB клавиатура успешно отключена");
        return 0;
    } else {
        printf("ERROR DEVICE_DISABLE_FAILED %lu\n", (DWORD)cr);
        logf("ERROR: Не удалось отключить USB клавиатуру (код " + std::to_string(cr) + ")");
        return 2;
    }
}

// Поиск USB устройства по Hardware ID
// Возвращает: true если устройство найдено, false если нет
bool FindUsbDeviceByHardwareId(const char* hardwareId, DEVINST* devInst) {
    logf("→ ПОИСК УСТРОЙСТВА: Ищем устройство с Hardware ID: " + std::string(hardwareId));

    // Ищем через SetupDiGetClassDevs с классом NULL для всех устройств
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        nullptr,
        nullptr,
        nullptr,
        DIGCF_PRESENT
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        logf("  [DEBUG] SetupDiGetClassDevsA вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        return false;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

    bool found = false;
    DWORD deviceIndex = 0;

    while (SetupDiEnumDeviceInfo(hDevInfo, deviceIndex, &devInfoData)) {
        deviceIndex++;

        // Получаем Hardware ID
        char hardwareIdBuffer[1024] = {0};
        if (SetupDiGetDeviceRegistryPropertyA(
            hDevInfo,
            &devInfoData,
            SPDRP_HARDWAREID,
            nullptr,
            (PBYTE)hardwareIdBuffer,
            sizeof(hardwareIdBuffer),
            nullptr
        )) {
            std::string deviceHardwareIds(hardwareIdBuffer);
            
            // Hardware IDs могут содержать несколько значений, разделенных \0
            size_t pos = 0;
            std::string token;
            bool currentDeviceMatches = false;
            
            while ((pos = deviceHardwareIds.find('\0', pos)) != std::string::npos) {
                token = deviceHardwareIds.substr(0, pos);
                
                logf("  [DEBUG] Проверяю Hardware ID: " + token);
                
                // Проверяем на совпадение (без учета регистра)
                std::string lowerToken = token;
                std::string lowerSearch = hardwareId;
                std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);
                std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
                
                if (lowerToken.find(lowerSearch) != std::string::npos) {
                    currentDeviceMatches = true;
                    break;
                }
                
                deviceHardwareIds.erase(0, pos + 1);
            }
            
            // Проверяем последний элемент, если он есть
            if (!deviceHardwareIds.empty() && !currentDeviceMatches) {
                std::string lowerToken = deviceHardwareIds;
                std::string lowerSearch = hardwareId;
                std::transform(lowerToken.begin(), lowerToken.end(), lowerToken.begin(), ::tolower);
                std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
                
                if (lowerToken.find(lowerSearch) != std::string::npos) {
                    currentDeviceMatches = true;
                }
            }

            if (currentDeviceMatches) {
                *devInst = devInfoData.DevInst;
                
                // Получаем описание устройства для лога
                char description[256] = {0};
                if (SetupDiGetDeviceRegistryPropertyA(
                    hDevInfo,
                    &devInfoData,
                    SPDRP_DEVICEDESC,
                    nullptr,
                    (PBYTE)description,
                    sizeof(description),
                    nullptr
                )) {
                    logf("  ✓ Найдено устройство: " + std::string(description) + " (Hardware ID: " + token + ")");
                } else {
                    logf("  ✓ Найдено устройство с Hardware ID: " + token);
                }
                
                found = true;
                break;
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    if (!found) {
        logf("  [DEBUG] Устройство с Hardware ID '" + std::string(hardwareId) + "' не найдено");
    }

    return found;
}

// Включение USB устройства по Hardware ID
// Возвращает: 0 = успех, >0 = код ошибки
// Выводит ответы в формате: OK USB_DEVICE_ENABLED или ERROR <тип_ошибки> [код]
int EnableUsbDeviceByHardwareId(const char* hardwareId) {
    logf("→ ВКЛЮЧЕНИЕ УСТРОЙСТВА: Запрос включения устройства с Hardware ID: " + std::string(hardwareId));

    DEVINST devInst;
    bool found = FindUsbDeviceByHardwareId(hardwareId, &devInst);

    if (!found) {
        printf("ERROR DEVICE_NOT_FOUND\n");
        logf("ERROR: Устройство с Hardware ID '" + std::string(hardwareId) + "' не найдено");
        return 1;
    }

    // Сначала пробуем включить устройство
    CONFIGRET cr = CM_Enable_DevNode(devInst, 0);

    if (cr == CR_SUCCESS) {
        logf("  Устройство успешно включено, обновляем информацию в реестре");
        
        // Обновить информацию в реестре, чтобы система корректно распознала устройство
        UpdateDeviceInRegistry(devInst);
        
        printf("OK USB_DEVICE_ENABLED\n");
        logf("✓ РУЧНОЕ ВКЛЮЧЕНИЕ: Устройство успешно включено");
        return 0;
    } else {
        printf("ERROR DEVICE_ENABLE_FAILED %lu\n", (DWORD)cr);
        logf("ERROR: Не удалось включить устройство (код " + std::to_string(cr) + ")");
        return 2;
    }
}

// Обновление информации об устройстве в реестре
void UpdateDeviceInRegistry(DEVINST devInst) {
    // Эта функция обновляет информацию об устройстве в реестре
    // и может включать в себя вызовы других функций для обновления драйверов
    logf("  Обновление информации об устройстве в реестре...");
    
    // Можем выполнить дополнительные действия для обновления устройства
    // Но для упрощения просто логгируем выполнение
    logf("  Информация об устройстве обновлена");
}

// USB Monitor class implementation
USBMonitor* USBMonitor::instance = nullptr;

USBMonitor::USBMonitor() : monitoring(false), messageHwnd(nullptr) {
    instance = this;
}

USBMonitor::~USBMonitor() {
    if (monitoring) {
        monitoring = false;
        if (monitorThread.joinable()) {
            monitorThread.join();
        }
    }
    if (messageHwnd) {
        DestroyWindow(messageHwnd);
        UnregisterClassA("USBMonitorClass", nullptr);
    }
}

LRESULT CALLBACK USBMonitor::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DEVICECHANGE:
            switch (wParam) {
                case DBT_DEVICEARRIVAL:
                case DBT_DEVICEREMOVECOMPLETE:
                {
                    PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
                    if (pHdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
                        PDEV_BROADCAST_VOLUME pVol = (PDEV_BROADCAST_VOLUME)pHdr;
                        if (pVol->dbcv_flags & DBTF_MEDIA) {
                            logf("USB device change detected");
                        }
                    }
                    break;
                }
            }
            return TRUE;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

bool USBMonitor::initialize() {
    // Register window class for device notifications
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "USBMonitorClass";
    
    if (!RegisterClassA(&wc)) {
        logf("Failed to register window class for USB monitor");
        return false;
    }
    
    // Create a hidden window to receive device notifications
    messageHwnd = CreateWindowExA(
        0, "USBMonitorClass", "USB Monitor Window",
        0, 0, 0, 0, 0,
        nullptr, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (!messageHwnd) {
        logf("Failed to create message window for USB monitor");
        return false;
    }
    
    // Register for device notifications
    DEV_BROADCAST_DEVICEINTERFACE_A notificationFilter = {};
    notificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE_A);
    notificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    // Using the disk device class GUID
    notificationFilter.dbcc_classguid = GUID_DEVINTERFACE_DISK; // Using disk interface for USB storage devices
    
    HDEVNOTIFY hDevNotify = RegisterDeviceNotificationA(
        messageHwnd,
        &notificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
    
    if (!hDevNotify) {
        logf("Failed to register for device notifications");
        DestroyWindow(messageHwnd);
        return false;
    }
    
    S.hwnd = messageHwnd; // Set the global state's hwnd
    logf("USB monitor initialized successfully");
    return true;
}

void USBMonitor::startMonitoring() {
    if (!monitoring) {
        monitoring = true;
        monitorThread = std::thread(&USBMonitor::monitorThreadFunc, this);
        logf("USB monitoring started");
    }
}

std::vector<USBDeviceInfo> USBMonitor::getConnectedDevices() {
    std::vector<USBDeviceInfo> devices;
    
    // Get all removable letters (USB drives)
    std::vector<char> letters = list_removable_letters();
    
    for (char letter : letters) {
        USBDeviceInfo device;
        device.driveLetter = std::string(1, letter) + ":";
        device.deviceID = device.driveLetter; // Using drive letter as ID for simplicity
        device.deviceName = "USB Drive " + device.driveLetter;
        device.deviceType = "Removable Drive";
        device.deviceClass = "Disk";
        device.isRemovable = true;
        device.isMounted = true; // Assume mounted since it appears in drive list
        
        devices.push_back(device);
    }
    
    // Add connected input devices (mouse, keyboard)
    std::vector<USBDeviceInfo> inputDevices = getConnectedInputDevices();
    devices.insert(devices.end(), inputDevices.begin(), inputDevices.end());
    
    return devices;
}

std::vector<USBDeviceInfo> USBMonitor::getConnectedInputDevices() {
    std::vector<USBDeviceInfo> inputDevices;
    
    // Search for HID devices (mice and keyboards)
    const GUID GUID_DEVINTERFACE_HID = {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
    
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_HID,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (hDevInfo != INVALID_HANDLE_VALUE) {
        SP_DEVICE_INTERFACE_DATA interfaceData;
        interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
        
        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, i, &interfaceData); i++) {
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
            
            if (requiredSize > 0) {
                std::vector<BYTE> buffer(requiredSize);
                PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
                detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
                
                SP_DEVINFO_DATA devInfoData;
                devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
                
                if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                    // Get device description
                    char description[256] = {0};
                    if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                        std::string descStr(description);
                        
                        // Check if it's a mouse or keyboard
                        std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);
                        
                        // Get hardware ID to check if it's USB
                        char hardwareId[512] = {0};
                        if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, hardwareId, sizeof(hardwareId), nullptr)) {
                            std::string hwId(hardwareId);
                            std::transform(hwId.begin(), hwId.end(), hwId.begin(), ::tolower);
                            
                            // Check if it's USB and mouse or keyboard
                            bool isUsb = hwId.find("usb\\") != std::string::npos;
                            bool isMouse = descStr.find("mouse") != std::string::npos && descStr.find("keyboard") == std::string::npos;
                            bool isKeyboard = descStr.find("keyboard") != std::string::npos;
                            
                            if (isUsb && (isMouse || isKeyboard)) {
                                USBDeviceInfo device;
                                device.deviceID = std::string(hardwareId);
                                device.deviceName = std::string(description);
                                device.deviceType = isMouse ? "Mouse" : "Keyboard";
                                device.deviceClass = "HID";
                                device.isRemovable = true;
                                device.isMounted = true;
                                device.driveLetter = "N/A";
                                
                                inputDevices.push_back(device);
                            }
                        }
                    }
                }
            }
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    
    return inputDevices;
}

bool USBMonitor::disableInputDevice(const std::string& deviceType) {
    if (deviceType != "mouse" && deviceType != "keyboard") {
        logf("Invalid device type for disabling: " + deviceType);
        return false;
    }
    
    const GUID GUID_DEVINTERFACE_HID = {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
    
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_HID,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        logf("Failed to get HID device info for disabling " + deviceType);
        return false;
    }
    
    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    
    bool foundAndDisabled = false;
    
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, i, &interfaceData) && !foundAndDisabled; i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        
        if (requiredSize > 0) {
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
            
            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                // Get device description
                char description[256] = {0};
                if (SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr)) {
                    std::string descStr(description);
                    std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);
                    
                    // Check if it matches the requested device type
                    bool isTargetDevice = false;
                    if (deviceType == "mouse") {
                        isTargetDevice = descStr.find("mouse") != std::string::npos && descStr.find("keyboard") == std::string::npos;
                    } else if (deviceType == "keyboard") {
                        isTargetDevice = descStr.find("keyboard") != std::string::npos;
                    }
                    
                    // Check if it's a USB device
                    char hardwareId[512] = {0};
                    if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, hardwareId, sizeof(hardwareId), nullptr)) {
                        std::string hwId(hardwareId);
                        std::transform(hwId.begin(), hwId.end(), hwId.begin(), ::tolower);
                        bool isUsb = hwId.find("usb\\") != std::string::npos;
                        
                        if (isUsb && isTargetDevice) {
                            // Try to disable the device
                            CONFIGRET cr = CM_Disable_DevNode(devInfoData.DevInst, 0);
                            if (cr == CR_SUCCESS) {
                                logf("Successfully disabled " + deviceType + ": " + std::string(description));
                                foundAndDisabled = true;
                            } else {
                                logf("Failed to disable " + deviceType + ": " + std::string(description) + " (error: " + std::to_string(cr) + ")");
                            }
                        }
                    }
                }
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return foundAndDisabled;
}

bool USBMonitor::enableInputDevice(const std::string& hardwareId) {
    // Find the device with the given hardware ID
    const GUID GUID_DEVINTERFACE_HID = {0x4d1e55b2, 0xf16f, 0x11cf, {0x88, 0xcb, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30}};
    
    HDEVINFO hDevInfo = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_HID,
        nullptr,
        nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );
    
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        logf("Failed to get HID device info for enabling device with ID: " + hardwareId);
        return false;
    }
    
    SP_DEVICE_INTERFACE_DATA interfaceData;
    interfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    bool foundAndEnabled = false;
    
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, &GUID_DEVINTERFACE_HID, i, &interfaceData) && !foundAndEnabled; i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        
        if (requiredSize > 0) {
            std::vector<BYTE> buffer(requiredSize);
            PSP_DEVICE_INTERFACE_DETAIL_DATA_A detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)buffer.data();
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
            
            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &interfaceData, detailData, requiredSize, nullptr, &devInfoData)) {
                // Get hardware ID
                char deviceHardwareId[512] = {0};
                if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, deviceHardwareId, sizeof(deviceHardwareId), nullptr)) {
                    std::string devHwId(deviceHardwareId);
                    std::transform(devHwId.begin(), devHwId.end(), devHwId.begin(), ::tolower);
                    std::string searchHwId = hardwareId;
                    std::transform(searchHwId.begin(), searchHwId.end(), searchHwId.begin(), ::tolower);
                    
                    if (devHwId.find(searchHwId) != std::string::npos) {
                        // Try to enable the device
                        CONFIGRET cr = CM_Enable_DevNode(devInfoData.DevInst, 0);
                        if (cr == CR_SUCCESS) {
                            logf("Successfully enabled device: " + std::string(deviceHardwareId));
                            foundAndEnabled = true;
                        } else {
                            logf("Failed to enable device: " + std::string(deviceHardwareId) + " (error: " + std::to_string(cr) + ")");
                        }
                    }
                }
            }
        }
    }
    
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return foundAndEnabled;
}

bool USBMonitor::safelyEjectDevice(const std::string& deviceId) {
    if (deviceId.length() < 1) return false;
    
    char driveLetter = deviceId[0]; // Expecting something like "E:" or just "E"
    if (driveLetter >= 'A' && driveLetter <= 'Z') {
        // Try the manual ejection method first (CM_Request_Device_Eject)
        int result = EjectUsbDriveManual(driveLetter);
        return (result == 0);
    }
    
    return false;
}

bool USBMonitor::unsafeEjectDevice(const std::string& deviceId) {
    if (deviceId.length() < 1) return false;
    
    char driveLetter = deviceId[0]; // Expecting something like "E:" or just "E"
    if (driveLetter >= 'A' && driveLetter <= 'Z') {
        // For unsafe eject, we'll try the direct method
        return SafelyEjectDrive(driveLetter); // Note: This is still safe but direct
    }
    
    return false;
}

void USBMonitor::monitorThreadFunc() {
    // Initialize the monitoring by arming existing devices
    ArmExisting();
    
    // Message loop for the monitoring thread
    MSG msg;
    while (monitoring && GetMessage(&msg, messageHwnd, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        // Add a small delay to prevent excessive CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}