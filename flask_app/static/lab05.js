// lab05.js - JavaScript for USB device management

document.addEventListener('DOMContentLoaded', function() {
    // Get references to the buttons and other UI elements
    const disableMouseBtn = document.getElementById('disable-mouse-btn');
    const listDrivesBtn = document.getElementById('list-drives-btn');
    const refreshAllBtn = document.getElementById('refresh-all-btn');
    const refreshInputDevicesBtn = document.getElementById('refresh-input-devices-btn');
    const listUsbDevicesBtn = document.getElementById('list-usb-devices-btn');
    const getUsbInfoBtn = document.getElementById('get-usb-info-btn');

    // Set up event listeners
    if (disableMouseBtn) {
        disableMouseBtn.addEventListener('click', disableUsbMouse);
    }

    if (listDrivesBtn) {
        listDrivesBtn.addEventListener('click', listUsbDrives);
    }

    if (refreshAllBtn) {
        refreshAllBtn.addEventListener('click', () => {
            listUsbDrives();
            listUsbDevices();
        });
    }

    if (refreshInputDevicesBtn) {
        refreshInputDevicesBtn.addEventListener('click', listUsbDevices);
    }

    if (listUsbDevicesBtn) {
        listUsbDevicesBtn.addEventListener('click', listUsbDevices);
    }

    if (getUsbInfoBtn) {
        getUsbInfoBtn.addEventListener('click', getUsbInfo);
    }

    // Set up event delegation for dynamically created buttons
    document.addEventListener('click', function(e) {
        if (e.target.classList.contains('control-btn')) {
            const btn = e.target;
            const action = btn.getAttribute('data-action');

            if (action === 'toggle') {
                const deviceId = btn.getAttribute('data-device-id');
                const shouldDisable = btn.getAttribute('data-should-disable') === 'true';
                toggleUsbDevice(deviceId, shouldDisable);
            }
        }
    });

    // Load initial device lists
    listUsbDrives();
    listUsbDevices();
});

// Function to list USB devices (from executables)
async function listUsbDevices() {
    try {
        const response = await fetch('/listUsbDevices', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            displayUsbDevices(data.devices);
            addToActivityLog('USB devices listed successfully');
        } else {
            addToActivityLog('Error listing USB devices: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing USB devices:', error);
        addToActivityLog('Error listing USB devices: ' + error.message);
    }
}

// Function to get USB info
async function getUsbInfo() {
    try {
        const response = await fetch('/getUsbInfo', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            addToActivityLog('USB system info retrieved');
            if (data.devices && data.devices.length > 0) {
                alert(`USB System Info:\nTotal Devices: ${data.devices.length}`);
            } else {
                alert('No USB devices found');
            }
        } else {
            addToActivityLog('Error getting USB info: ' + data.message);
        }
    } catch (error) {
        console.error('Error getting USB info:', error);
        addToActivityLog('Error getting USB info: ' + error.message);
    }
}

// Function to disable USB mouse (from executables)
async function disableUsbMouse() {
    try {
        // Get the list of USB devices to find a mouse
        const response = await fetch('/listUsbDevices', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success' && data.devices) {
            // Look for a HID device that looks like a mouse
            const mouseDevice = data.devices.find(device => {
                const name = device.name || device.displayName || '';
                const type = device.type || device.deviceType || '';
                return (type.toLowerCase().includes('hid') || name.toLowerCase().includes('mouse')) && 
                       !name.toLowerCase().includes('keyboard');
            });

            if (mouseDevice) {
                const deviceId = mouseDevice.device_id || mouseDevice.deviceID || mouseDevice.name;
                const disableResponse = await fetch('/disableUsbDevice', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ device_id: deviceId })
                });

                const disableData = await disableResponse.json();

                if (disableData.status === 'success') {
                    alert('USB Mouse disabled successfully!');
                    addToActivityLog('USB Mouse disabled: ' + disableData.message);
                    listUsbDevices(); // Refresh the device list
                } else {
                    alert('Error: ' + disableData.message);
                }
            } else {
                alert('No USB mouse found to disable');
                addToActivityLog('No USB mouse found to disable');
            }
        } else {
            alert('Could not list USB devices to find mouse');
            addToActivityLog('Could not list USB devices: ' + (data.message || 'Unknown error'));
        }
    } catch (error) {
        console.error('Error disabling USB mouse:', error);
        alert('Could not disable USB mouse');
        addToActivityLog('Failed to disable USB mouse: ' + error.message);
    }
}

// Function to list USB drives - now gets all USB devices and filters for drives
async function listUsbDrives() {
    try {
        // Get all USB devices and filter for drives
        const response = await fetch('/listUsbDevices', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            displayUsbDrives(data.devices);
            addToActivityLog('USB drives listed successfully');
        } else {
            addToActivityLog('Error listing USB devices: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing USB drives:', error);
        addToActivityLog('Error listing USB drives: ' + error.message);
    }
}

// Function to display USB devices (from executables) in the UI - Non-storage devices only
function displayUsbDevices(devices) {
    const container = document.getElementById('other-usb-devices-list');
    if (!container) return;

    if (!devices || devices.length === 0) {
        container.innerHTML = '<p>No USB devices found</p>';
        return;
    }

    // Filter out storage devices (drives) so only other devices are shown here
    const otherDevices = devices.filter(device => !device.isDrive &&
        !(device.type && device.type.toLowerCase().includes('drive')) &&
        !(device.deviceType && device.deviceType.toLowerCase().includes('drive')) &&
        !device.driveLetter);

    if (otherDevices.length === 0) {
        container.innerHTML = '<p>No non-storage USB devices found</p>';
        return;
    }

    let html = '<ul class="input-device-list">';
    otherDevices.forEach(device => {
        const deviceId = device.device_id || device.deviceID || device.name;
        const deviceName = device.name || device.displayName || 'Unknown Device';
        const deviceType = device.type || device.deviceType || 'USB Device';
        const vid = device.vid || 'N/A';
        const pid = device.pid || 'N/A';

        html += `
        <li class="input-device-item">
            <div class="device-info">
                <span class="device-name">${deviceName}</span>
                <span class="device-type">[${deviceType}]</span>
                <span class="device-vidpid">VID: ${vid}, PID: ${pid}</span>
            </div>
            <div class="device-status">
                <span class="status-indicator connected">
                    ● Connected
                </span>
                <button class="control-btn" data-device-id="${deviceId.replace(/"/g, '&quot;')}" data-action="toggle" data-should-disable="true">Disable</button>
            </div>
        </li>`;
    });
    html += '</ul>';

    container.innerHTML = html;
}

// Function to display USB drives with eject functionality
function displayUsbDrives(devices) {
    const container = document.getElementById('usb-drives-list');
    if (!container) return;

    // Filter out only storage devices (drives) based on the updated detection method
    const storageDevices = devices.filter(device =>
        device.isDrive ||
        (device.driveLetter) ||
        (device.deviceType && device.deviceType.toLowerCase().includes('drive')) ||
        (device.type && device.type.toLowerCase().includes('drive')));

    if (storageDevices.length === 0) {
        container.innerHTML = '<p>No USB drives found</p>';
        return;
    }

    let html = '<ul class="input-device-list">';
    storageDevices.forEach(device => {
        // Extract drive letter using multiple methods to ensure it's captured
        let driveLetter = device.driveLetter || '';

        // If driveLetter is empty, try to extract from device name
        if (!driveLetter && device.name && device.name.includes(':')) {
            // Look for patterns like "Drive Letter: E:" in the device name
            const match = device.name.match(/Drive Letter:\s*([A-Za-z]):?/);
            if (match) {
                driveLetter = match[1];
            } else {
                // Fallback: take first character if it's followed by ':'
                const parts = device.name.split(':');
                if (parts.length > 0) {
                    const firstPart = parts[0].trim();
                    if (firstPart.length === 1 && /[A-Za-z]/.test(firstPart)) {
                        driveLetter = firstPart;
                    }
                }
            }
        }

        const deviceName = device.displayName || device.name || 'Unknown Drive';

        // Don't add the button if we don't have a valid drive letter
        if (!driveLetter) {
            console.warn('Could not extract drive letter for device:', device);
        }

        html += `
        <li class="input-device-item">
            <div class="device-info">
                <span class="device-name">${deviceName}</span>
                <span class="device-type">[DRIVE]</span>
                <span class="device-vidpid">${device.deviceID || device.vid ? 'ID: ' + (device.deviceID || device.vid) : 'ID: Unknown'}</span>
            </div>
            <div class="device-status">
                <span class="status-indicator connected">
                    ● Connected
                </span>
                ${driveLetter ? `<button class="control-btn" onclick="ejectUsbDrive('${driveLetter}')">Eject Safely</button>` : '<button class="control-btn" disabled>No Letter</button>'}
                ${driveLetter ? `<button class="control-btn" onclick="forceEjectUsbDrive('${driveLetter}')">Force Eject</button>` : '<button class="control-btn" disabled>No Letter</button>'}
            </div>
        </li>`;
    });
    html += '</ul>';

    container.innerHTML = html;
}

// Function to toggle (enable/disable) a USB device
async function toggleUsbDevice(deviceId, shouldDisable) {
    console.log("Toggle USB device called with deviceId:", deviceId, "shouldDisable:", shouldDisable);
    try {
        const url = shouldDisable ? '/disableUsbDevice' : '/enableUsbDevice';
        const action = shouldDisable ? 'disable' : 'enable';

        console.log("Making API call to:", url, "with device_id:", deviceId);

        const response = await fetch(url, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ device_id: deviceId })
        });

        console.log("API response status:", response.status);

        const data = await response.json();
        console.log("API response data:", data);

        addToActivityLog(data.message);

        if (data.status === 'success') {
            alert(`USB device ${action}d successfully!`);
            listUsbDevices(); // Refresh the device list
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error(`Error ${action}ing USB device:`, error);
        addToActivityLog(`Error ${action}ing USB device: ` + error.message);
    }
}

// Function to eject USB drive safely
async function ejectUsbDrive(driveLetter) {
    if (!driveLetter) {
        alert('No drive letter specified');
        return;
    }
    
    if (!confirm(`Are you sure you want to safely eject drive ${driveLetter}:?`)) {
        return;
    }

    try {
        const response = await fetch('/ejectUsbDrive', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ drive: driveLetter })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        if (data.status === 'success') {
            alert(`Drive ${driveLetter}: ejected successfully!`);
            listUsbDevices(); // Refresh the device list
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error('Error ejecting USB drive:', error);
        addToActivityLog('Error ejecting USB drive: ' + error.message);
    }
}

// Function to force eject USB drive
async function forceEjectUsbDrive(driveLetter) {
    if (!driveLetter) {
        alert('No drive letter specified');
        return;
    }
    
    if (!confirm(`Are you sure you want to force eject drive ${driveLetter}:? This may cause data loss if the drive is in use.`)) {
        return;
    }

    try {
        const response = await fetch('/forceEjectUsbDrive', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ drive: driveLetter })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        if (data.status === 'success') {
            alert(`Drive ${driveLetter}: force ejected successfully!`);
            listUsbDevices(); // Refresh the device list
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error('Error force ejecting USB drive:', error);
        addToActivityLog('Error force ejecting USB drive: ' + error.message);
    }
}

// Function to add entries to the activity log
function addToActivityLog(message) {
    const logContainer = document.getElementById('log-entries');
    if (!logContainer) return;

    const timestamp = new Date().toLocaleString();
    const logEntry = document.createElement('div');
    logEntry.className = 'log-entry';
    logEntry.innerHTML = `<span class="timestamp">[${timestamp}]</span> ${message}`;

    logContainer.prepend(logEntry); // Add to the top of the log

    // Keep only the last 50 entries to prevent the log from growing too large
    if (logContainer.children.length > 50) {
        logContainer.removeChild(logContainer.lastChild);
    }
}

// Function to refresh input devices (placeholder)
function refreshInputDevices() {
    listUsbDevices();
    addToActivityLog('Refreshing input devices...');
}

// Function to show the goodbye.gif overlay
function showGoodbyeGif() {
    // Check if the overlay already exists to avoid duplicates
    let overlay = document.getElementById('goodbye-overlay');
    if (overlay) {
        overlay.remove(); // Remove existing overlay if it exists
    }

    // Create overlay div
    overlay = document.createElement('div');
    overlay.id = 'goodbye-overlay';
    overlay.style.position = 'fixed';
    overlay.style.top = '0';
    overlay.style.left = '0';
    overlay.style.width = '100%';
    overlay.style.height = '100%';
    overlay.style.backgroundColor = 'rgba(0, 0, 0, 0.8)';
    overlay.style.display = 'flex';
    overlay.style.justifyContent = 'center';
    overlay.style.alignItems = 'center';
    overlay.style.zIndex = '9999';
    overlay.style.flexDirection = 'column';

    // Create and configure image element
    const gif = document.createElement('img');
    gif.src = '/static/lab_05/Goodbye.gif';
    gif.alt = 'Goodbye';
    gif.style.maxWidth = '50%';
    gif.style.maxHeight = '50%';
    gif.style.borderRadius = '10px';

    // Add text below the gif
    const text = document.createElement('div');
    text.textContent = 'Ejecting USB device...';
    text.style.color = 'white';
    text.style.fontSize = '24px';
    text.style.marginTop = '20px';
    text.style.textAlign = 'center';

    // Add elements to overlay
    overlay.appendChild(gif);
    overlay.appendChild(text);

    // Add overlay to the body
    document.body.appendChild(overlay);
}

// Function to hide the goodbye.gif overlay
function hideGoodbyeGif() {
    const overlay = document.getElementById('goodbye-overlay');
    if (overlay) {
        overlay.remove();
    }
}