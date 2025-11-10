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

#pragma comment(lib, "setupapi.lib")

static std::ofstream gLog;

static void logf(const std::string& s){ 
    SYSTEMTIME st; GetLocalTime(&st);
    char ts[64]; sprintf(ts,"%04u-%02u-%02u %02u:%02u:%02u ",
        st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    gLog<<ts<<s<<"\n"; gLog.flush();
    printf("%s%s\n", ts, s.c_str());
}

static bool IsUsbDrive(char letter){
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

static std::vector<char> list_removable_letters(){
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

struct State {
    HWND hwnd{};
    std::unordered_map<char,HANDLE> volHandle;          
    std::unordered_map<char,HDEVNOTIFY> volNotify;      
    std::unordered_map<HANDLE,char> handleToLetter;     
    std::unordered_map<char,bool> safePending;          
    std::unordered_map<char,bool> queryFailed;          
    std::unordered_map<char,bool> alreadyReported;      
} S;

static void Disarm(char L, bool keepMapping = false){
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

static bool Arm(char L){
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

static std::string maskToLetters(DWORD um){
    std::string r;
    for(int i=0;i<26;i++) if(um & (1u<<i)){ if(!r.empty()) r.push_back(','); r.push_back('A'+i); }
    return r;
}

static void ArmExisting(){
    for(char L : list_removable_letters()) Arm(L);
}

static bool SafelyEjectDrive(char driveLetter) {
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
static int EjectUsbDriveManual(char driveLetter) {
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

static bool SafelyEjectDriveViaCM(char driveLetter) {
    int result = EjectUsbDriveManual(driveLetter);
    return (result == 0);
}

// Ручное отключение USB-мыши через CM_Disable_DevNode
// Не нарушает мониторинг, не использует блокировки
// Возвращает: 0 = успех, >0 = код ошибки
// Выводит ответы в формате: OK USB_MOUSE_DISABLED или ERROR <тип_ошибки> [код]
static int DisableUsbMouseManual() {
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
                                          (descStr.find("keypad") == std::string::npos);
                            
                            if (isUsb && isMouse) {
                                // Поднимаемся по дереву устройств вверх, ища USB устройство с VID/PID
                                DEVINST currentInst = devInfoData.DevInst;
                                bool foundUsbDevice = false;
                                
                                for (int level = 0; level < 10; level++) {
                                    DEVINST parentInst = 0;
                                    CONFIGRET parentCr = CM_Get_Parent(&parentInst, currentInst, 0);
                                    if (parentCr != CR_SUCCESS) break;
                                    
                                    currentInst = parentInst;
                                    
                                    char parentInstanceId[512] = {0};
                                    CONFIGRET cr2 = CM_Get_Device_IDA(parentInst, parentInstanceId, sizeof(parentInstanceId), 0);
                                    if (cr2 == CR_SUCCESS) {
                                        std::string parentInstanceStr(parentInstanceId);
                                        logf("  [DEBUG] Проверяю родительское устройство (уровень " + std::to_string(level) + "): " + parentInstanceStr.substr(0, 80));
                                        
                                        // Ищем USB устройство с VID/PID
                                        if (parentInstanceStr.find("USB\\") == 0 && parentInstanceStr.find("VID_") != std::string::npos) {
                                            logf("  ✓ Найдено USB устройство для мыши: " + parentInstanceStr.substr(0, 80));
                                            mouseDevInst = parentInst;
                                            foundUsbDevice = true;
                                            foundMouse = true;
                                            break;
                                        }
                                    }
                                }
                                
                                if (!foundUsbDevice) {
                                    // Если не нашли USB устройство, используем само HID устройство
                                    logf("  ✓ Найдена USB HID мышь: " + std::string(description));
                                    mouseDevInst = devInfoData.DevInst;
                                    foundMouse = true;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        if (deviceCount == 0) {
            logf("  [DEBUG] HID устройства не найдены через SetupDiEnumDeviceInterfaces");
        } else if (!foundMouse) {
            logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " HID устройств, USB мышь не найдена");
        }
        
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    
    // Способ 2: Ищем через класс Mouse
    if (!foundMouse) {
        logf("  Способ 2: Ищем USB мышь через класс Mouse:");
        
        // GUID для мышей
        const GUID mouseClassGuid = {0x4d36e96f, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};
        
        HDEVINFO hDevInfo = SetupDiGetClassDevsA(
            &mouseClassGuid,
            nullptr,
            nullptr,
            DIGCF_PRESENT
        );
        
        if (hDevInfo == INVALID_HANDLE_VALUE) {
            DWORD err = GetLastError();
            logf("  [DEBUG] SetupDiGetClassDevsA для Mouse вернул INVALID_HANDLE_VALUE (ошибка " + std::to_string(err) + ")");
        } else {
            SP_DEVINFO_DATA devInfoData;
            devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
            
            DWORD deviceCount = 0;
            for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &devInfoData); i++) {
                deviceCount++;
                
                char instanceId[512] = {0};
                if (SetupDiGetDeviceInstanceIdA(hDevInfo, &devInfoData, instanceId, sizeof(instanceId), nullptr)) {
                    std::string instanceStr(instanceId);
                    std::transform(instanceStr.begin(), instanceStr.end(), instanceStr.begin(), ::tolower);
                    
                    char description[256] = {0};
                    SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr);
                    
                    logf("  [DEBUG] Проверяю мышь #" + std::to_string(i) + ": " + std::string(description) + " (" + instanceStr.substr(0, 60) + ")");
                    
                    // Проверяем, что это USB устройство (должно иметь VID_ в instance ID)
                    bool isUsb = (instanceStr.find("usb\\") != std::string::npos) || 
                                (instanceStr.find("hid\\vid_") != std::string::npos);
                    
                    if (isUsb) {
                        // Поднимаемся по дереву устройств вверх, ища USB устройство с VID/PID
                        DEVINST currentInst = devInfoData.DevInst;
                        bool foundUsbDevice = false;
                        
                        for (int level = 0; level < 10; level++) {
                            DEVINST parentInst = 0;
                            CONFIGRET parentCr = CM_Get_Parent(&parentInst, currentInst, 0);
                            if (parentCr != CR_SUCCESS) break;
                            
                            currentInst = parentInst;
                            
                            char parentInstanceId[512] = {0};
                            CONFIGRET cr2 = CM_Get_Device_IDA(parentInst, parentInstanceId, sizeof(parentInstanceId), 0);
                            if (cr2 == CR_SUCCESS) {
                                std::string parentInstanceStr(parentInstanceId);
                                logf("  [DEBUG] Проверяю родительское устройство (уровень " + std::to_string(level) + "): " + parentInstanceStr.substr(0, 80));
                                
                                // Ищем USB устройство с VID/PID
                                if (parentInstanceStr.find("USB\\") == 0 && parentInstanceStr.find("VID_") != std::string::npos) {
                                    logf("  ✓ Найдено USB устройство для мыши: " + parentInstanceStr.substr(0, 80));
                                    mouseDevInst = parentInst;
                                    foundUsbDevice = true;
                                    foundMouse = true;
                                    break;
                                }
                            }
                        }
                        
                        if (!foundUsbDevice) {
                            // Если не нашли USB устройство, используем само HID устройство
                            logf("  ✓ Найдена USB мышь (HID): " + std::string(description));
                            mouseDevInst = devInfoData.DevInst;
                            foundMouse = true;
                        }
                        break;
                    }
                }
            }
            
            if (deviceCount == 0) {
                logf("  [DEBUG] Мыши не найдены через SetupDiEnumDeviceInfo");
            } else if (!foundMouse) {
                logf("  [DEBUG] Проверено " + std::to_string(deviceCount) + " мышей, USB мышь не найдена");
            }
            
            SetupDiDestroyDeviceInfoList(hDevInfo);
        }
    }
    
    // Способ 3: Ищем USB устройство с VID/PID, поднимаясь по дереву от найденных HID устройств
    if (!foundMouse) {
        logf("  Способ 3: Ищем USB устройство, поднимаясь по дереву от HID устройств:");
        
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
                        char description[256] = {0};
                        SetupDiGetDeviceRegistryPropertyA(hDevInfo, &devInfoData, SPDRP_DEVICEDESC, nullptr, (PBYTE)description, sizeof(description), nullptr);
                        std::string descStr(description);
                        std::transform(descStr.begin(), descStr.end(), descStr.begin(), ::tolower);
                        
                        bool isMouse = (descStr.find("mouse") != std::string::npos) && 
                                      (descStr.find("keyboard") == std::string::npos);
                        
                        if (isMouse) {
                            // Поднимаемся по дереву устройств вверх, ища USB устройство
                            DEVINST currentInst = devInfoData.DevInst;
                            for (int level = 0; level < 10; level++) {
                                DEVINST parentInst = 0;
                                CONFIGRET parentCr = CM_Get_Parent(&parentInst, currentInst, 0);
                                if (parentCr != CR_SUCCESS) break;
                                
                                currentInst = parentInst;
                                
                                char parentInstanceId[512] = {0};
                                CONFIGRET cr2 = CM_Get_Device_IDA(parentInst, parentInstanceId, sizeof(parentInstanceId), 0);
                                if (cr2 == CR_SUCCESS) {
                                    std::string parentInstanceStr(parentInstanceId);
                                    logf("  [DEBUG] Проверяю родительское устройство (уровень " + std::to_string(level) + "): " + parentInstanceStr.substr(0, 80));
                                    
                                    // Ищем USB устройство с VID/PID
                                    if (parentInstanceStr.find("USB\\") == 0 && parentInstanceStr.find("VID_") != std::string::npos) {
                                        logf("  ✓ Найдено USB устройство для мыши: " + parentInstanceStr.substr(0, 80));
                                        mouseDevInst = parentInst;
                                        foundMouse = true;
                                        break;
                                    }
                                }
                            }
                            
                            if (foundMouse) break;
                        }
                    }
                }
            }
            
            SetupDiDestroyDeviceInfoList(hDevInfo);
        }
    }
    
    if (!foundMouse) {
        printf("ERROR NO_USB_MOUSE_FOUND 0\n");
        logf("ERROR: USB мышь не найдена");
        return 1;
    }
    
    // Проверяем статус устройства перед отключением
    ULONG status = 0;
    ULONG problem = 0;
    CONFIGRET statusCr = CM_Get_DevNode_Status(&status, &problem, mouseDevInst, 0);
    if (statusCr == CR_SUCCESS) {
        logf("  [DEBUG] Статус устройства: status=0x" + std::to_string(status) + ", problem=0x" + std::to_string(problem));
    }
    
    // Пробуем отключить устройство через CM_Disable_DevNode
    // НЕ используем CM_DISABLE_PERSIST, чтобы устройство можно было включить при переподключении
    logf("  Пробуем отключить USB мышь: DevInst=" + std::to_string(mouseDevInst));
    CONFIGRET cr = CM_Disable_DevNode(mouseDevInst, 0);  // 0 = временное отключение, не постоянное
    
    logf("  CM_Disable_DevNode вернул код: " + std::to_string(cr));
    
    // Если CM_Disable_DevNode не работает, пробуем CM_Request_Device_Eject
    if (cr != CR_SUCCESS && cr != CR_INVALID_DEVNODE) {
        logf("  Пробуем альтернативный метод: CM_Request_Device_Eject");
        PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
        char vetoName[MAX_PATH] = {0};
        CONFIGRET ejectCr = CM_Request_Device_EjectA(mouseDevInst, &vetoType, vetoName, sizeof(vetoName), 0);
        logf("  CM_Request_Device_Eject вернул код: " + std::to_string(ejectCr));
        
        if (ejectCr == CR_SUCCESS) {
            printf("OK USB_MOUSE_DISABLED\n");
            logf("✓ РУЧНОЕ ОТКЛЮЧЕНИЕ: USB мышь успешно отключена через CM_Request_Device_Eject");
            return 0;
        }
    }
    
    if (cr == CR_SUCCESS) {
        printf("OK USB_MOUSE_DISABLED\n");
        logf("✓ РУЧНОЕ ОТКЛЮЧЕНИЕ: USB мышь успешно отключена через CM_Disable_DevNode");
        return 0;
    } else if (cr == CR_INVALID_DEVNODE) {
        // Устройство может быть уже отключено или не существует
        // Проверяем статус еще раз
        statusCr = CM_Get_DevNode_Status(&status, &problem, mouseDevInst, 0);
        if (statusCr == CR_SUCCESS && (status & DN_STARTED) == 0) {
            // Устройство уже отключено
            printf("OK USB_MOUSE_DISABLED\n");
            logf("✓ РУЧНОЕ ОТКЛЮЧЕНИЕ: USB мышь уже отключена (статус: 0x" + std::to_string(status) + ")");
            return 0;
        } else {
            printf("ERROR DISABLE_FAILED %lu\n", (DWORD)cr);
            logf("ERROR: CM_Disable_DevNode failed для USB мыши (код " + std::to_string(cr) + ", статус: 0x" + std::to_string(status) + ")");
            return 2;
        }
    } else {
        printf("ERROR DISABLE_FAILED %lu\n", (DWORD)cr);
        logf("ERROR: CM_Disable_DevNode failed для USB мыши (код " + std::to_string(cr) + ")");
        return 2;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l){
    switch(m){
    case WM_CREATE: return 0;
    
    case WM_TIMER:{
        if(w==1){
            KillTimer(h,1);
            // Сканируем диски после подключения USB
            logf("[DEBUG] Таймер: сканирование дисков...");
            auto currentDisks = list_removable_letters();
            if(currentDisks.empty()){
                logf("[DEBUG] Таймер: нет съёмных дисков");
            }
            for(char L : currentDisks){
                if(!S.volHandle.count(L)){
                    logf("→ USB диск "+std::string(1,L)+": обнаружен при сканировании");
                    Arm(L);
                }else{
                    logf("[DEBUG] Диск "+std::string(1,L)+": уже зарегистрирован");
                }
            }
        }
        else if(w==2){
            KillTimer(h,2);
            logf("[DEBUG] Таймер SAFE: проверка дисков после QUERYREMOVE...");
            auto currentDisks = list_removable_letters();
            std::vector<char> toRemove;
            std::vector<char> toReportFailed;
            
            for(auto& pair : S.safePending){
                char L = pair.first;
                bool stillExists = std::find(currentDisks.begin(), currentDisks.end(), L) != currentDisks.end();
                
                if(!stillExists && !S.alreadyReported[L]){
                    logf("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён через системное меню");
                    S.alreadyReported[L]=true;
                    toRemove.push_back(L);
                }else if(stillExists && !S.alreadyReported[L] && !S.queryFailed[L]){

                    logf("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск "+std::string(1,L)+": - устройство занято!");
                    logf("  Причина: открыты файлы или идёт запись на диск");
                    S.queryFailed[L]=true;
                    S.safePending.erase(L);
                    toReportFailed.push_back(L);
                }
            }
            
            for(char L : toRemove){
                S.safePending.erase(L);
                Disarm(L); 
                S.alreadyReported.erase(L);
            }
            
            for(char L : toReportFailed){
                Arm(L); 
                S.alreadyReported.erase(L);
            }
        }
        return 0;
    }

    case WM_DEVICECHANGE:{
        PDEV_BROADCAST_HDR hdr = (PDEV_BROADCAST_HDR)l;
        
        if(hdr){
            std::string evtName = "UNKNOWN";
            if(w==DBT_DEVICEARRIVAL) evtName="ARRIVAL";
            if(w==DBT_DEVICEREMOVECOMPLETE) evtName="REMOVECOMPLETE";
            if(w==DBT_DEVICEQUERYREMOVE) evtName="QUERYREMOVE";
            if(w==DBT_DEVICEQUERYREMOVEFAILED) evtName="QUERYREMOVEFAILED";
            
            char dbgmsg[256];
            sprintf(dbgmsg, "[DEBUG] Event=%s DevType=%d", evtName.c_str(), hdr->dbch_devicetype);
            logf(dbgmsg);
        }

        if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_HANDLE){
            auto* dh=(PDEV_BROADCAST_HANDLE)hdr;
            auto it = S.handleToLetter.find(dh->dbch_handle);
            char L = (it==S.handleToLetter.end()? 0 : it->second);
            if(!L) return TRUE;

            if(w==DBT_DEVICEQUERYREMOVE){
                logf("→ QUERYREMOVE(HANDLE) для диска "+std::string(1,L)+": - пользователь запросил безопасное извлечение");
                S.safePending[L]=true;
                S.alreadyReported[L]=false;
                
                auto it = S.volHandle.find(L);
                if(it!=S.volHandle.end()){
                    HANDLE h = it->second;
                    auto itN = S.volNotify.find(L);
                    if(itN!=S.volNotify.end() && itN->second){ 
                        UnregisterDeviceNotification(itN->second); 
                        S.volNotify.erase(L);
                    }
                    CloseHandle(h); 
                    S.volHandle.erase(it);
                }
                
                SetTimer(S.hwnd, 2, 3000, nullptr); 
                return TRUE;            
            }
            if(w==DBT_DEVICEQUERYREMOVEFAILED){
                logf("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск "+std::string(1,L)+": - устройство занято!");
                logf("  Причина: открыты файлы или идёт запись на диск");
                S.queryFailed[L]=true;
                S.safePending.erase(L);
                S.alreadyReported.erase(L);
                KillTimer(S.hwnd, 2); 
                
                Arm(L);
                return TRUE;
            }
            if(w==DBT_DEVICEREMOVECOMPLETE){
                if(!S.alreadyReported[L]){
                    bool safe = S.safePending.count(L)>0;
                    if(safe){
                        logf("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён через системное меню");
                    }else{
                        logf("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён без запроса (выдернули флешку)");
                    }
                    S.alreadyReported[L]=true;
                }
                S.safePending.erase(L);
                S.queryFailed.erase(L);
                S.handleToLetter.erase(dh->dbch_handle); 
                KillTimer(S.hwnd, 2); 
                Disarm(L);
                return TRUE;
            }
            return TRUE;
        }

        if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_VOLUME){
            auto* dv=(PDEV_BROADCAST_VOLUME)hdr;
            std::string letters = maskToLetters(dv->dbcv_unitmask);
            for(char L : letters){
                if(w==DBT_DEVICEARRIVAL){
                    logf("→ USB диск "+std::string(1,L)+": подключён");
                    Arm(L);
                }else if(w==DBT_DEVICEREMOVECOMPLETE){

                    if(!S.alreadyReported[L]){
                        bool safe = S.safePending.count(L)>0;
                        if(safe){
                            logf("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён через системное меню");
                        }else{
                            logf("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён без запроса (выдернули флешку)");
                        }
                        S.alreadyReported[L]=true;
                    }
                    S.safePending.erase(L);
                    S.queryFailed.erase(L);
                    S.alreadyReported.erase(L);
                    Disarm(L);
                }else if(w==DBT_DEVICEQUERYREMOVEFAILED){
                    logf("⚠ ОТКАЗ В БЕЗОПАСНОМ ИЗВЛЕЧЕНИИ: Диск "+std::string(1,L)+": - устройство занято!");
                    logf("  Причина: открыты файлы или идёт запись на диск");
                    S.queryFailed[L]=true;
                    S.safePending.erase(L);
                    S.alreadyReported.erase(L);
                    KillTimer(S.hwnd, 2); 
                }else if(w==DBT_DEVICEREMOVEPENDING || w==DBT_DEVICEQUERYREMOVE){
                    logf("[DEBUG] QUERYREMOVE(volume hint) "+std::string(1,L));
                }
            }
            return 0;
        }

        if(hdr && hdr->dbch_devicetype==DBT_DEVTYP_DEVICEINTERFACE){
            auto* di=(PDEV_BROADCAST_DEVICEINTERFACE_A)hdr;
            std::string path = di && di->dbcc_name? di->dbcc_name : "";
            if(w==DBT_DEVICEARRIVAL && path.find("USB#") != std::string::npos){
                // При подключении USB устройства проверяем, не отключено ли оно программно
                // и если да - включаем его автоматически
                logf("[DEBUG] USB устройство подключено: " + path.substr(0, 80));
                
                // Пытаемся найти устройство по пути и включить его, если оно отключено
                // Это поможет восстановить работу мыши после переподключения
                // (детальная реализация требует парсинга пути устройства)
                logf("[DEBUG] USB устройство подключено: "+path.substr(0,80));

                SetTimer(S.hwnd, 1, 2000, nullptr); 
            }
            if(w==DBT_DEVICEREMOVECOMPLETE && path.find("USB#") != std::string::npos){
                logf("[DEBUG] USB устройство отключено: "+path.substr(0,80));
                
                auto currentDisks = list_removable_letters();
                
                std::vector<char> toRemove;
                for(auto& pair : S.volHandle){
                    char L = pair.first;
                    bool stillExists = std::find(currentDisks.begin(), currentDisks.end(), L) != currentDisks.end();
                    if(!stillExists && !S.alreadyReported[L]){
                        bool safe = (S.safePending.count(L) > 0);
                        if(safe){
                            logf("✓ БЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён через системное меню");
                        }else{
                            logf("✗ НЕБЕЗОПАСНОЕ ИЗВЛЕЧЕНИЕ: Диск "+std::string(1,L)+": извлечён без запроса (выдернули флешку)");
                        }
                        S.alreadyReported[L]=true;
                        S.safePending.erase(L);
                        toRemove.push_back(L);
                    }
                }
                for(char L : toRemove){
                    Disarm(L);
                    S.alreadyReported.erase(L);
                }
            }
            return 0;
        }
        return 0;
    }

    case WM_DESTROY: PostQuitMessage(0); return 0;
    default: return DefWindowProc(h,m,w,l);
    }
}

static HANDLE g_lockHandle = INVALID_HANDLE_VALUE;
static char g_lockedDrive = 0;

static DWORD WINAPI ConsoleInputThread(LPVOID param) {
    printf("\n=== Commands ===\n");
    printf("  eject X   - Safely eject drive X: (e.g., 'eject E')\n");
    printf("  ejectcm X - Safely eject drive X: via CM API\n");
    printf("  lock X    - LOCK drive X: to test QUERYREMOVEFAILED\n");
    printf("  unlock    - UNLOCK locked drive\n");
    printf("  list      - List all removable drives\n");
    printf("  EJECT_USB_DRIVE <letter> - Safely eject USB drive\n");
    printf("  DISABLE_USB_MOUSE - Disable USB HID mouse\n");
    printf("  quit      - Exit program\n");
    printf("================\n\n");
    
    char line[256];
    while (true) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        // Убираем \n
        size_t len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = 0;
        
        if (strncmp(line, "eject ", 6) == 0 && strlen(line) >= 7) {
            char drive = toupper(line[6]);
            if (drive >= 'A' && drive <= 'Z') {
                SafelyEjectDrive(drive);
            } else {
                printf("Invalid drive letter\n");
            }
        }
        else if (strncmp(line, "ejectcm ", 8) == 0 && strlen(line) >= 9) {
            char drive = toupper(line[8]);
            if (drive >= 'A' && drive <= 'Z') {
                int result = EjectUsbDriveManual(drive);
                if (result == 0) {
                    printf("Drive %c: ejected successfully\n", drive);
                } else {
                    printf("Failed to eject drive %c: (error code %d)\n", drive, result);
                }
            } else {
                printf("Invalid drive letter\n");
            }
        }
        else if (strncmp(line, "EJECT_USB_DRIVE ", 16) == 0 && strlen(line) >= 17) {
            char drive = toupper(line[16]);
            if (drive >= 'A' && drive <= 'Z') {
                int result = EjectUsbDriveManual(drive);
                if (result == 0) {
                    printf("Command completed successfully\n");
                } else {
                    printf("Command failed with error code %d\n", result);
                }
            } else {
                printf("ERROR INVALID_DRIVE_LETTER\n");
            }
        }
        else if (strcmp(line, "DISABLE_USB_MOUSE") == 0) {
            int result = DisableUsbMouseManual();
            if (result == 0) {
                printf("Command completed successfully\n");
            } else {
                printf("Command failed with error code %d\n", result);
            }
        }
        else if (strncmp(line, "lock ", 5) == 0 && strlen(line) >= 6) {
            char drive = toupper(line[5]);
            if (drive >= 'A' && drive <= 'Z') {
                if (g_lockHandle != INVALID_HANDLE_VALUE) {
                    printf("Drive %c: is already locked. Unlock first!\n", g_lockedDrive);
                } else {
                    std::string path = "";
                    path.push_back(drive);
                    path += ":\\test_lock_file.dat";
                    
                    g_lockHandle = CreateFileA(
                        path.c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        0, 
                        nullptr,
                        CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL,
                        nullptr
                    );
                    
                    if (g_lockHandle == INVALID_HANDLE_VALUE) {
                        printf("ERROR: Failed to lock drive %c: (error %lu)\n", drive, GetLastError());
                    } else {
                        const char* data = "LOCKED FOR TEST";
                        DWORD written;
                        WriteFile(g_lockHandle, data, strlen(data), &written, nullptr);
                        FlushFileBuffers(g_lockHandle);
                        
                        g_lockedDrive = drive;
                        printf("✓ Drive %c: LOCKED!\n", drive);
                        printf("  File: %c:\\test_lock_file.dat\n", drive);
                        printf("  Now try 'Safely Remove Hardware' - it will FAIL!\n");
                        printf("  Type 'unlock' to release\n");
                    }
                }
            } else {
                printf("Invalid drive letter\n");
            }
        }
        else if (strcmp(line, "unlock") == 0) {
            if (g_lockHandle == INVALID_HANDLE_VALUE) {
                printf("No drive is locked\n");
            } else {
                CloseHandle(g_lockHandle);
                g_lockHandle = INVALID_HANDLE_VALUE;
                
                std::string path = "";
                path.push_back(g_lockedDrive);
                path += ":\\test_lock_file.dat";
                DeleteFileA(path.c_str());
                
                printf("✓ Drive %c: UNLOCKED!\n", g_lockedDrive);
                g_lockedDrive = 0;
            }
        }
        else if (strcmp(line, "list") == 0) {
            auto drives = list_removable_letters();
            if (drives.empty()) {
                printf("No removable drives found\n");
            } else {
                printf("Removable drives: ");
                for (char d : drives) printf("%c: ", d);
                printf("\n");
            }
        }
        else if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) {
            printf("Exiting...\n");
            PostQuitMessage(0);
            break;
        }
        else if (strlen(line) > 0) {
            printf("Unknown command. Type 'list', 'eject X', 'ejectcm X', or 'quit'\n");
        }
    }
    return 0;
}

int main(int argc, char* argv[]){
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
    gLog.open("usb_log.txt", std::ios::app);

    // Обработка аргументов командной строки
    if (argc > 1) {
        std::string cmd = argv[1];
        
        if (cmd == "--eject" && argc > 2) {
            char driveLetter = toupper(argv[2][0]);
            if (driveLetter >= 'A' && driveLetter <= 'Z') {
                int result = EjectUsbDriveManual(driveLetter);
                fflush(stdout);
                fflush(stderr);
                gLog.close();
                return result;  // 0 = успех, >0 = код ошибки
            } else {
                printf("ERROR INVALID_DRIVE_LETTER\n");
                fflush(stdout);
                gLog.close();
                return 1;
            }
        }
        else if (cmd == "--list-drives-json") {
            auto drives = list_removable_letters();
            printf("[");
            for (size_t i = 0; i < drives.size(); i++) {
                char letter = drives[i];
                char root[4] = {letter, ':', '\\', 0};
                DWORD sectorsPerCluster, bytesPerSector, freeClusters, totalClusters;
                bool isReady = GetDiskFreeSpaceA(root, &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters);
                
                ULARGE_INTEGER totalBytes, freeBytes;
                if (isReady) {
                    totalBytes.QuadPart = (ULONGLONG)totalClusters * sectorsPerCluster * bytesPerSector;
                    freeBytes.QuadPart = (ULONGLONG)freeClusters * sectorsPerCluster * bytesPerSector;
                } else {
                    totalBytes.QuadPart = 0;
                    freeBytes.QuadPart = 0;
                }
                
                char label[MAX_PATH + 1] = {0};
                GetVolumeInformationA(root, label, MAX_PATH, nullptr, nullptr, nullptr, nullptr, 0);
                
                printf("%s{\"letter\":\"%c\",\"label\":\"%s\",\"isReady\":%s,\"totalBytes\":%llu,\"freeBytes\":%llu}",
                       (i > 0 ? "," : ""),
                       letter,
                       label,
                       isReady ? "true" : "false",
                       totalBytes.QuadPart,
                       freeBytes.QuadPart);
            }
            printf("]\n");
            gLog.close();
            return 0;
        }
        else if (cmd == "--disable-hid-mouse") {
            int result = DisableUsbMouseManual();
            fflush(stdout);
            fflush(stderr);
            gLog.close();
            return result;  // 0 = успех, >0 = код ошибки
        }
    }

    HINSTANCE hi=GetModuleHandle(nullptr);
    const char* cls="UsbMonWnd";
    WNDCLASSA wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.lpszClassName=cls;
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowExA(0,cls,"",0,0,0,0,0,HWND_MESSAGE,nullptr,hi,nullptr);
    S.hwnd=hwnd;

    auto regIface=[&](const GUID& g){
        DEV_BROADCAST_DEVICEINTERFACE_A f{}; f.dbcc_size=sizeof(f);
        f.dbcc_devicetype=DBT_DEVTYP_DEVICEINTERFACE; f.dbcc_classguid=g;
        RegisterDeviceNotificationA(hwnd,&f,DEVICE_NOTIFY_WINDOW_HANDLE);
    };
    const GUID GUID_DEVINTERFACE_VOLUME = {0x53f5630d,0xb6bf,0x11d0,{0x94,0xf2,0x00,0xa0,0xc9,0x1e,0xfb,0x8b}};
    const GUID GUID_DEVINTERFACE_USB_DEVICE = {0xA5DCBF10L,0x6530,0x11D2,{0x90,0x1F,0x00,0xC0,0x4F,0xB9,0x51,0xED}};
    regIface(GUID_DEVINTERFACE_VOLUME);
    regIface(GUID_DEVINTERFACE_USB_DEVICE);

    logf("═══════════════════════════════════════════════════════");
    logf("  USB Monitor - Контроль безопасного извлечения USB");
    logf("═══════════════════════════════════════════════════════");
    logf("");
    logf("Сканирование USB устройств...");

    ArmExisting();
    
    logf("");
    logf("✓ Мониторинг запущен. Ожидание событий...");
    logf("  Команды: list | eject X | ejectcm X | quit");
    logf("");

    HANDLE hThread = CreateThread(nullptr, 0, ConsoleInputThread, nullptr, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
    }

    MSG msg;
    while(GetMessage(&msg,nullptr,0,0)){ TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}
