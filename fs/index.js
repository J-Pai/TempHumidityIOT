$(document).ready(function() {
    // Make request for current temperature and humidity.
    $.ajax({
        url: "http://192.168.0.104/dht"
    }).done(function(data) {
        console.log(data);
    }).fail(function(xhr, status) {
        console.log(xhr, status);
    });
    console.log('READY!');
});