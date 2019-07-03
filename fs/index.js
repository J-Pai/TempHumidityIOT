const HOST = window.location.hostname === "127.0.0.1" || window.location.hostname === "localhost" ?
    "http://192.168.0.104" : "";
console.log(HOST);
/**
 * Make request for Temperature and Humidity and apply to HTML.
 */
function requestCurrTempHumi() {
    $.ajax({
        url: HOST + "/dht"
    }).done(function(data) {
        let infoTempHumi = JSON.parse(data);
        console.log(infoTempHumi);
        let timestamp = Date(infoTempHumi.d);
        let temp = parseFloat(infoTempHumi.t.substring(0, infoTempHumi.t.length - 1));
        let tempType = infoTempHumi.t.includes("F") ? "F" : "C";
        let humi = parseFloat(infoTempHumi.h);

        $('currTemp')

        console.log(timestamp, temp, tempType, humi);
    }).fail(function(xhr, status) {
        console.log(xhr, status);
    });
}

requestCurrTempHumi();

$(document).ready(function() {
});