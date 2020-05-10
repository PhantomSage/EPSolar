# Overview 

This is a fork https://github.com/dpoulson/EPSolar , lets see if I modify anything.
All credit to where it is due.

This project is to connect an EPSolar/EPEver Tracer Solar Charge Controller to an MQTT server using an ESP8266 based NodeMCU, and a RS485 modbus adapter. A lot of inspiration was taken from https://github.com/jaminNZx/Tracer-RS485-Modbus-Blynk, which takes the information and pushes to the Blynk service. With MQTT however, I can then incorporate it easily into my OpenHAB home automation, and also graph the readings using Grafana.

# Hardware
I am developing this on a NodeMCU v1, but see no reason why it won't work with other similar hardware.
The fork is executed on a ESP-01 as stated below.

# MQTT
By default, this will write to queues under:

/EPSolar/\<device ID\>/\<stat\>

# Usage

Copy the config.h-default to config.h and edit the parameters. If you have more than one solar module, then you can adjust the ID number in each client.publish line. Plans are to at some point make this a single variable, but that is in testing.

# Incorporating into OpenHAB

You will need the MQTT binding installed and configured to connect to the same MQTT instance. Once that is done, you can create an items file similar to this:

     Number EPSolar_Temp "Temperature [%.2f °C]" { mqtt="<[mymosquitto:EPSolar/1/ctemp:state:default" }

     Number EPSolar_BattVolt "Battery Voltage [%.2f V]" { mqtt="<[mymosquitto:EPSolar/1/bvoltage:state:default" }
     Number EPSolar_BattRemain "Battery Remaining [%.2f %]" { mqtt="<[mymosquitto:EPSolar/1/bremaining:state:default" }
     Number EPSolar_BattTemp "Battery Temp [%.2f °C]" { mqtt="<[mymosquitto:EPSolar/1/btemp:state:default" }

     Number EPSolar_LoadPower "Load Power [%.2f W]" { mqtt="<[mymosquitto:EPSolar/1/lpower:state:default" }
     Number EPSolar_LoadCurrent "Load Current [%.2f A]" { mqtt="<[mymosquitto:EPSolar/1/lcurrent:state:default" }

     Number EPSolar_PVVolt "PV Voltage [%.2f V]" { mqtt="<[mymosquitto:EPSolar/1/pvvoltage:state:default" }
     Number EPSolar_PVCurrent "PV Current [%.2f A]" { mqtt="<[mymosquitto:EPSolar/1/pvcurrent:state:default" }
     Number EPSolar_PVPower "PV Power [%.2f W]" { mqtt="<[mymosquitto:EPSolar/1/pvpower:state:default" }

     Number EPSolar_ChargeCurrent "Battery Charge Current [%.2f A]" { mqtt="<[mymosquitto:EPSolar/1/battChargeCurrent:state:default" }

     Number EPSolar_PVVoltMax "PV Voltage MAX (today) [%.2f V]" { mqtt="<[mymosquitto:EPSolar/1/stats_today_pv_volt_max:state:default" }
     Number EPSolar_PVVoltMin "PV Voltage MIN (today) [%.2f V]" { mqtt="<[mymosquitto:EPSolar/1/stats_today_pv_volt_min:state:default" }

This will store the results in these items. 

For full graphing, follow this tutorial: https://community.openhab.org/t/influxdb-grafana-persistence-and-graphing/13761


# The fork
This fork is made by PhanomSage, all credit where credit is due to the original deverlopers. 

## What this fork brings:
* multi-wifi, it has a list of different wifi networks that it will try to connect to
* wifi-scan, it will scan it's surrounding wifi networks to enable automatic detection and logging where it is. E.g. on a mobile installation like a boat, it will be able to tell the server side if it is in the harbour or not.
* More info/statistics in a hearbeat

##Potential future:
* inteligent loging to jiffs filesystem when no upstread is available
* SoftAP + STA, to allow for local wifi2modbus
* modbus2mqtt bridge
* manual on/off (coil handling over mqtt)
* full remote OTA, not only local OTA

# Real case
To aid others, I thought I write down my setup

## ESP8266 hardware
I use a ESP-01 from https://www.kjell.com/se/produkter/el-verktyg/arduino/moduler/wifi-modul-for-arduino-esp8266-p87947
and a Runrain OPEN-SMART USB To ESP8266 ESP-01 Wi-Fi Adapter Module W/ CH340G Driver" programmer.

## ESP8266 flashing

Installed arduino.
Set the board to generic 8266
Installed the libraries via library manager: modbusmaster
Do NOT use the SimpleTimer from the library manager.
Use this simple timer library: https://github.com/schinken/SimpleTimer

copy config.h-default to config.h and edit it to have your own config.
Build & Install, your done with the ESP part!

## RS485 Hardware
I use one of thiese to bridge between ESP8266 and RS485, very nifty little box.
https://www.tindie.com/products/plop211/epever-rs485-to-wifi-adaptor-new-revision/


# Reference material used:
https://github.com/kasbert/epsolar-tracer/blob/master/pyepsolartracer/registers.py


# Author

Darren Poulson \<darren.poulson@gmail.com\>

Further reading: http://60chequersavenue.net/wordpress/2017/07/pretty-solar-graphs/


