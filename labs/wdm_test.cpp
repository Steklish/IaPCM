#include <ntddk.h>

extern "C"
NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    // Example I/O port address (replace with your device's port)
    PUCHAR ioPort = (PUCHAR)0x378; // Example: Parallel port base address

    // Read a byte from the I/O port
    UCHAR portValue = READ_PORT_UCHAR(ioPort);

    // Print the read value to debug output
    KdPrint(("Value read from I/O port 0x378: 0x%x\n", portValue));

    DriverObject->DriverUnload = [](PDRIVER_OBJECT) {
        KdPrint(("Driver unloaded\n"));
    };

    return STATUS_SUCCESS;
}
