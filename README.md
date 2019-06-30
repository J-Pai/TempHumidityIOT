# Temp and Humidity Tracker IOT
## Documentation
- https://github.com/nodemcu/nodemcu-devkit-v1.0
- https://github.com/cesanta/mongoose-os
- https://mongoose-os.com/docs/mongoose-os/quickstart/setup.md

## Overview
Embedded code using Mongoose OS which keeps track of current temperature and humidity. Device used is the NodeMCU DevKit version 1.0.

This DevKit contains a built in ESP8266 Wifi Chip.

Temperature and Humidity history is stored in onboard flash. Hourly scheduled measurements will be stored up to 10 days. Can be accessed/dumped by accessing embedded webserver.

## Configuration Options
```
mos wifi <SSID> <PASSWD>
mos config-set app.dht.fahrenheit=1
mos config-set app.silent=true
mos call Sys.GetInfo
mos console
```
