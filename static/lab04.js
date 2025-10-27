function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}
const glow = document.getElementById('fx');
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