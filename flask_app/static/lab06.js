// lab06.js - JavaScript for Bluetooth & Audio management

document.addEventListener('DOMContentLoaded', function() {
    // Get references to the buttons and other UI elements
    const listBluetoothBtn = document.getElementById('list-bluetooth-btn');
    const toggleBluetoothBtn = document.getElementById('toggle-bluetooth-btn');
    const listAudioBtn = document.getElementById('list-audio-btn');
    const getAudioInfoBtn = document.getElementById('get-audio-info-btn');
    const refreshBtn = document.getElementById('refresh-btn');
    const audioFileInput = document.getElementById('audio-file-input');
    const playAudioBtn = document.getElementById('play-audio-btn');

    // Set up event listeners
    if (listBluetoothBtn) {
        listBluetoothBtn.addEventListener('click', listBluetoothDevices);
    }

    if (toggleBluetoothBtn) {
        toggleBluetoothBtn.addEventListener('click', toggleBluetooth);
    }

    if (listAudioBtn) {
        listAudioBtn.addEventListener('click', listAudioDevices);
    }

    if (getAudioInfoBtn) {
        getAudioInfoBtn.addEventListener('click', getAudioInfo);
    }

    if (playAudioBtn) {
        playAudioBtn.addEventListener('click', playAudioFile);
    }

    if (refreshBtn) {
        refreshBtn.addEventListener('click', () => {
            listBluetoothDevices();
            listAudioDevices();
        });
    }

    // Load initial device lists
    listBluetoothDevices();
    listAudioDevices();
});

// Function to list Bluetooth devices
async function listBluetoothDevices() {
    try {
        const response = await fetch('/listBluetoothDevices', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            displayBluetoothDevices(data.devices || []);
            addToActivityLog('Bluetooth devices listed successfully');
        } else {
            addToActivityLog('Error listing Bluetooth devices: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing Bluetooth devices:', error);
        addToActivityLog('Error listing Bluetooth devices: ' + error.message);
    }
}

// Function to toggle Bluetooth
async function toggleBluetooth() {
    try {
        const response = await fetch('/toggleBluetooth', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ state: 'toggle' })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        if (data.status === 'success') {
            alert('Bluetooth operation completed!');
            listBluetoothDevices(); // Refresh the device list
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error('Error toggling Bluetooth:', error);
        addToActivityLog('Error toggling Bluetooth: ' + error.message);
    }
}

// Function to list audio devices
async function listAudioDevices() {
    try {
        const response = await fetch('/listAudioDevices', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            displayAudioDevices(data.devices || []);
            addToActivityLog('Audio devices listed successfully');
        } else {
            addToActivityLog('Error listing audio devices: ' + data.message);
        }
    } catch (error) {
        console.error('Error listing audio devices:', error);
        addToActivityLog('Error listing audio devices: ' + error.message);
    }
}

// Function to get audio info
async function getAudioInfo() {
    try {
        const response = await fetch('/getAudioInfo', {
            method: 'GET'
        });

        const data = await response.json();

        if (data.status === 'success') {
            addToActivityLog('Audio system info retrieved');
            if (data.devices && data.devices.length > 0) {
                alert(`Audio System Info:\nTotal Devices: ${data.devices.length}`);
            } else {
                alert('No audio devices found');
            }
        } else {
            addToActivityLog('Error getting audio info: ' + data.message);
        }
    } catch (error) {
        console.error('Error getting audio info:', error);
        addToActivityLog('Error getting audio info: ' + error.message);
    }
}

// Function to display Bluetooth devices in the UI
function displayBluetoothDevices(devices) {
    const container = document.getElementById('bluetooth-devices-list');
    if (!container) return;

    if (!devices || devices.length === 0) {
        container.innerHTML = '<p>No Bluetooth devices found</p>';
        return;
    }

    let html = '<ul class="input-device-list">';
    devices.forEach(device => {
        const deviceName = device.name || device.displayName || 'Unknown Device';
        const deviceAddress = device.address || 'N/A';
        const isConnected = device.connected === true || (deviceName.toLowerCase().includes('connected'));
        
        html += `
        <li class="input-device-item">
            <div class="device-info">
                <span class="device-name">${deviceName.replace(' [CONNECTED]', '').replace(' [PAIRED]', '').replace(' [DISCOVERABLE]', '')}</span>
                <span class="device-vidpid">Address: ${deviceAddress}</span>
                <span class="device-type">[${isConnected ? 'CONNECTED' : 'DISCONNECTED'}]</span>
            </div>
            <div class="device-status">
                <span class="status-indicator ${isConnected ? 'connected' : 'disconnected'}">
                    ${isConnected ? '● Connected' : '○ Disconnected'}
                </span>
            </div>
        </li>`;
    });
    html += '</ul>';

    container.innerHTML = html;
}

// Function to display audio devices in the UI
function displayAudioDevices(devices) {
    const container = document.getElementById('audio-devices-list');
    if (!container) return;

    if (!devices || devices.length === 0) {
        container.innerHTML = '<p>No audio devices found</p>';
        return;
    }

    let html = '<ul class="input-device-list">';
    devices.forEach(device => {
        const deviceId = device.id !== undefined ? device.id : 'N/A';
        const deviceName = device.name || device.displayName || 'Unknown Device';
        const deviceType = device.type || 'Audio';
        const isBluetooth = device.bluetooth || deviceType.toLowerCase().includes('bluetooth');
        
        html += `
        <li class="input-device-item">
            <div class="device-info">
                <span class="device-name">${deviceName}</span>
                <span class="device-type">[${isBluetooth ? 'BLUETOOTH' : deviceType.toUpperCase()}]</span>
                <span class="device-vidpid">ID: ${deviceId}</span>
            </div>
            <div class="device-status">
                <button class="control-btn" onclick="setAudioVolume('${deviceName}', 50)">Set Volume</button>
                <button class="control-btn" onclick="toggleAudioMute('${deviceName}')">Mute/Unmute</button>
            </div>
        </li>`;
    });
    html += '</ul>';

    container.innerHTML = html;
}

// Function to set audio volume
async function setAudioVolume(deviceName, volume) {
    try {
        const response = await fetch('/setAudioVolume', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ device: deviceName, volume: parseInt(volume) })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        // The executable doesn't support volume control, so just log the attempt
        addToActivityLog(`Volume setting attempt for ${deviceName} to ${volume}% (not supported by executable)`);
    } catch (error) {
        console.error('Error setting audio volume:', error);
        addToActivityLog('Error setting audio volume: ' + error.message);
    }
}

// Function to toggle audio mute
async function toggleAudioMute(deviceName) {
    try {
        const response = await fetch('/toggleAudioMute', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ device: deviceName })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        // The executable doesn't support mute, so just log the attempt
        addToActivityLog(`Mute toggle attempt for ${deviceName} (not supported by executable)`);
    } catch (error) {
        console.error('Error toggling audio mute:', error);
        addToActivityLog('Error toggling audio mute: ' + error.message);
    }
}

// Function to play audio file
async function playAudioFile() {
    const audioFileInput = document.getElementById('audio-file-input');
    const file = audioFileInput.files[0];

    if (!file) {
        alert('Please select an audio file first');
        addToActivityLog('Error: No audio file selected');
        return;
    }

    // For now, just send the filename to the server
    try {
        const response = await fetch('/playAudio', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                audio_file: file.name  // In a real implementation, we would handle file uploads properly
            })
        });

        const data = await response.json();
        addToActivityLog(data.message);

        if (data.status === 'success') {
            alert('Audio playback command sent successfully!');
        } else {
            alert('Error: ' + data.message);
        }
    } catch (error) {
        console.error('Error playing audio:', error);
        addToActivityLog('Error playing audio: ' + error.message);
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