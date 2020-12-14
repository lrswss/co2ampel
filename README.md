## CO2-Ampel

Yet another CO2-Ampel based on an ESP8266 (Wemos D1 mini) to continuously
measure CO2 concentration indoor with a [SCD30](https://www.sensirion.com/en/environmental-sensors/carbon-dioxide-sensors/carbon-dioxide-sensors-co2/) sensor accompanied by a [BME280](https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/)
for temperature, humidity and air pressure readings. Current air condition
(good, medium, critial, bad) is shown using an array of WS2812 LEDs lit up
in green, yellow or red.

Unlike other devices this one runs on two 18650 LiIon-batteries, offers
a sophisticated webinterface for configuration and live sensor readings.
Sensor data can be logged to local flash (LittleFS), retrieved RESTful,
published using MQTT or even transmitted with LoRaWAN if a
[RFM95 shield](https://github.com/hallard/WeMos-Lora) is installed.

## Hardware

First you need to get a [suitable housing](https://www.ovomaltine.de/produkte/ovomaltine-pulver-dose)
for your mobile CO2-Ampel. More coming soon...

## Compiling the firmware for the Wemos D1 mini

First adjust settings in `config.h` according to your needs and hardware setup.
[Dont' forget to add support for ESP8266 based boards](https://github.com/esp8266/Arduino)
to your Arduino IDE using the boards manager.

Before trying to compile and flashing the sketch to your `Wemos D1 R1` board make
sure that the following libraries have been installed. Don't be too intimidated
by the long list, the [Arduino IDE library manager](https://www.arduino.cc/en/Guide/Libraries)
will give you a hand.

* [EPS8266Wifi](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WiFi)
* [ESP8266WebServer](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266WebServer)
* [ESP8266HTTPUpdateServer](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266HTTPUpdateServer)
* [ESP8266mDNS](https://github.com/esp8266/Arduino/tree/master/libraries/ESP8266mDNS)
* [uEEPROMLib](https://github.com/Naguissa/uEEPROMLib)
* [RTClib](https://github.com/adafruit/RTClib)
* [Adafruit_NeoPixle](https://github.com/adafruit/Adafruit_NeoPixel)
* [PubSubClient](https://github.com/knolleary/pubsubclient/releases)
* [ArduinoJson](https://arduinojson.org/)
* [BME280I2C](https://github.com/finitespace/BME280)
* [MCCI LoRaWAN LMIC Library](https://github.com/mcci-catena/arduino-lmic)
* [LittleFS](https://github.com/esp8266/Arduino/tree/master/libraries/LittleFS)
* [NTPClient](https://github.com/arduino-libraries/NTPClient)
* [RunningMedian](https://github.com/RobTillaart/RunningMedian)
* [Time](https://github.com/PaulStoffregen/Time)
* [Timezone](https://github.com/JChristensen/Timezone)

## Contributing

Pull requests are welcome! For major changes, please open an issue first to
discuss what you would like to change.

## License

Copyright (c) 2020 Lars Wessels.
This software was published under the Apache License 2.0
Please check the [license file](LICENSE).
