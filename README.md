# new-harvest-incubator-tray-firmware
New Harvest Incubator Tray firmware.

Control the Incubator Tray and construct mobile app and web-interface. In addition to added libraries you will need:

* Arduino IDE to run the main tray_NewHarvest.ino sketch. 
* Suport for Adafruit ESP32 Feather, follow the instructions: https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/using-with-arduino-ide
* Add OneWire library from the Library manager inside Aruino IDE.

To control the tray run the [tray_NewHarvest.ino](https://github.com/IRNAS/new-harvest-incubator-tray-firmware/blob/master/tray_NewHarvest.ino) sketch. In the main sketch Blynk app is defined along with the web-interface. 

For detailed description of the control algorithm, read the readme section in the [Tray.h](https://github.com/IRNAS/new-harvest-incubator-tray-firmware/blob/master/Tray.h) file.

For the user manual see [Instructions](https://github.com/IRNAS/new-harvest-incubator-tray-firmware/blob/master/Instructions.pdf).
