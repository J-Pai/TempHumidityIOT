const HOST = window.location.hostname === "127.0.0.1" || window.location.hostname === "localhost" ?
    "http://192.168.0.104" : "";
console.log("HOST:", HOST);

window.chartColors = {
    red: 'rgb(255, 99, 132)',
    orange: 'rgb(255, 159, 64)',
    yellow: 'rgb(255, 205, 86)',
    green: 'rgb(75, 192, 192)',
    blue: 'rgb(54, 162, 235)',
    purple: 'rgb(153, 102, 255)',
    grey: 'rgb(201, 203, 207)',
};

window.chartBgColors = {
    red: 'rgb(255, 99, 132, 0.2)',
    orange: 'rgb(255, 159, 64, 0.2)',
    yellow: 'rgb(255, 205, 86, 0.2)',
    green: 'rgb(75, 192, 192, 0.2)',
    blue: 'rgb(54, 162, 235, 0.2)',
    purple: 'rgb(153, 102, 255, 0.2)',
    grey: 'rgb(201, 203, 207, 0.2)',
};


$(document).ready(function () {
    let tempType = "";

    const tempDataset = {
        label: 'Temperature',
        borderColor: window.chartColors.red,
        backgroundColor: window.chartBgColors.red,
        fill: true,
        yAxisID: 'y-axis-1',
        data: [],
    };
    const humiDataset = {
        label: 'Humidity',
        borderColor: window.chartColors.blue,
        backgroundColor: window.chartBgColors.blue,
        fill: true,
        yAxisID: 'y-axis-2',
        data: [],
    };

    const ctx = $("#histTempHumi")[0].getContext("2d");
    const myChart = new Chart(ctx, {
        type: "line",
        data: {
            labels: [],
            datasets: [
                tempDataset,
                humiDataset,
            ]
        },
        options: {
            title: {
                display: true,
                text: "Historical Temperature and Humidity"
            },
            scales: {
                yAxes: [{
                    position: "left",
                    id: "y-axis-1",
                    scaleLabel: {
                        labelString: "Temperature",
                        display: true,
                    }
                }, {
                    position: "right",
                    id: "y-axis-2",
                    scaleLabel: {
                        labelString: "Humidity (%)",
                        display: true,
                    }
                }],
            },
        },
    });

    /**
     * Make request for Temperature and Humidity and apply to HTML.
     */
    function requestCurrTempHumi() {
        $.ajax({
            url: HOST + "/dht"
        }).done(function (data) {
            const infoTempHumi = JSON.parse(data);
            const timestamp = new Date(infoTempHumi.d);
            const temp = parseFloat(infoTempHumi.t.substring(0, infoTempHumi.t.length - 1));
            tempType = infoTempHumi.t.includes("F") ? "&#176;F" : "&#176;C";
            const humi = parseFloat(infoTempHumi.h);
            $("#currTemp").html(temp + tempType);
            $("#currHumi").html(humi + "%");
            $("#currTimestamp").html(timestamp.toString());
        }).fail(function (xhr, status) {
            console.log(xhr, status);
        });
    }

    /**
     * Request and Update Temperature and Humidity history graph.
     */
    function requestHistTempHumi() {
        $.ajax({
            url: HOST + "/dht/history"
        }).done(function (data) {
            const jsonStr = "[" + data + "]";
            console.log(data);
            const histTempHumi = JSON.parse(jsonStr);

            myChart.data.labels = [];
            tempDataset.data = [];
            humiDataset.data = [];

            histTempHumi.forEach(element => {
                const timestamp = moment(element.d).format("YYYY-MM-DD HH:mm:ss");
                const temp = parseFloat(element.t.substring(0, element.t.length - 1));
                myChart.options.scales.yAxes[0].scaleLabel.labelString = element.t.includes("F") ?
                    "Temperature (" + String.fromCharCode(176) + "F)" :
                    "Temperature (" + String.fromCharCode(176) + "C)";
                const humi = parseFloat(element.h);
                myChart.data.labels.push(timestamp);
                tempDataset.data.push(temp);
                humiDataset.data.push(humi);
            });
            myChart.update();
        }).fail(function (xhr, status) {
            console.log(xhr, status);
        });
    }

    requestCurrTempHumi();
    requestHistTempHumi();
});