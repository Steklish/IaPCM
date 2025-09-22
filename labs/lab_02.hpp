#ifndef PCI_DEVICE_ENUMERATOR_H
#define PCI_DEVICE_ENUMERATOR_H

#include <string>
#include <vector>
#include <utility>

std::vector<std::pair<std::string, std::string>> EnumeratePCIDevices();

#endif // PCI_DEVICE_ENUMERATOR_H