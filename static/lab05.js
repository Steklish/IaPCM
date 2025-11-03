// Initialize device tracking before functions are defined
let previousDevices = [];

// Add log entry
function addLogEntry(message, type = 'info') {
    const logEntries = document.getElementById('log-entries');
    if (!logEntries) return;
    
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

// Display USB devices in the list - original function
function originalDisplayUSBDevices(devices) {
    const usbDevicesList = document.getElementById('usb-devices-list');
    if (!usbDevicesList) return;
    
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

// Refresh USB devices list
async function refreshUSBDevices() {
    const statusText = document.getElementById('status-text');
    if (!statusText) return;
    
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

// Enhanced displayUSBDevices function that checks for disconnections
function displayUSBDevices(devices) {
    // Check for disconnections before updating the display
    checkForDisconnectedDevices(devices);
    
    // Call the original function
    originalDisplayUSBDevices(devices);
}

// Function to show goodbye GIF
function showGoodbyeGif() {
    const goodbyeGif = document.getElementById('goodbye-gif');
    if (!goodbyeGif) return;
    
    goodbyeGif.style.display = 'flex';
    
    // After animation completes, hide the GIF again
    setTimeout(() => {
        goodbyeGif.style.display = 'none';
    }, 2000); // Match the CSS animation duration
}

// Check for disconnected devices by comparing with previous state
function checkForDisconnectedDevices(currentDevices) {
    // Only check for disconnections if we have a previous state (not on initial load)
    if (previousDevices.length > 0) {
        // Find devices that were present before but are not in the current list
        const disconnectedDevices = previousDevices.filter(prevDevice => 
            !currentDevices.some(currDevice => currDevice.id === prevDevice.id)
        );
        
        if (disconnectedDevices.length > 0) {
            // Show goodbye GIF for any USB device disconnection, but log differently based on type
            // For now, show goodbye GIF for any disconnected device, regardless of type
            var deviceList = disconnectedDevices.map(d => d.name || d.id).join(', ');
            addLogEntry('Device(s) disconnected: ' + deviceList, 'info');
            showGoodbyeGif();
        }
    }
    
    // Update the previous devices list
    previousDevices = [...currentDevices];
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

// DOM ready function
document.addEventListener('DOMContentLoaded', function() {
    // DOM elements - only accessed after DOM is loaded
    const coordsLabel = document.getElementById('cursor-coords');
    const screenLabel = document.getElementById('screen-dimensions');
    const refreshBtn = document.getElementById('refresh-btn');
    const statusText = document.getElementById('status-text');

    // Define updateScreenDimensions function inside DOMContentLoaded
    function updateScreenDimensions() {
        if (screenLabel) {
            screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
        }
    }

    // Initialize
    updateScreenDimensions();

    // Event listeners
    if (refreshBtn) {
        refreshBtn.addEventListener('click', refreshUSBDevices);
    }

    // Add a welcome message to the log
    addLogEntry('USB Monitor initialized. Monitoring for connected devices...', 'info');

    // Initial load with error handling
    try {
        refreshUSBDevices();
    } catch (error) {
        console.error('Error during initial USB device refresh:', error);
        addLogEntry(`Error during initial USB device refresh: ${error.message}`, 'error');
    }

    // Refresh devices every 5 seconds
    setInterval(() => {
        try {
            refreshUSBDevices();
        } catch (error) {
            console.error('Error during periodic USB device refresh:', error);
            addLogEntry(`Error during periodic USB device refresh: ${error.message}`, 'error');
        }
    }, 5000);

    // Update screen dimensions on window resize
    window.addEventListener('resize', updateScreenDimensions);

    // Hide the debugging information that was used for parallax
    if (coordsLabel) coordsLabel.style.visibility = 'hidden';
    if (screenLabel) screenLabel.style.visibility = 'hidden';
});