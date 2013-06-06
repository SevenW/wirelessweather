wirelessweather
===============

receive signals from wireless weather sensors from Fine Offset, Alecto and look alikes with standard and modified Jeenodes and arduino.

WeatherStationFSKv2 is able to receive signals from Fine Offset, Alecto, LaCrosse IT+ and WS1600
weather stations using the RFM12B receiver on standard JeeNodes. It can only operate at the frequency
band of the RFM12B. In Europe that will be most of the time 868MHz, for which the code is applicable.
But alse US 915MHz and globally 433MHz can be used. This requires an adaptation in the setting of the
RFM12B frequency.

weatherstationFSK uses an adapted version of the RF12 drivers.
The adapted version isd part of the rf12mods branch of jcw/jeelib in github:
https://github.com/jcw/jeelib/tree/rf12mods
It is adapted such that fixed length packages can be received.
A external function rf12_setFixedLength(uint8_t packet_len) is provided.
With packet_len > 0 exactly this maximum number of bytes is received, and
no checks on  rf12_len (= rf12_buf[2]) will be done.
With packet_len = 0, the driver fucntions as before and is intended to
receive standard RF12-driver packages.

To install rf12mods: (not recommended for JeeNode micro (JNu) users)
1. rename ...\Arduino\Libraries\jeelib to ...\Arduino\Libraries\jeelib-original
2. download jeelib from the rf12mods branch
      https://github.com/jcw/jeelib/tree/rf12mods
3. copy this jeelib version to ...\Arduino\Libraries\jeelib

Untested, but alternatively, only the files RF12.h and RF12.cpp can be replaced in:
   ...\Arduino\Libraries\jeelib

To install the wheatherstation sketch
1. create a folder weatherstationFSK in the arduino base folder
      arduino/weatherstationFSKv2
2. copy weatherstationFSKv2.ino to this folder
3. when already active, close the arduino IDE
      added libraries and projects are only seen at startup
4. start the arduino IDE, and select the sketch weatherstationFSKv2

Use:
The amount of information printed can be controlled by setting some #defines 
in the first view lines of the weatherstationFSKv2.ino:
    //Define the printed output
    //#define LOGRAW 1 //comment = disable, uncomment = enable logging received package group_id 212 (0xD4)
    //#define LOGPKT 1 //comment = disable, uncomment = enable logging unique package passing crc
    #define LOGDCF 1 //comment = disable, uncomment = enable updating time and logging of DCF77 values
    #define LOGDAT 1 //comment = disable, uncomment = enable logging of parsed sensor data


Rinie added IT+ decoding that uses the same speed as FineOffet
Added WS1600 decoding that uses half the speed.
See http://fredboboss.free.fr/tx29/index.php?lang=en
#define ITPLUS1600 for WS1600 decoding