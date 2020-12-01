// WeatherStation persisitent configuration
// Copyright (c) 2020 SevenWatt.com, all rights reserved

#include <Arduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <map>
//#include "weather.h"

#ifndef MAX_WS
#define MAX_WS 4
#endif

struct WSSetting
{
    unsigned long lastReported; //not serialized
    unsigned long lastSeen;     //not serialized
    //WSBase ws;
    WSBase *wsp;
    std::map<time_t, double> rainhist;
    std::map<time_t, double> windhist;
    std::map<time_t, double> gusthist;

    //burst handling for WSWH1080 derived class
    bool mreportable;
    //unsigned long burstStart;

    //serializeable for MQTT station configuration
    uint16_t wsID;
    uint16_t wsType;
    bool wunderground;
    char wuID[10];
    char wuPW[10];
    bool domoticz;
    char dzURL[50];
    int dzPort;
    bool dzSecure;
    char dzID[20];
    char dzPW[20];
    uint32_t dzTHidx;
    uint32_t dzWidx;
    uint32_t dzRidx;
    uint32_t dzLidx;
    uint32_t dzUVidx;
    bool windguru;
    char wgSalt[40];
    char wgUID[40];
    char wgPW[20];

    WSSetting() : mreportable(false), wsID(0xffff), wsType(0xffff),
                  wunderground(false),
                  domoticz(false),
                  dzPort(0),
                  windguru(false)
    {
        // clear entire arrays, this way we can use strncpy with sizeof-1 and be guaranteed a
        // terminating zero
        memset(wuID, 0, sizeof(wuID));
        memset(wuPW, 0, sizeof(wuPW));
        memset(dzURL, 0, sizeof(dzURL));
        memset(dzID, 0, sizeof(dzID));
        memset(dzPW, 0, sizeof(dzPW));
        memset(wgSalt, 0, sizeof(wgSalt));
        memset(wgUID, 0, sizeof(wgUID));
        memset(wgPW, 0, sizeof(wgPW));

        dzPort = 0;
        dzSecure = true;
        dzTHidx = 0;
        dzWidx = 0;
        dzRidx = 0;
        dzLidx = 0;
        dzUVidx = 0;

        lastReported = millis() - 60000;
        lastSeen = millis() - 60000;
        //burstStart = millis();

        //initialize to base object
        wsp = new WSBase();
    };

    virtual bool reportable()
    {
        if (mreportable)
        {
            mreportable = false;
            return true;
        }
        return false;
    }

    void deserialize(JsonObject ojson)
    {
        wsID = ojson["wsID"] | 0xffff;
        wsType = ojson["wsType"] | 0xffff;
        wunderground = ojson["wunderground"] | false;
        strncpy(wuID, ojson["wuID"] | "", sizeof(wuID));
        strncpy(wuPW, ojson["wuPW"] | "", sizeof(wuPW));
        domoticz = ojson["domoticz"] | false;
        strncpy(dzURL, ojson["dzURL"] | "", sizeof(dzURL));
        dzPort = ojson["dzPort"];
        dzSecure = ojson["dzSecure"];
        strncpy(dzID, ojson["dzID"] | "", sizeof(dzID));
        strncpy(dzPW, ojson["dzPW"] | "", sizeof(dzPW));
        dzTHidx = ojson["dzTHidx"];
        dzWidx = ojson["dzWidx"];
        dzRidx = ojson["dzRidx"];
        dzLidx = ojson["dzLidx"];
        dzUVidx = ojson["dzUVidx"];
        windguru = ojson["windguru"] | false;
        strncpy(wgSalt, ojson["wgSalt"] | "", sizeof(wgSalt));
        strncpy(wgUID, ojson["wgUID"] | "", sizeof(wgUID));
        strncpy(wgPW, ojson["wgPW"] | "", sizeof(wgPW));
        return;
    }

    void deserialize(String sjson)
    {
        DynamicJsonDocument djson(4096);
        DeserializationError err = deserializeJson(djson, sjson);
        if (err)
        {
            printf("WSSetting JSON deserialization error: %s", err.c_str());
        }
        deserialize(djson.as<JsonObject>());
        return;
    }

    void serialize(JsonObject ojson)
    {
        ojson["wsID"] = wsID;
        ojson["wsType"] = wsType;
        ojson["wunderground"] = wunderground;
        ojson["wuID"] = wuID;
        ojson["wuPW"] = wuPW;
        ojson["domoticz"] = domoticz;
        ojson["dzURL"] = dzURL;
        ojson["dzPort"] = dzPort;
        ojson["dzSecure"] = dzSecure;
        ojson["dzID"] = dzID;
        ojson["dzPW"] = dzPW;
        ojson["dzTHidx"] = dzTHidx;
        ojson["dzWidx"] = dzWidx;
        ojson["dzRidx"] = dzRidx;
        ojson["dzLidx"] = dzLidx;
        ojson["dzUVidx"] = dzUVidx;
        ojson["windguru"] = windguru;
        ojson["wgSalt"] = wgSalt;
        ojson["wgUID"] = wgUID;
        ojson["wgPW"] = wgPW;

        const char *cstr = ojson["dzURL"];
        printf("debug1 serialize ojson %s from %s\n", cstr, dzURL);
        return;
    }

    String serialize()
    {
        String sjson;
        DynamicJsonDocument djson(4096);
        serialize(djson.to<JsonObject>());
        if (serializeJson(djson, sjson) == 0)
        {
            printf("WSSetting JSON serialization error");
        }
        return sjson;
    }

    virtual String urlWunderground(const char *wuID, const char *wuPW)
    {
        //https://support.weather.com/s/article/PWS-Upload-Protocol?language=en_US
        //https://weatherstation.wunderground.com/weatherstation/updateweatherstation.php?ID=ISHERT38&PASSWORD=jHfIiiya&dateutc=now&tempf=37.8&humidity=1&action=updateraw
        String s = "";
        return s + "/weatherstation/updateweatherstation.php?ID=" + wuID +
               "&PASSWORD=" + wuPW +
               "&dateutc=now" +
               "&tempf=" + ((wsp->temperature * 9.0) / 5.0 + 32.0) +
               "&humidity=" + wsp->humidity +
               "&rainin=" + (wsp->rain1h / 25.4) +
               "&winddir=" + wsp->winddir +
               "&windspeedmph=" + (wsp->windspeed1m * 0.621371) +
               "&windgustmph=" + (wsp->windgust1m * 0.621371) +
               "&UV=" + wsp->UVI +
               "&action=updateraw";
    }

    //https://www.domoticz.com/wiki/Domoticz_API/JSON_URL's
    virtual String urlDomoticzTemp(uint32_t idx, const char *dzID, const char *dzPW)
    {
        //Note: idx is different for temp/hum and wind and uv.....
        //user and PW needs to be base64 enncoded.
        //
        ///json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=TEMP;HUM;HUM_STAT
        String s = "/json.htm?";
        if (strlen(dzID) > 0)
        {
            s = s + "username=" + dzID + "&password=" + dzPW;
        }
        return s + "type=command&param=udevice&idx=" + idx + "&nvalue=0&svalue=" + wsp->temperature + ";" + wsp->humidity + ";0";
    }

    virtual String urlDomoticzWind(uint32_t idx, const char *dzID, const char *dzPW)
    {
        //Note: idx is different for temp/hum and wind and uv.....
        //user and PW needs to be base64 enncoded.
        //
        ///json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=WB;WD;WS;WG;22;24
        String s = "/json.htm?";
        if (strlen(dzID) > 0)
        {
            s = s + "username=" + dzID + "&password=" + dzPW;
        }
        return s + "type=command&param=udevice&idx=" + idx +
               "&nvalue=0&svalue=" + wsp->winddir + ";;" + (wsp->windspeed * 10 / 3.6) + ";" + (wsp->windgust * 10 / 3.6) + ";" +
               wsp->temperature + ";0";
    }

    virtual String urlDomoticzRain(uint32_t idx, const char *dzID, const char *dzPW)
    {
        //Note: idx is different for temp/hum and wind and uv.....
        //user and PW needs to be base64 enncoded.
        //
        ///json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=RAINRATE;RAINCOUNTER
        String s = "/json.htm?";
        if (strlen(dzID) > 0)
        {
            s = s + "username=" + dzID + "&password=" + dzPW;
        }
        return s + "type=command&param=udevice&idx=" + idx + "&nvalue=0&svalue=" + (wsp->rain1h * 100) + ";" + wsp->rain;
    }

    virtual String urlDomoticzLight(uint32_t idx, const char *dzID, const char *dzPW)
    {
        //Note: idx is different for temp/hum and wind and uv.....
        //user and PW needs to be base64 enncoded.
        //
        ///json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=VALUE
        String s = "/json.htm?";
        if (strlen(dzID) > 0)
        {
            s = s + "username=" + dzID + "&password=" + dzPW;
        }
        return s + "type=command&param=udevice&idx=" + idx + "&nvalue=0&svalue=" + wsp->lightlux;
    }

    virtual String urlDomoticzUV(uint32_t idx, const char *dzID, const char *dzPW)
    {
        //Note: idx is different for temp/hum and wind and uv.....
        //user and PW needs to be base64 enncoded.
        //
        ///json.htm?type=command&param=udevice&idx=IDX&nvalue=0&svalue=UV;TEMP
        String s = "/json.htm?";
        if (strlen(dzID) > 0)
        {
            s = s + "username=" + dzID + "&password=" + dzPW;
        }
        return s + "type=command&param=udevice&idx=" + idx + "&nvalue=0&svalue=" + wsp->UVI + ";" + wsp->temperature;
    }

    virtual String urlWindguru(const char *wgUID, const char *wgPW)
    {
        ///upload/api.php?uid=stationXY&salt=20180214171400&hash=c9441d30280f4f6f4946fe2b2d360df5&wind_avg=12.5&wind_dir=165&temperature=20.5
        String salt = String(millis());
        //String salt = "20180214171400";
        String key = salt + wgUID + wgPW;
        //String key =  "20180214171400stationXYsupersecret"; //test string gives MD5: c9441d30280f4f6f4946fe2b2d360df5
        char *key2 = const_cast<char *>(key.c_str());
        unsigned char *hash = MD5::make_hash(key2);
        char *md5hash = MD5::make_digest(hash, 16);
        //printf("MD5 %s %s\n", key2, md5hash);
        free(hash);

        String s = "";
        return s + "/upload/api.php?uid=" + wgUID +
               "&salt=" + salt +
               "&hash=" + md5hash +
               "&interval=60" +
               "&wind_avg=" + (0.540 * wsp->windspeed1m) +
               "&wind_max=" + (0.540 * wsp->windgust1m) +
               "&wind_direction=" + wsp->winddir +
               "&temperature=" + wsp->temperature;
        //"&rh=" + humidity;
    }

    virtual void update(WSBase *data, uint8_t *pktbuf)
    {
        //copy data to this station
        *wsp = *data;
        mreportable = true;
        lastSeen = millis();

        printf("Station updated T=%fC\n", wsp->temperature);
        wsp->printtype();
        data->printtype();

        //update last hour rain
        double rainprevhour = data->rain;
        rainhist.insert(std::make_pair(data->at.tv_sec, data->rain));
        std::map<time_t, double>::iterator it = rainhist.begin();
        while (it != rainhist.end() && it->first < data->at.tv_sec - 3600)
        {
            //do not use stale data
            if (it->first > data->at.tv_sec - 7200)
                rainprevhour = it->second;
            printf("rainloop %ld,%f\n", it->first, it->second);
            it = rainhist.erase(it);
        }
        wsp->rain1h = data->rain - rainprevhour;
        printf("rainhist size %d\n", rainhist.size());
        printf("rain1h %f\n", wsp->rain1h);

        //calculate average windspeed for last minute
        int count = 0;
        double windsum = 0.0;
        windhist.insert(std::make_pair(data->at.tv_sec, data->windspeed));
        it = windhist.begin();
        while (it != windhist.end())
        {
            printf("windloop %ld,%f\n", it->first, it->second);
            if (it->first < data->at.tv_sec - 60)
                it = windhist.erase(it);
            else
            {
                count++;
                windsum += it->second;
                ++it;
            }
        }
        wsp->windspeed1m = windsum / count;
        printf("windspeed1m %f\n", wsp->windspeed1m);

        //calculate max windgust for last minute
        wsp->windgust1m = 0;
        gusthist.insert(std::make_pair(data->at.tv_sec, data->windgust));
        it = gusthist.begin();
        while (it != gusthist.end())
        {
            if (it->first < data->at.tv_sec - 60)
                it = gusthist.erase(it);
            else
            {
                if (it->second > wsp->windgust1m)
                    wsp->windgust1m = it->second;
                ++it;
            }
        }
        printf("windmax1m %f\n", wsp->windgust1m);
    }
};

struct WSUnknown : public WSBase, public WSSetting
{
    //nothing to override right now
    WSUnknown(){};
};

struct WSBR1800 : public BR1800, public WSSetting
{
    //nothing to override right now
    WSBR1800(){};
};

struct WSWH1080 : public WH1080, public WSSetting
{
    //transmissions are a burst of up to 6 identical messages
    //unsigned long burstStart;
    char packets[6][10];
    unsigned long ts[6];
    int equal[6];
    int burstCount;

    WSWH1080()
    {
        mreportable = false;
        //burstStart = millis();
    }

    void validatePackets() {
        printf("WH1080 received %d packets with crc ok in burst", burstCount);       
        if (burstCount > 1) {
            for (int i = 0; i < burstCount - 1; i++)
            {
                equal[i] = 0;
                for (int j = 1; j < burstCount; j++)
                {
                    if (strncmp(packets[0], packets[j], 9) == 0)
                    {
                        equal[i]++;
                    }
                }
            }
            int maxEqualIdx = 0;
            int maxEqual = equal[0];
            for (int i = 1; i < burstCount; i++) {
                if (equal[i] > maxEqual) {
                    maxEqual = equal[i];
                    maxEqualIdx = i;
                }
            }
            printf("WH1080 maximum number of equal packets %d of %d in burst, found at index %d", maxEqual, burstCount, maxEqualIdx);
            //redecode packet as the last one may not be the best one.
            uint8_t len = (wsp->msgformat == MSG_WS3000 ? LEN_WS3000 : LEN_WS4000);
            wsp->decode(wsp->msgformat, (uint8_t *)packets[maxEqualIdx], len);
        }
    }
    
    virtual bool reportable()
    {
        if (mreportable && lastSeen - millis() > 200)
        {
            //decide which response is the correct
            validatePackets();

            mreportable = false;
            return true;
        }
        return false;
    }

    virtual void update(WSBase *data, uint8_t *pktbuf)
    {
        //store ws objects and pktbuf in arrays
        if (millis() - lastSeen > 500) {
            //reset packets when previous packet at least early than 500ms ago.
            burstCount = 0;
        }
        //Add packet to storage unless to many packets in burst
        ts[burstCount] = millis();
        strncpy(packets[burstCount], (char *)pktbuf, 9);
        burstCount++;
    }
};

struct WSUnknownFineOffset : public UnknownFineOffset, public WSSetting
{
    //nothing to override right now
    WSUnknownFineOffset(){};
};

class WSConfigTest
{
public:
    WSSetting ws;
    WSSetting wsdes;

    WSConfigTest()
    {
        ws.wsID = 0x3C;
        ws.wsType = 0x24;
        ws.wunderground = true;
        strncpy(ws.wuID, "wuUserId", sizeof(ws.wuID));
        strncpy(ws.wuPW, "wuPassWd", sizeof(ws.wuPW));
        ws.domoticz = true;
        strncpy(ws.dzURL, "192.168.44.254", sizeof(ws.dzURL));
        ws.dzPort = 8080;
        ws.dzSecure = false;
        strncpy(ws.dzID, "", sizeof(ws.dzID));
        strncpy(ws.dzPW, "", sizeof(ws.dzPW));
        ws.dzTHidx = 2955;
        ws.dzWidx = 2956;
        ws.dzRidx = 2957;
        ws.dzLidx = 2958;
        ws.dzUVidx = 2959;
        ws.windguru = false;
        strncpy(ws.wgSalt, "", sizeof(ws.wgSalt));
        strncpy(ws.wgUID, "", sizeof(ws.wgUID));
        strncpy(ws.wgPW, "", sizeof(ws.wgPW));
    };

    void test()
    {
        String ser = ws.serialize();
        printf("%s\n", ser.c_str());
        wsdes.deserialize(ser);
        printf("url is %s and %s\n", ws.dzURL, wsdes.dzURL);
        wsdes.wsID = 1;
        wsdes.wsType = 2;
        WSSetting stations[2];
        stations[0] = ws;
        stations[1] = wsdes;

        const int capacity = JSON_ARRAY_SIZE(MAX_WS) + JSON_OBJECT_SIZE(10);
        //DynamicJsonDocument json(2 * capacity);
        DynamicJsonDocument json(4096);

        for (int i = 0; i < 2; i++)
        {
            JsonObject ojson = json.createNestedObject();
            stations[i].serialize(ojson); //Populate the JsonObject
        }

        String sjson;
        serializeJson(json, sjson);
        printf("array: %s\n", sjson.c_str());

        DynamicJsonDocument ijson(4096);
        DeserializationError err = deserializeJson(ijson, sjson);
        WSSetting istations[2];
        for (int i = 0; i < 2; i++)
        {
            istations[i].deserialize(json[i].as<JsonObject>());
        }
        printf("desarray %d, %d, %s, %d, %d ,%s\n", istations[0].wsID, istations[0].wsType, istations[0].dzURL, istations[1].wsID, istations[1].wsType, istations[1].dzURL);
    };
};

// WSConfig manages the storage of data upload parameters
// A singleton object needs to be allocated during set-up and remain
// in-memory "forever" because the client libs refer to its storage.
class WSConfig
{
public:
    // Configuration parameters that need to remain allocated for the duration of the app because
    // other classes, such as .... rely on that.
    WSSetting stations[MAX_WS];

    WSConfig()
        : initialized(false)
    {
        for (int i = 0; i < MAX_WS; i++)
        {
            stations[i] = WSSetting();
        }
    }

    uint8_t ilookup(uint16_t wsType, uint16_t wsID)
    {
        for (int i = 0; i < MAX_WS; i++)
        {
            if (stations[i].wsType == wsType && stations[i].wsID == wsID)
                return i;
        }
        return 0xff;
    };

    WSSetting *lookup(uint16_t wsType, uint16_t wsID)
    {
        uint8_t idx = ilookup(wsType, wsID);
        return (idx < MAX_WS) ? &stations[idx] : nullptr;
    };

    void add(String sjson)
    {
        //through MQTT message
        WSSetting newWS;
        newWS.deserialize(sjson);
        uint8_t idx = ilookup(newWS.wsType, newWS.wsID);
        if (idx < MAX_WS)
        {
            printf("WSConfig::add: station updated at index %d\n", idx);
        }
        else
        {
            printf("WSConfig::add: station not found\n");
            //find first free slot
            idx = ilookup(0xffff, 0xffff);
            if (idx < MAX_WS)
            {
                printf("WSConfig::add: at first free entry at %d\n", idx);
            }
            else
            {
                idx = MAX_WS - 1;
                printf("WSConfig::add: no free slot, overwrite last entry at %d\n", idx);
            }
        }

        WSBase *WS;
        switch (newWS.wsType)
        {
        case 0x24:
        {
            WS = new WSBR1800();
            break;
        }
        case 0x28:
        case 0x2A:
        {
            WS = new WSWH1080();
            break;
        }
        default:
            WS = new WSUnknown();
        }
        printf("1\n");
        delete stations[idx].wsp;
        printf("2\n");
        stations[idx] = newWS;
        printf("3\n");
        stations[idx].wsp = WS;
        save();
    };

    void remove(String sjson)
    {
        //through MQTT message
        WSSetting delWS;
        delWS.deserialize(sjson);
        if (delWS.wsType == 0xffff or delWS.wsID == 0xffff)
        {
            printf("WSConfig::remove: unexpected wsType or wsID\n");
        }
        uint8_t idx = ilookup(delWS.wsType, delWS.wsID);
        if (idx < MAX_WS)
        {
            printf("WSConfig::remove: station remove at at index %d\n", idx);
            WSSetting emptyWS;
            stations[idx] = emptyWS;
        }
        else
        {
            printf("WSConfig::remove: station not found\n");
        };
    };

    //Save to SPI Flash
    void save()
    {
        //const int capacity = JSON_ARRAY_SIZE(MAX_WS) + JSON_OBJECT_SIZE(10);
        //DynamicJsonDocument json(2 * capacity);
        DynamicJsonDocument json(4096);

        for (int i = 0; i < MAX_WS; i++)
        {
            printf("%d, type %d\n", stations[i].wsID, stations[i].wsType);
            JsonObject ojson = json.createNestedObject();
            stations[i].serialize(ojson); //Populate the JsonObject
            printf("%d, type %d-ex\n", stations[i].wsID, stations[i].wsType);
        }

        printf("going to open stationconfig file\n");

        File configFile = SPIFFS.open("/stationconfig.json", FILE_WRITE);
        if (!configFile)
        {
            printf("failed to write stationconfig.json\n");
        }
        else
        {
            printf("going to serialize stationconfig file\n");
            serializeJson(json, configFile);
            configFile.close();
            printf("serialized stationconfig file\n");
        }
    };

    //Load from SPI Flash
    void load()
    {
        // mount SPIFFS, this does nothing if it's already mounted.
        if (!SPIFFS.begin(false))
        {
            uint32_t t0 = millis();
            printf("** Formatting fresh SPIFFS, takes ~20 seconds\n");
            if (!SPIFFS.begin(true))
            {
                printf("SPIFFS formatting failed, check your hardware, OOPS!\n");
            }
            else
            {
                printf("Format took %lus\n", (millis() - t0 + 500) / 1000);
            }
        }

        File configFile = SPIFFS.open("/stationconfig.json", FILE_READ);
        if (configFile && configFile.size() > 10)
        {
            // load as json
            size_t size = configFile.size();
            printf("stationconfig file size is %d\n", size);
            DynamicJsonDocument json(4096);
            DeserializationError err = deserializeJson(json, configFile);
            configFile.close();
            if (err)
            {
                printf("failed to parse stationconfig.json: %s\n\n\n\n", err.c_str());
#if 1
                File cf = SPIFFS.open("/stationconfig.json", FILE_READ);
                printf("Contents (%d):\n\n\n <<", cf.size());
                for (int ch = cf.read(); ch != -1; ch = cf.read())
                {
                    if (ch >= ' ' && ch <= '~')
                        putchar(ch);
                    else
                        putchar('~');
                }
                printf(">>\n");
                cf.close();
#endif
                return;
            }

            printf("Dumping stationconfig\n\n");
            File cf = SPIFFS.open("/stationconfig.json", FILE_READ);
            printf("Contents (%d): <<", cf.size());
            for (int ch = cf.read(); ch != -1; ch = cf.read())
            {
                if (ch >= ' ' && ch <= '~')
                    putchar(ch);
                else
                    putchar('~');
            }
            printf(">>\n");
            cf.close();

            printf("\n\nDumped stationconfig\n\n");

            for (int i = 0; i < MAX_WS; i++)
            {
                stations[i].deserialize(json[i].as<JsonObject>());
                printf("reportable after deserialize %d", stations[i].mreportable);
            };

            //printf("Config restored: MQTT<%s,%s;%s,%s...> AP<%s>\n",
            //        mqtt_server, mqtt_port, mqtt_ident, psk, ap_pass);
        }
        else
        {
            if (configFile)
                configFile.close();
            Serial.println("No config file found.");

            //store a file with unitialized entries
            save();
        }
        initialized = true;
    };

    //private:

    bool initialized; // true once the config has been read
};
