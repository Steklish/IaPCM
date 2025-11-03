function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}

// DOM elements
const coordsLabel = document.getElementById('cursor-coords');
const screenLabel = document.getElementById('screen-dimensions');
const refreshBtn = document.getElementById('refresh-btn');
const usbDevicesList = document.getElementById('usb-devices-list');
const logEntries = document.getElementById('log-entries');
const statusText = document.getElementById('status-text');

// Initialize
updateScreenDimensions();

// Add log entry
function addLogEntry(message, type = 'info') {
    const timestamp = new Date().toLocaleTimeString();
    const logEntry = document.createElement('div');
    logEntry.className = `log-entry ${type}`;
    logEntry.innerHTML = `<span class="timestamp">[${timestamp}]</span> ${message}`;
    logEntries.prepend(logEntry); // Add to top of log
    
    // Limit log entries to prevent excessive DOM elements
    if (logEntries.children.length > 50) {
        logEntries.removeChild(logEntries.lastChild);
    }
}

// Refresh USB devices list
async function refreshUSBDevices() {
    try {
        statusText.textContent = 'Refreshing USB devices...';
        const response = await axios.get('/getUSBDevices');
        
        if (response.data.status === 200) {
            displayUSBDevices(response.data.devices);
            statusText.textContent = `Found ${response.data.devices.length} USB device(s)`;
        } else {
            statusText.textContent = 'Error getting USB devices';
            addLogEntry('Error getting USB devices from server', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        statusText.textContent = 'Error getting USB devices';
        addLogEntry(`Error getting USB devices: ${error.message}`, 'error');
    }
}

// Display USB devices in the list
function displayUSBDevices(devices) {
    // Clear current list
    usbDevicesList.innerHTML = '';
    
    if (devices.length === 0) {
        usbDevicesList.innerHTML = '<p class="no-devices">No USB devices connected</p>';
        return;
    }
    
    devices.forEach(device => {
        const deviceElement = document.createElement('div');
        deviceElement.className = 'device-item';
        
        deviceElement.innerHTML = `
            <div class="device-header">
                <span class="device-name">${device.name || 'Unknown Device'}</span>
                <span class="device-status ${device.mounted ? 'connected' : 'disconnected'}">
                    ${device.mounted ? 'Connected' : 'Disconnected'}
                </span>
            </div>
            <div class="device-details">
                <p><strong>Drive:</strong> ${device.drive || 'N/A'}</p>
                <p><strong>Type:</strong> ${device.type || 'N/A'}</p>
                <p><strong>ID:</strong> ${device.id || 'N/A'}</p>
                <div class="device-actions">
                    <button class="eject-btn safe-eject" onclick="safelyEjectDevice('${device.id}')">
                        Safely Eject
                    </button>
                    <button class="eject-btn unsafe-eject" onclick="unsafeEjectDevice('${device.id}')">
                        Unsafe Eject
                    </button>
                </div>
            </div>
        `;
        
        usbDevicesList.appendChild(deviceElement);
    });
}

// Safely eject a USB device
async function safelyEjectDevice(deviceId) {
    addLogEntry(`Attempting safe ejection for device: ${deviceId}`, 'info');
    
    try {
        const response = await axios.post('/safelyEjectUSB', deviceId);
        
        if (response.data.status === 200) {
            addLogEntry(response.data.message, 'success');
            // Refresh the device list to reflect changes
            setTimeout(refreshUSBDevices, 1000);
        } else {
            addLogEntry(response.data.message, 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        addLogEntry(`Error during safe ejection: ${error.message}`, 'error');
    }
}

// Unsafely eject a USB device
async function unsafeEjectDevice(deviceId) {
    addLogEntry(`Attempting unsafe ejection for device: ${deviceId}`, 'warning');
    
    try {
        const response = await axios.post('/unsafeEjectUSB', deviceId);
        
        if (response.data.status === 200) {
            addLogEntry(response.data.message, 'success');
            // Refresh the device list to reflect changes
            setTimeout(refreshUSBDevices, 1000);
        } else {
            addLogEntry(response.data.message, 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        addLogEntry(`Error during unsafe ejection: ${error.message}`, 'error');
    }
}

// Event listeners
if (refreshBtn) {
    refreshBtn.addEventListener('click', refreshUSBDevices);
}

// Initial load
refreshUSBDevices();

// Refresh devices every 5 seconds
setInterval(refreshUSBDevices, 5000);

// Add a welcome message to the log
addLogEntry('USB Monitor initialized. Monitoring for connected devices...', 'info');

// Update screen dimensions on window resize
window.addEventListener('resize', updateScreenDimensions);

// Hide the debugging information that was used for parallax
coordsLabel.style.visibility = 'hidden';
screenLabel.style.visibility = 'hidden';