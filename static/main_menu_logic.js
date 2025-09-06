const coordsLabel = document.getElementById('cursor-coords');
const screenLabel = document.getElementById('screen-dimensions');
const lEye = document.getElementById('l_eye');
const rEye = document.getElementById('r_eye');
const glow = document.getElementById('glow');
const viniette = document.getElementById('viniette');
const container = document.querySelector('.parallax-container');

function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}

updateScreenDimensions();

window.addEventListener('resize', updateScreenDimensions);

window.addEventListener('mousemove', (event) => {
    const rect = container.getBoundingClientRect();
    const mouseX = event.clientX - rect.left;
    const mouseY = event.clientY - rect.top;

    const centerX = container.clientWidth / 2;
    const centerY = container.clientHeight / 2;

    
    const max_deviation = rect.height / 5
    
    
    const base_vertical = rect.height / 4
    const abs_base_horisontal = rect.width / 4.6
    
    const dx = mouseX - centerX;
    const dy = mouseY - base_vertical;

    const distance = Math.sqrt(dx * dx + dy * dy);
    let angle = (Math.atan2(dy, dx) * (180 / Math.PI)).toFixed(2);

    const max_distance = Math.sqrt(Math.pow(rect.width / 2, 2) + Math.pow(rect.height / 2, 2));
    const normalized_distance = distance / max_distance;

    const base_scale = 0.1;
    const scale_factor = 0.0008;
    const new_scale = base_scale + distance * distance * scale_factor * scale_factor;

    lEye.style.transform = `scaleX(${0.8 + normalized_distance * 0.2})`;
    rEye.style.transform = `scaleX(${0.8 + normalized_distance * 0.2})`;
    
    lEye.style.scale = new_scale;
    rEye.style.scale = new_scale;
    
    glow.style.opacity = (1.0 - normalized_distance) * 2;
    viniette.style.opacity = (1.0 - normalized_distance) / 1.5;

    container.style.transform = `translateX(${-dx / 100}px) translateY(${-dy / 100}px)`;

    // lEye.style.left = (mouseX - lEye.offsetWidth / 2) + 'px';
    // lEye.style.top = (mouseY - lEye.offsetHeight / 2) + 'px';
    lEye.style.left = -abs_base_horisontal + dx / 20 + 'px'
    lEye.style.top = base_vertical + Math.max(-max_deviation, Math.min(max_deviation, dy/10)) + 'px'
    rEye.style.left = abs_base_horisontal + dx / 20 + 'px'
    rEye.style.top =  base_vertical + Math.max(-max_deviation, Math.min(max_deviation, dy/10)) + 'px'
    
    
    coordsLabel.textContent = `Cursor Position: ${mouseX.toFixed(0)}, ${mouseY.toFixed(0)} | Distance: ${distance.toFixed(2)}px, Angle: ${angle}°  >>  ${normalized_distance}`;
});