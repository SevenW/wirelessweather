Wireless weather station receiver for Buienradar BR-1800 and Alecto WH3000/WH4000
=================================================================================

The application receives FSK messages of FineOffset weather stations. It decodes the WH1080 type statyions (a.o. Alecto WH3000, WH4000) as well Buienradar BR-1800 and FineOffset WH2300. This message type is also known as WH24.

The application connects to LAN and/or internet through WiFi and can be configured to make a secure MQTT conenctions over LAN/WAN. It also can connect directly to weather station API's on the internet:
- Wundergound: Weather.com PWS api / Weather Underground
- Windguru
- Domoticz
- MQTT message for exampe for processing in Node-Red

The application has been designed for operation at a remote weather station. It can be configured remotely through MQTT messages. Also the firmware can be updated OTA with annoucment over MQTT based on the work of Thorsten von Eicken (tve). See https://github.com/tve/esp32-secure-base.

Hardware and SDE
----------------
The hardware required is an ESP32 with Lora SX1276 or RFM96 868MhZ radio. Testing has been performed on the Heltec ESP32 Lora with OLED display. The Software builds have been performed with Visual Studio Code and PlatformIO.

PlatformIO build guidance
-------------------------
PlatformIO dependcy finder looses track in the esp32-secure-base, asyncTCP and espWiFIasyncClient libraries. The first `pio run` will fail just as the next ones. The receipe is:
1. repeat `pio run` until the first error is about an Async File Handler.
2. open folder .pio/lib_deps/heltec_usb (last part is the env being built)
3. remove AsyncTCP foder(s) here.
4. repeat the above.

