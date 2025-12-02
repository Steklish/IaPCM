// lab05.js - JavaScript for USB device management

document.addEventListener('DOMContentLoaded', function() {
    // Get references to the buttons and other UI elements
    const disableMouseBtn = document.getElementById('disable-mouse-btn');
    const listDrivesBtn = document.getElementById('list-drives-btn');
    const refreshBtn = document.getElementById('refresh-btn');
    const refreshInputDevicesBtn = document.getElementById('refresh-input-devices-btn');

    // Set up event listeners
    if (disableMouseBtn) {
        disableMouseBtn.addEventListener('click', disableUsbMouse);
    }

    if (listDrivesBtn) {
        listDrivesBtn.addEventListener('click', listUsbDrives);
    }

    if (refreshBtn) {
        refreshBtn.addEventListener('click', listUsbDrives);
    }

    if (refreshInputDevicesBtn) {
        refreshInputDevicesBtn.addEventListener('click', listInputDevices);
    }
    
    // Load initial device lists
    listUsbDrives();
    listInputDevices();
});

// Function to disable USB mouse
async function disableUsbMouse() {
    try {
        const response = await fetch('/disableUsbMouse', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            }
        });
        
        const data = await response.json();
        addToActivityLog(data.message);
        
        if (data.status === 200) {
            alert('USB Mouse disabled successfully!');
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error('Error disabling USB mouse:', error);
        addToActivityLog('Error disabling USB mouse: ' + error.message);
    }
}

// Function to list USB drives
async function listUsbDrives() {
    try {
        const response = await fetch('/listUsbDrives', {
            method: 'GET'
        });
        
        const data = await response.json();
        
        if (data.status === 200) {
            displayUsbDrives(data.drives);
            addToActivityLog('USB drives listed successfully');
        } else {
            addToActivityLog('Error listing USB drives: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing USB drives:', error);
        addToActivityLog('Error listing USB drives: ' + error.message);
    }
}

// Function to list input devices
async function listInputDevices() {
    try {
        const response = await fetch('/listInputDevices', {
            method: 'GET'
        });
        
        const data = await response.json();
        
        if (data.status === 200) {
            displayInputDevices(data.devices);
            addToActivityLog('Input devices listed successfully');
        } else {
            addToActivityLog('Error listing input devices: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing input devices:', error);
        addToActivityLog('Error listing input devices: ' + error.message);
    }
}

// Function to display input devices in the UI
function displayInputDevices(devices) {
    const container = document.getElementById('input-devices-list');
    if (!container) return;
    
    if (devices.length === 0) {
        container.innerHTML = '<p>No input devices found</p>';
        return;
    }
    
    let html = '<ul class="input-device-list">';
    devices.forEach(device => {
        html += `
        <li class="input-device-item">
            <div class="device-info">
                <span class="device-name">${device.name}</span>
                <span class="device-type">[${device.type}]</span>
                <span class="device-vidpid">VID: ${device.vid}, PID: ${device.pid}</span>
            </div>
            <div class="device-status">
                <span class="status-indicator ${device.connected ? 'connected' : 'disconnected'}">
                    ${device.connected ? '● Connected' : '○ Disconnected'}
                </span>
            </div>
        </li>`;
    });
    html += '</ul>';
    
    container.innerHTML = html;
}

// Function to display USB drives in the UI
function displayUsbDrives(drives) {
    const container = document.getElementById('usb-devices-list');
    if (!container) return;
    
    if (drives.length === 0) {
        container.innerHTML = '<p>No removable drives found</p>';
        return;
    }
    
    let html = '<ul class="drive-list">';
    drives.forEach(drive => {
        html += `
        <li class="drive-item">
            <span class="drive-letter">${drive.letter}</span>
            <span class="drive-path">${drive.path}</span>
            <button class="eject-btn" onclick="ejectUsbDrive('${drive.letter}')">Eject</button>
            <button class="eject-cm-btn" onclick="ejectUsbDriveManual('${drive.letter}')">Eject (CM)</button>
        </li>`;
    });
    html += '</ul>';
    
    container.innerHTML = html;
}

// Function to eject USB drive
async function ejectUsbDrive(driveLetter) {
    if (!confirm(`Are you sure you want to eject drive ${driveLetter}:?`)) {
        return;
    }
    
    driveLetter = driveLetter[0]
    console.log(driveLetter)
    
    // Show goodbye.gif when ejecting starts
    showGoodbyeGif();
    
    try {
        const formData = new FormData();
        formData.append('drive', driveLetter);
        
        const response = await fetch(`/ejectUsbDrive`, {
            method: 'POST',
            body: formData
        });
        
        const data = await response.json();
        addToActivityLog(`Eject attempt for drive ${driveLetter}: ${data.message}`);
        
        if (data.status === 200) {
            alert(`Drive ${driveLetter}: ejected successfully!`);
            // Refresh the drive list after ejection
            setTimeout(listUsbDrives, 1000);
        } else {
            alert(`Error ejecting drive ${driveLetter}: ${data.message}`);
        }
    } catch (error) {
        console.error('Error ejecting USB drive:', error);
        addToActivityLog(`Error ejecting drive ${driveLetter}: ${error.message}`);
    } finally {
        // Hide the goodbye.gif after the operation completes
        hideGoodbyeGif();
    }
}

// Function to eject USB drive via CM API
async function ejectUsbDriveManual(driveLetter) {
    if (!confirm(`Are you sure you want to eject drive ${driveLetter}: via CM API?`)) {
        return;
    }
    
    driveLetter = driveLetter[0]

    // Show goodbye.gif when ejecting starts
    showGoodbyeGif();
    
    try {
        const formData = new FormData();
        formData.append('drive', driveLetter);
        
        const response = await fetch(`/ejectUsbDriveManual`, {
            method: 'POST',
            body: formData
        });
        
        const data = await response.json();
        addToActivityLog(`CM eject attempt for drive ${driveLetter}: ${data.message}`);
        
        if (data.status === 200) {
            alert(`Drive ${driveLetter}: ejected successfully via CM API!`);
            // Refresh the drive list after ejection
            setTimeout(listUsbDrives, 1000);
        } else {
            alert(`Error ejecting drive ${driveLetter} via CM API: ${data.message}`);
        }
    } catch (error) {
        console.error('Error ejecting USB drive via CM API:', error);
        addToActivityLog(`Error ejecting drive ${driveLetter} via CM API: ${error.message}`);
    } finally {
        // Hide the goodbye.gif after the operation completes
        hideGoodbyeGif();
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
    listInputDevices();
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