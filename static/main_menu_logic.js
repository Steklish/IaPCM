const coordsLabel = document.getElementById('cursor-coords');
const screenLabel = document.getElementById('screen-dimensions');
const lEye = document.getElementById('l_eye');
const rEye = document.getElementById('r_eye');
const glow = document.getElementById('glow');
const viniette = document.getElementById('viniette');
const container = document.querySelector('.parallax-container');
const l3_icon = document.getElementById('l3_icon');

function updateScreenDimensions() {
    screenLabel.textContent = `Screen Dimensions: ${window.innerWidth}x${window.innerHeight}`;
}

updateScreenDimensions();

const lab_1_button = document.getElementById("l1_button");
const lab_2_button = document.getElementById("l2_button");
const lab_3_button = document.getElementById("l3_button");

lab_2_button.onmouseenter = () =>
{
    container.style.filter = "blur(15px)"

}

lab_2_button.onmouseleave = () =>
{
    container.style.filter = "blur(0px)"
}

lab_3_button.onmouseenter = () =>
{
    container.style.filter = "saturate(0%)"
    l3_icon.style.opacity = "1"
}

lab_3_button.onmouseleave = () =>
{
    container.style.filter = "saturate(100%)"
    l3_icon.style.opacity = "0"
}

let button_list = [lab_1_button, lab_2_button];


function hide_except_self(self_id){
    for (let i = 0; i < button_list.length; i++){
        if (button_list[i] != self_id){
            button_list[i].style.visibility = "hidden";
        }
    }
}

function show_all_buttons(){
    for (let i = 0; i < button_list.length; i++){
        button_list[i].style.visibility = "visible";
    }
}

const hide_delay = 0;


// for (let i = 0; i < button_list.length; i++){
//     button_list[i].onmouseenter = () => {
//         hide_except_self(button_list[i]);
//         //     container.style.filter = "blur(15px)"
//         // setTimeout(() => {
//         // }, hide_delay); // delay in milliseconds
//     };

//     button_list[i].onmouseleave = () => {
//         show_all_buttons();
//         // setTimeout(() => {
//         // }, hide_delay); // delay in milliseconds
//     };
// }

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