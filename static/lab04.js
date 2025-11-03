function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}

const glow = document.getElementById('fx');
const coordsLabel = document.getElementById('cursor-coords');
const screenLabel = document.getElementById('screen-dimensions');
const container = document.querySelector('.parallax-container');
const head = document.getElementById("head");

// Camera control elements
const getCameraInfoBtn = document.getElementById('get-camera-info');
const takePhotoBtn = document.getElementById('take-photo');
const startRecordingBtn = document.getElementById('start-recording');
const stopRecordingBtn = document.getElementById('stop-recording');
const toggleCovertBtn = document.getElementById('toggle-covert');
const cameraInfoDiv = document.getElementById('camera-info');
const statusMessageDiv = document.getElementById('status-message');
const capturedFilesDiv = document.getElementById('captured-files');

updateScreenDimensions();

// Camera control functions
async function getCameraInfo() {
    try {
        const response = await axios.get('/isCameraOpen');
        if (response.data.status === 200) {
            const isOpen = response.data.message === 'true';
            let infoHtml = `<h3>Camera Status</h3>`;
            infoHtml += `<p>Camera is ${isOpen ? 'OPEN' : 'CLOSED'}</p>`;
            if (isOpen) {
                // Get additional camera information via a new endpoint
                const infoResponse = await axios.get('/getCameraInfo');
                if (infoResponse.data.status === 200) {
                    const camInfo = infoResponse.data.message;
                    infoHtml += `<p>Camera Index: ${camInfo.index}</p>`;
                    infoHtml += `<p>Resolution: ${camInfo.width}x${camInfo.height}</p>`;
                    infoHtml += `<p>FPS: ${camInfo.fps}</p>`;
                    infoHtml += `<p>Name: ${camInfo.name}</p>`;
                }
            }
            cameraInfoDiv.innerHTML = infoHtml;
        } else {
            showStatus('Error getting camera info', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error getting camera info', 'error');
    }
}

async function takePhoto() {
    try {
        const response = await axios.get('/takeFrame');
        if (response.data.status === 200) {
            showStatus(`Photo saved: ${response.data.message}`, 'success');
            updateCapturedFiles();
        } else {
            showStatus('Error taking photo', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error taking photo', 'error');
    }
}

async function startRecording() {
    try {
        const filename = `static/output/recording_${Date.now()}.avi`;
        const response = await axios.post('/startRecording', null, {
            params: { filename: filename, fps: 30 }
        });
        if (response.status === 200) {
            showStatus(response.data.message, 'success');
        } else {
            showStatus('Error starting recording', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error starting recording', 'error');
    }
}

async function stopRecording() {
    try {
        const response = await axios.get('/stopRecording');
        if (response.data.status === 200) {
            showStatus(response.data.message, 'success');
            updateCapturedFiles();
        } else {
            showStatus(response.data.message, 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error stopping recording', 'error');
    }
}

let covertModeEnabled = false;

async function toggleCovertMode() {
    try {
        // Perform 1-second covert recording on server side
        const filename = `static/output/covert_recording_${Date.now()}.avi`;
        const response = await axios.post('/oneSecondCovertRecording', null, {
            params: { filename: filename, fps: 30 }
        });
        
        if (response.data.status === 200) {
            showStatus('Covert recording completed', 'success');
            updateCapturedFiles();
            
            // Redirect to home page after covert recording
            setTimeout(() => {
                window.location.href = '/';
            }, 500); // Small delay to show message before redirect
        } else {
            showStatus('Error during covert recording', 'error');
        }
    } catch (error) {
        console.error('Error:', error);
        showStatus('Error during covert recording', 'error');
    }
}

async function updateCapturedFiles() {
    try {
        const response = await axios.get('/getCapturedFiles');
        if (response.data.status === 200) {
            const files = response.data.files;
            let filesHtml = '<h4>Saved Files:</h4>';
            let imagePreviewsHtml = '<div class="image-container">';
            
            if (files.length > 0) {
                files.forEach(file => {
                    // Add file to the list
                    filesHtml += `<div class="file-item">${file.name} (${(file.size / 1024).toFixed(2)} KB)</div>`;
                    
                    // Check if it's an image file and add preview
                    const imageExtensions = ['.jpg', '.jpeg', '.png', '.bmp', '.gif'];
                    const fileExt = file.extension.toLowerCase();
                    if (imageExtensions.includes(fileExt)) {
                        const imagePath = `/static/output/${file.name}`;
                        imagePreviewsHtml += `
                        <div class="image-preview-container">
                            <img src="${imagePath}" class="image-preview" alt="${file.name}" title="${file.name}">
                            <div class="image-filename">${file.name}</div>
                        </div>`;
                    }
                });
            } else {
                filesHtml += '<div class="file-item">No files captured yet</div>';
            }
            
            imagePreviewsHtml += '</div>';
            capturedFilesDiv.innerHTML = filesHtml + imagePreviewsHtml;
        } else {
            console.error('Error getting captured files');
        }
    } catch (error) {
        console.error('Error:', error);
    }
}

function showStatus(message, type) {
    statusMessageDiv.textContent = message;
    statusMessageDiv.style.backgroundColor = type === 'success' ? 'rgba(0, 255, 0, 0.2)' : 'rgba(255, 0, 0, 0.2)';
    statusMessageDiv.style.display = 'block';
    
    // Hide message after 5 seconds
    setTimeout(() => {
        statusMessageDiv.style.display = 'none';
    }, 5000);
}

// Event listeners for camera controls
if (getCameraInfoBtn) {
    getCameraInfoBtn.addEventListener('click', getCameraInfo);
}

if (takePhotoBtn) {
    takePhotoBtn.addEventListener('click', takePhoto);
}

if (startRecordingBtn) {
    startRecordingBtn.addEventListener('click', startRecording);
}

if (stopRecordingBtn) {
    stopRecordingBtn.addEventListener('click', stopRecording);
}

if (toggleCovertBtn) {
    toggleCovertBtn.addEventListener('click', toggleCovertMode);
}

// Initialize captured files display
updateCapturedFiles();

window.addEventListener('resize', updateScreenDimensions);

window.addEventListener('mousemove', (event) => {
    const rect = container.getBoundingClientRect();
    const mouseX = event.clientX - rect.left;
    const mouseY = event.clientY - rect.top;

    const centerX = container.clientWidth / 2;
    const centerY = container.clientHeight / 2;

    const max_deviation = rect.height / 5

    const dx = mouseX - centerX;
    const dy = mouseY - centerY;

    const dy2 = dy * dy / 50;
    const dy3 = Math.cbrt(dy * 2);

    const distance = Math.sqrt(dx * dx + dy * dy);
    let angle = (Math.atan2(dy, dx) * (180 / Math.PI)).toFixed(2);

    const max_distance = Math.sqrt(Math.pow(rect.width / 2, 2) + Math.pow(rect.height / 2, 2));
    const normalized_distance = distance / max_distance;

    container.style.transform = `translateX(${-dx / 100}px) translateY(${-dy / 100}px)`;
    
    glow.style.opacity = (1.0 - normalized_distance) * 2;
    head.style.top = centerY + dy3 / 50 - head.offsetHeight / 2 + 'px';
    head.style.left = centerX - head.offsetWidth / 2 + Math.pow(dx / 80, 2)  * Math.sign(dx) + 'px'
    
    glow.style.top = head.style.top
    glow.style.left = head.style.left
    
    coordsLabel.textContent = `Cursor Position: ${mouseX.toFixed(0)}, ${mouseY.toFixed(0)} | Distance: ${distance.toFixed(2)}px, Angle: ${angle}°  >>  ${normalized_distance}`;
});