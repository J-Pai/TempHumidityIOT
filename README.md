# Temp and Humidity Tracker IOT
## Documentation
- https://github.com/nodemcu/nodemcu-devkit-v1.0
- https://github.com/cesanta/mongoose-os
- https://mongoose-os.com/docs/mongoose-os/quickstart/setup.md

## Overview
Embedded code using Mongoose OS which keeps track of current temperature and humidity. Device used is the NodeMCU DevKit version 1.0.

This DevKit contains a built in ESP8266 Wifi Chip.

Temperature and Humidity history is stored in onboard flash. Hourly scheduled measurements will be stored up to 10 days. Can be accessed/dumped by accessing embedded webserver.

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
