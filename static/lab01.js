
function updateBatteryInfo() {
    axios.get('/getCharge')
        .then(function (response) {
            document.getElementById('charge').innerText = 'Charge: ' + response.data.message + '%';
        })
        .catch(function (error) {
            console.log(error);
        });

    axios.get('/getStatus')
        .then(function (response) {
            document.getElementById('status').innerText = 'Status: ' + response.data.message;
        })
        .catch(function (error) {
            console.log(error);
        });

    axios.get('/getPowerMode')
        .then(function (response) {
            document.getElementById('power-mode').innerText = 'Power Mode: ' + response.data.message;
            const gif = document.getElementById('power-gif');
            const loader = document.getElementById('power-loader');
            for (const sheet of document.styleSheets) {
                try {
                    for (const rule of sheet.cssRules) {
                    if (rule.selectorText === '.loader::after') {
                        // rule.style.content = '"New content"';
                        rule.style.animation = 'none';
                        break;
                    }
                    }
                } catch(e) {
                    // Ignore cross-origin stylesheet errors
                }
            }
            
            if (response.data.message === "Online") {
                gif.style.display = 'block';
                for (const sheet of document.styleSheets) {
                try {
                    for (const rule of sheet.cssRules) {
                    if (rule.selectorText === '.loader::after') {
                        rule.style.animation = 'full 5s ease-in-out infinite';
                        break;
                    }
                    }
                } catch(e) {
                    // Ignore cross-origin stylesheet errors
                }
            }
                
            } else {
                gif.style.display = 'none';
                loader.style.animation = "none";
            }
        })
        .catch(function (error) {
            console.log(error);
        });

    axios.get('/getInfo')
        .then(function (response) {
            document.getElementById('info').innerText = 'Info: ' + response.data.message;
        })
        .catch(function (error) {
            console.log(error);
        });

    axios.get('/getTimeLeft')
        .then(function (response) {
            let seconds = response.data.message;
            if (seconds < 0) {
                document.getElementById('time-left').innerText = 'Time Left: Unknown';
            } else {
                let hours = Math.floor(seconds / 3600);
                let minutes = Math.floor((seconds % 3600) / 60);
                document.getElementById('time-left').innerText = 'Time Left: ' + hours + 'h ' + minutes + 'm';
            }
        })
        .catch(function (error) {
            console.log(error);
        });

    axios.get('/isEco')
        .then(function (response) {
            document.getElementById('eco-mode').innerText = 'Eco Mode: ' + response.data.message;
        })
        .catch(function (error) {
            console.log(error);
        });
}

function sleep() {
    axios.get('/sleep')
        .then(function (response) {
            console.log(response.data);
        })
        .catch(function (error) {
            console.log(error);
        });
}

function hibernate() {
    axios.get('/hibernate')
        .then(function (response) {
            console.log(response.data);
        })
        .catch(function (error) {
            console.log(error);
        });
}

setInterval(updateBatteryInfo, 1000);
updateBatteryInfo();
