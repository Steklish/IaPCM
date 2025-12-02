from flask import Flask, render_template, send_from_directory, request, jsonify
import os
import subprocess
import json
import sys
import re

app = Flask(__name__)

def run_usb_executable(args):
    """Run the USB manager executable with specified arguments"""
    exec_path = os.path.join(os.getcwd(), 'executables', 'usb_manager.exe')
    try:
        result = subprocess.run([exec_path] + args, capture_output=True, text=True, timeout=30)
        
        # Parse the output to extract device information
        if result.returncode == 0:
            output_lines = result.stdout.strip().split('\n')
            
            # Filter out informational lines and keep only device entries
            devices = []
            current_device = {}
            
            for line in output_lines:
                line = line.strip()
                
                # Check if this is a device entry line (starts with a number and bracket)
                if line.startswith('[') and line[1].isdigit() and '] ' in line:
                    # If we have a previous device, save it
                    if current_device and 'displayName' in current_device:
                        devices.append(current_device)
                    
                    # Start a new device
                    current_device = {}
                    
                    # Extract device name (after the number bracket)
                    parts = line.split('] ', 1)
                    if len(parts) > 1:
                        current_device['name'] = parts[1]
                        current_device['displayName'] = parts[1]
                
                # Extract device ID, type, drive letter, etc.
                elif 'Device ID:' in line:
                    device_id = line.replace('Device ID:', '').strip()
                    current_device['deviceID'] = device_id
                    # Use device ID as device_id for API compatibility
                    current_device['device_id'] = device_id
                    # Extract VID/PID from device ID if present
                    vid_match = re.search(r'USB\\VID_([0-9A-Fa-f]+)', device_id)
                    pid_match = re.search(r'PID_([0-9A-Fa-f]+)', device_id)
                    if vid_match:
                        current_device['vid'] = vid_match.group(1)
                    if pid_match:
                        current_device['pid'] = pid_match.group(1)
                
                elif 'Type:' in line:
                    # Check if the type contains "Drive" to identify USB drives
                    original_type = line.replace('Type:', '').strip()
                    is_drive = 'Drive' in original_type

                    # Check if the type line contains drive letter info like "Drive Letter: E:"
                    drive_letter_match = re.search(r'Drive Letter:\s*([A-Za-z]):?', original_type)
                    if drive_letter_match:
                        drive_letter = drive_letter_match.group(1).strip()
                        current_device['driveLetter'] = drive_letter

                    device_type = original_type.replace('(Drive)', '').replace('(drive)', '').strip()
                    # Also remove drive letter part from type if present
                    device_type = re.sub(r'- Drive Letter:\s*[A-Za-z]:?', '', device_type).strip()
                    current_device['type'] = device_type
                    current_device['deviceType'] = device_type
                    current_device['isDrive'] = is_drive

                elif 'Drive Letter:' in line:
                    # Extract drive letter from format like "Drive Letter: E:"
                    drive_letter = line.replace('Drive Letter:', '').strip()
                    # Remove the colon if present
                    if drive_letter.endswith(':'):
                        drive_letter = drive_letter[:-1]
                    current_device['driveLetter'] = drive_letter.strip()

                elif 'Removable:' in line:
                    is_removable = 'Yes' in line
                    current_device['removable'] = is_removable
                    current_device['connected'] = True  # Removable devices are considered connected
                
                elif line == '--------------------------------------------------':
                    # End of current device entry
                    if current_device and 'displayName' in current_device:
                        devices.append(current_device)
                        current_device = {}
            
            # Add the last device if exists
            if current_device and 'displayName' in current_device:
                devices.append(current_device)
            
            return {"status": "success", "devices": devices}
        else:
            return {"status": "error", "message": result.stderr or f"Command failed with exit code {result.returncode}"}
    
    except subprocess.TimeoutExpired:
        return {"status": "error", "message": "Command timed out"}
    except FileNotFoundError:
        return {"status": "error", "message": "USB manager executable not found"}
    except Exception as e:
        return {"status": "error", "message": str(e)}

def run_bluetooth_executable():
    """Run the Bluetooth scanner executable"""
    exec_path = os.path.join(os.getcwd(), 'executables', 'bluetooth_scanner.exe')
    try:
        result = subprocess.run([exec_path], capture_output=True, text=True, timeout=45)
        
        if result.returncode == 0:
            output_lines = result.stdout.strip().split('\n')
            devices = []
            
            current_device = {}
            for line in output_lines:
                line = line.strip()
                
                if line.startswith('Name:'):
                    # If we have a previous device, save it
                    if current_device:
                        devices.append(current_device)
                    
                    device_name = line.replace('Name:', '').strip()
                    # Check if the name contains connection status
                    is_connected = '[CONNECTED]' in device_name
                    is_paired = '[PAIRED]' in device_name
                    is_discoverable = '[DISCOVERABLE]' in device_name
                    
                    clean_name = device_name.replace(' [CONNECTED]', '').replace(' [PAIRED]', '').replace(' [DISCOVERABLE]', '')
                    
                    current_device = {
                        'name': clean_name,
                        'connected': is_connected,
                        'paired': is_paired,
                        'discoverable': is_discoverable
                    }
                
                elif line.startswith('Address:') and current_device:
                    address = line.replace('Address:', '').strip()
                    current_device['address'] = address
            
            # Add the last device if exists
            if current_device:
                devices.append(current_device)
            
            return {"status": "success", "devices": devices}
        else:
            return {"status": "error", "message": result.stderr or f"Command failed with exit code {result.returncode}"}
    
    except subprocess.TimeoutExpired:
        return {"status": "error", "message": "Command timed out"}
    except FileNotFoundError:
        return {"status": "error", "message": "Bluetooth scanner executable not found"}
    except Exception as e:
        return {"status": "error", "message": str(e)}

def run_audio_executable(args):
    """Run the audio player executable"""
    exec_path = os.path.join(os.getcwd(), 'executables', 'bluetooth_audio_player.exe')
    try:
        result = subprocess.run([exec_path] + args, capture_output=True, text=True, timeout=30)
        
        if result.returncode == 0:
            # For --list command, parse the audio devices
            if args and args[0] == '--list':
                output_lines = result.stdout.strip().split('\n')
                devices = []
                
                for line in output_lines:
                    line = line.strip()
                    
                    if ':' in line and not line.startswith('Available') and not line.startswith('----------------------------------------') and not line.startswith('No audio devices'):
                        # Parse format: "0: Device Name [BLUETOOTH]"
                        parts = line.split(':', 1)
                        if len(parts) > 1:
                            try:
                                device_index = int(parts[0].strip())
                                device_info = parts[1].strip()
                                
                                is_bluetooth = '[BLUETOOTH]' in device_info
                                clean_name = device_info.replace(' [BLUETOOTH]', '')
                                
                                devices.append({
                                    'id': device_index,
                                    'name': clean_name,
                                    'type': 'bluetooth' if is_bluetooth else 'audio',
                                    'bluetooth': is_bluetooth
                                })
                            except ValueError:
                                continue  # Skip if the index is not a number
                
                return {"status": "success", "devices": devices}
            else:
                # For play commands, return success message
                return {"status": "success", "message": result.stdout.strip()}
        else:
            return {"status": "error", "message": result.stderr or f"Command failed with exit code {result.returncode}"}
    
    except subprocess.TimeoutExpired:
        return {"status": "error", "message": "Command timed out"}
    except FileNotFoundError:
        return {"status": "error", "message": "Audio player executable not found"}
    except Exception as e:
        return {"status": "error", "message": str(e)}

# Main route
@app.route('/')
def index():
    return render_template('index.html')

# Lab routes
@app.route('/lab01')
def lab01():
    return render_template('lab01.html')

@app.route('/lab02')
def lab02():
    return render_template('lab02.html')

@app.route('/lab03')
def lab03():
    return render_template('lab03.html')

@app.route('/lab04')
def lab04():
    return render_template('lab04.html')

@app.route('/lab05')
def lab05():
    return render_template('lab05.html')

@app.route('/lab06')
def lab06():
    return render_template('lab06.html')

# USB Management Routes
@app.route('/listUsbDevices')
def list_usb_devices():
    return jsonify(run_usb_executable(['--list']))

@app.route('/disableUsbDevice', methods=['POST'])
def disable_usb_device():
    data = request.get_json()
    device_id = data.get('device_id', '')
    if not device_id:
        return jsonify({"status": "error", "message": "Device ID is required"})
    
    return jsonify(run_usb_executable(['--disable', device_id]))

@app.route('/enableUsbDevice', methods=['POST'])
def enable_usb_device():
    data = request.get_json()
    device_id = data.get('device_id', '')
    if not device_id:
        return jsonify({"status": "error", "message": "Device ID is required"})
    
    return jsonify(run_usb_executable(['--enable', device_id]))

@app.route('/ejectUsbDrive', methods=['POST'])
def eject_usb_drive():
    data = request.get_json()
    drive_letter = data.get('drive', '')
    if not drive_letter:
        return jsonify({"status": "error", "message": "Drive letter is required"})
    
    # Ensure drive letter is just a single character
    if len(drive_letter) > 1:
        drive_letter = drive_letter[0]
    
    return jsonify(run_usb_executable(['--eject', drive_letter]))

@app.route('/forceEjectUsbDrive', methods=['POST'])
def force_eject_usb_drive():
    data = request.get_json()
    drive_letter = data.get('drive', '')
    if not drive_letter:
        return jsonify({"status": "error", "message": "Drive letter is required"})
    
    # Ensure drive letter is just a single character
    if len(drive_letter) > 1:
        drive_letter = drive_letter[0]
    
    return jsonify(run_usb_executable(['--force-eject', drive_letter]))

@app.route('/getUsbInfo')
def get_usb_info():
    # For now, just return the list of devices as info
    return jsonify(run_usb_executable(['--list']))

# Bluetooth Management Routes
@app.route('/listBluetoothDevices')
def list_bluetooth_devices():
    return jsonify(run_bluetooth_executable())

@app.route('/toggleBluetooth', methods=['POST'])
def toggle_bluetooth():
    # The executable doesn't support toggle, so return a message
    return jsonify({"status": "success", "message": "Bluetooth toggle functionality is not available in the current executable"})

# Audio Management Routes
@app.route('/listAudioDevices')
def list_audio_devices():
    return jsonify(run_audio_executable(['--list']))

@app.route('/setAudioVolume', methods=['POST'])
def set_audio_volume():
    # The executable doesn't support volume control, so return a message
    data = request.get_json()
    device = data.get('device', '')
    volume = data.get('volume', 50)
    return jsonify({"status": "success", "message": f"Setting volume for {device} to {volume}% is not supported by the executable"})

@app.route('/toggleAudioMute', methods=['POST'])
def toggle_audio_mute():
    # The executable doesn't support mute, so return a message
    data = request.get_json()
    device = data.get('device', '')
    return jsonify({"status": "success", "message": f"Toggling mute for {device} is not supported by the executable"})

@app.route('/getAudioInfo')
def get_audio_info():
    return jsonify(run_audio_executable(['--list']))

@app.route('/playAudio', methods=['POST'])
def play_audio():
    data = request.get_json()
    audio_file = data.get('audio_file', '')
    device_index = data.get('device_index', None)

    if not audio_file:
        return jsonify({"status": "error", "message": "Audio file is required"})

    if device_index is not None:
        cmd = [audio_file, str(device_index)]
    else:
        cmd = [audio_file]

    return jsonify(run_audio_executable(cmd))

# Serve static files
@app.route('/static/<path:filename>')
def static_files(filename):
    return send_from_directory('static', filename)

if __name__ == '__main__':
    app.run(debug=True, port=8081)