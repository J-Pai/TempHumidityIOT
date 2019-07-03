const HOST = window.location.hostname === "127.0.0.1" || window.location.hostname === "localhost" ?
    "http://192.168.0.104" : "";
console.log("HOST:", HOST);
/**
 * Make request for Temperature and Humidity and apply to HTML.
 */
function requestCurrTempHumi() {
    $.ajax({
        url: HOST + "/dht"
    }).done(function(data) {
        const infoTempHumi = JSON.parse(data);
        const timestamp = Date(infoTempHumi.d);
        const temp = parseFloat(infoTempHumi.t.substring(0, infoTempHumi.t.length - 1));
        const tempType = infoTempHumi.t.includes("F") ? "F" : "C";
        const humi = parseFloat(infoTempHumi.h);

        $('#currTemp').html(temp + "&#176;" + tempType);
        $('#currHumi').html(humi + "%");
        $('#currTimestamp').html(timestamp.toString());

        console.log(timestamp, temp, tempType, humi);
    }).fail(function(xhr, status) {
        console.log(xhr, status);
    });
}

$(document).ready(function() {
    function requestHistTempHumi() {
        $.ajax({
            url: HOST + "/dht/history"
        }).done(function(data) {
            const jsonStr = "[" + data + "]";
            const histTempHumi = JSON.parse(jsonStr);
            console.log(histTempHumi);
        }).fail(function(xhr, status) {
            console.log(xhr, status);
        });

    }
    requestCurrTempHumi();
    requestHistTempHumi();
});