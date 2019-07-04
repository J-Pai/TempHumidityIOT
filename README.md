# Temp and Humidity Tracker IOT

## Overview
Embedded code using Mongoose OS which keeps track of current temperature and humidity. Device used is the NodeMCU DevKit version 1.0.

This DevKit contains a built in ESP8266 Wifi Chip.

Temperature and Humidity history is stored in onboard flash. Hourly scheduled measurements will be stored up to 10 days. Can be accessed/dumped by accessing embedded webserver.

Humidity/Temperature sensor used is a DHT11.

## Web Interface
- Displays current temperature and humidity values.
- Displays a history of recorded temperature and humidity values.
- CSS Framework: [Semantic UI (2.4.2)](https://semantic-ui.com/)
- Chart Framework: [Chart.js (2.8.0)](https://www.chartjs.org/)

## Build, Flash, Use
1) `mos build` - Build code and generate fw.zip file.
2) `mos flash` - Flash onto ESP8266 board.
3) `mos wifi <SSID> <PASSWD>` - Setup Wifi connection.
4) `mos console` - Watch for SSID and assigned IP address to connect to.

## Configuration Options
```
mos wifi <SSID> <PASSWD>
mos config-set app.dht.fahrenheit=false
mos config-set app.silent=false
mos call Sys.GetInfo
mos console
```

## Documentation
### Hardware/OS Documentation
- https://github.com/nodemcu/nodemcu-devkit-v1.0
- https://github.com/cesanta/mongoose-os
- https://mongoose-os.com/docs/mongoose-os/quickstart/setup.md
### Web App Documentation
- https://semantic-ui.com/introduction/getting-started.html
- https://www.chartjs.org/docs/latest/
- https://momentjs.com/docs/
