
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
            if (response.data.message === "Online") {
                gif.style.display = 'block';
            } else {
                gif.style.display = 'none';
            }
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
