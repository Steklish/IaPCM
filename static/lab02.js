function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}
const glow_d = document.getElementById('glow_dynamic');
const glow = document.getElementById('glow_static');
const viniette = document.getElementById('vin');
const coordsLabel = document.getElementById('cursor-coords');
const screenLabel = document.getElementById('screen-dimensions');
const container = document.querySelector('.parallax-container');
const head = document.getElementById("head");

updateScreenDimensions();

window.addEventListener('resize', updateScreenDimensions);

window.addEventListener('mousemove', (event) => {
    const rect = container.getBoundingClientRect();
    const mouseX = event.clientX - rect.left;
    const mouseY = event.clientY - rect.top;

    const centerX = container.clientWidth / 2;
    const centerY = container.clientHeight / 2;

    const max_deviation = rect.height / 5

    const dx = mouseX - centerX;
    const dy = mouseY - centerY / 2;

    const dy2 = dy * dy / 50;

    const distance = Math.sqrt(dx * dx + dy * dy);
    let angle = (Math.atan2(dy, dx) * (180 / Math.PI)).toFixed(2);

    const max_distance = Math.sqrt(Math.pow(rect.width / 2, 2) + Math.pow(rect.height / 2, 2));
    const normalized_distance = distance / max_distance;

    container.style.transform = `translateX(${-dx / 100}px) translateY(${-dy / 100}px)`;
    glow.style.opacity = (1.0 - normalized_distance) * 2;
    glow_d.style.opacity = (0.8 - normalized_distance);
    viniette.style.opacity = (1.0 - normalized_distance) / 5;
    head.style.top = Math.max((centerY - head.offsetHeight / 2), (centerY + dy2 / 50 - head.offsetHeight / 2)) + 15 + 'px';
    glow_d.style.top = Math.max((centerY - head.offsetHeight / 2), (centerY + dy2 / 50 - head.offsetHeight / 2)) + 'px';
    head.style.left = centerX - head.offsetWidth / 2 + Math.pow(dx / 80, 2)  * Math.sign(dx) + 'px'

    head.style.transform = `scaleX(${Math.sqrt(0.8 + normalized_distance * 0.2)}) rotate(${Math.pow(dx / 250, 3) % 360}deg)`;

    coordsLabel.textContent = `Cursor Position: ${mouseX.toFixed(0)}, ${mouseY.toFixed(0)} | Distance: ${distance.toFixed(2)}px, Angle: ${angle}°  >>  ${normalized_distance}`;
});

// Function to fetch PCI devices and render table
async function renderPCIDevicesTable() {
    try {
        const response = await axios.get('/getPCIDevices');
        if (response.data.status !== 200) {
            throw new Error('Failed to fetch PCI devices');
        }
        const devices = response.data.devices;
        const right = document.getElementById("card__right");
        const left = document.getElementById("card__left");
        devices.forEach(dev => {
            console.log(dev);
            const item_l = `
            <div class="dev_g_${dev.id}">
                <div class="item">id</div>
                <div class="item">DevID</div>
                <div class="item">VenID</div>
                <div class="item"></div>
                <div class="item"></div>
                <div class="item"></div>
                <div class="item"></div>
            </div>
            `
            const item_r = `
            <div class="dev_g_${dev.id}">
                <div class="item">${dev.id}</div>
                <div class="item">${dev.DevID}</div>
                <div class="item">${dev.VenID}</div>
                <div class="item"></div>
                <div class="item"></div>
                <div class="item"></div>
                <div class="item"></div>
            </div>`

            left.innerHTML += item_r;
            right.innerHTML += item_l;
        });
        const cardTitle = document.getElementById("card__title");
        cardTitle.textContent = `PCI devices (${devices.length})`;
    } catch (err) {
        console.error('Error loading PCI devices:', err);
    }
}

// Call on page load
window.addEventListener('DOMContentLoaded', renderPCIDevicesTable);