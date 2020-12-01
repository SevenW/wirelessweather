#include <string>

#include "ftoa.h"
#include <ArduinoJson.h>
#include <md5.h>

#define MSG_WH2300 36
#define MSG_WS4000 40
#define MSG_WS3000 42
#define LEN_WH2300 16
#define LEN_WS4000 10
#define LEN_WS3000 9

class WSBase
{
public:
    //identity
    uint16_t msgformat = 0xffff;
    uint16_t stationID = 0xffff;

    //data
    double temperature; //in Celscius
    uint16_t humidity;  //relative %
    uint16_t winddir;   //in degrees North = 0
    double windspeed;   //in km/h
    double windgust;    //in km/h
    double lightlux;    //in Lux
    double rain;        //in mm
    uint16_t UVraw;     //a.u.
    uint8_t UVI;        //index 0-13
    bool low_battery;   //true is low battery

    //calculated fields
    double windspeed1m; //in km/h
    double windgust1m;  //in km/h
    double rain1h;      //in mm

    //RF receive
    int32_t afc;       // in Hz
    uint8_t rssi;      // in dBm
    uint8_t lna;       // in step
    uint8_t snr;       // in dB
    struct timeval at; // arrival timestamp

    //default constructor
    WSBase()
    {
        msgformat = 0xFFFF;
        stationID = 0xFFFF;

        temperature = 0.0;
        humidity = 0;
        winddir = 0;
        windspeed = 0.0;
        windgust = 0.0;
        lightlux = 0.0;
        rain = 0.0;
        UVraw = 0;
        UVI = 0;
        low_battery = false;
        windspeed1m = 0.0;
        windgust1m = 0.0;
        rain1h = 0.0;

        afc = 0;
        rssi = 0;
        lna = 0;
        snr = 0;
        at = (struct timeval){0};
    }

    //copy constructor
    WSBase(const WSBase &ws)
    {
        //identity
        msgformat = ws.msgformat;
        stationID = stationID;

        //data
        temperature = ws.temperature;
        humidity = ws.humidity;
        winddir = ws.winddir;
        windspeed = ws.windspeed;
        windgust = ws.windgust;
        lightlux = ws.lightlux;
        rain = ws.rain;
        UVraw = ws.UVraw;
        UVI = ws.UVI;
        low_battery = ws.low_battery;

        //calculated fields
        windspeed1m = ws.windspeed1m;
        windgust1m = ws.windgust1m;
        rain1h = ws.rain1h;

        //RF receive
        afc = ws.afc;
        rssi = ws.rssi;
        lna = ws.lna;
        snr = ws.snr;
        at = ws.at;
    }

    virtual ~WSBase(){};
    virtual bool decode(uint8_t fmt, uint8_t *buf, uint8_t len)
    {
        return false;
    };

    //copy assignment operator
    WSBase &operator=(const WSBase &ws)
    {
        //identity
        msgformat = ws.msgformat;
        stationID = ws.stationID;

        //data
        temperature = ws.temperature;
        humidity = ws.humidity;
        winddir = ws.winddir;
        windspeed = ws.windspeed;
        windgust = ws.windgust;
        lightlux = ws.lightlux;
        rain = ws.rain;
        UVraw = ws.UVraw;
        UVI = ws.UVI;
        low_battery = ws.low_battery;

        //calculated fields
        windspeed1m = ws.windspeed1m;
        windgust1m = ws.windgust1m;
        rain1h = ws.rain1h;

        //RF receive
        afc = ws.afc;
        rssi = ws.rssi;
        lna = ws.lna;
        snr = ws.snr;
        at = ws.at;

        return *this;
    }

    void setRFStats(struct timeval rxAt, uint8_t rxrssi, uint8_t rxsnr, uint8_t rxlna, int32_t rxafc)
    {
        at = rxAt;
        rssi = rxrssi;
        snr = rxsnr;
        lna = rxlna;
        afc = rxafc;
    };

    // virtual void print() = 0;
    // virtual String urlWunderground(const char *wuID, const char *wuPW) = 0;
    // virtual String urlDomoticzTemp(uint32_t idx, const char *dzID, const char *dzPW) = 0;
    // virtual String urlDomoticzWind(uint32_t idx, const char *dzID, const char *dzPW) = 0;
    // virtual String urlDomoticzRain(uint32_t idx, const char *dzID, const char *dzPW) = 0;
    // virtual String urlDomoticzLight(uint32_t idx, const char *dzID, const char *dzPW) = 0;
    // virtual String urlDomoticzUV(uint32_t idx, const char *dzID, const char *dzPW) = 0;
    // virtual String urlWindguru(const char *wgUID, const char *wgSalt, const char *wgHash) =0;

    virtual void printtype() {
        printf("Instance of WSBase\n");
    };

    virtual String mqttPayload()
    {
        String sjson;
        DynamicJsonDocument djson(2048);
        djson["ts"] = at.tv_sec;
        djson["stType"] = msgformat;
        djson["stID"] = stationID;
        djson["T"] = temperature;
        djson["rh"] = humidity;
        djson["winddir"] = winddir;
        djson["wind"] = windspeed;
        djson["wind1m"] = windspeed1m;
        djson["gust"] = windgust;
        djson["gust1m"] = windgust1m;
        djson["rain"] = rain;
        djson["rain1h"] = rain1h;
        djson["lux"] = lightlux;
        djson["UV"] = UVraw;
        djson["UVI"] = UVI;
        djson["battery"] = ((low_battery ? 0 : 100));
        djson["rssi"] = rssi / -2.0;
        djson["snr"] = snr;
        djson["lna"] = lna;
        djson["afc"] = afc;

        if (serializeJson(djson, sjson) == 0)
        {
            printf("WSBase JSON serialization error");
        }
        return sjson;
    };

    virtual void print()
    {
        char fstr[30];
        printf("ID: %02x, ", stationID);
        printf("T=%8s°C, ", ftoa(temperature, fstr, 1));
        printf("relH=%3d%%, ", humidity);
        printf("Wvel=%5skm/h, ", ftoa(windspeed, fstr, 1));
        printf("Wmax=%5skm/h, ", ftoa(windgust, fstr, 1));
        printf("Wdir=%3d°, ", winddir);
        printf("Rain=%6smm, ", ftoa(rain, fstr, 1));
        printf("UV=%5d, ", UVraw);
        printf("UVindex=%2d, ", UVI);
        printf("Light=%6sW/m^2, ", ftoa(0.0079 * lightlux, fstr, 1));
        (low_battery) ? printf("low battery") : printf("battery ok");
        printf("\n");
    }
};

class BR1800 : public WSBase
{
public:
    BR1800() {};
    
    BR1800(uint8_t fmt, uint8_t len, uint8_t *buf)
    {
        msgformat = fmt;
        decode(fmt, buf, len);
    }

    ~BR1800(){};

    virtual void printtype()
    {
        printf("Instance of BR1800\n");
    };

    bool decode(uint8_t fmt, uint8_t *buf, uint8_t len)
    {
        /*
        - Payload:   FF II DD VT TT HH WW GG RR RR UU UU LL LL LL CC BB
        - F: 8 bit Family Code, fixed 0x24
        - I: 8 bit Sensor ID, set on battery change
        - D: 8 bit Wind direction
        - V: 4 bit Various bits, D11S, wind dir 8th bit, wind speed 8th bit
        - B: 1 bit low battery indicator
        - T: 11 bit Temperature (+40*10), top bit is low battery flag
        - H: 8 bit Humidity
        - W: 8 bit Wind speed
        - G: 8 bit Gust speed
        - R: 16 bit rainfall counter
        - U: 16 bit UV value
        - L: 24 bit light value
        - C: 8 bit CRC checksum of the 15 data bytes
        - B: 8 bit Bitsum (sum without carry, XOR) of the 16 data bytes
        */

        // station id
        stationID = buf[1];
        // winddirection
        winddir = ((buf[3] & 0x80) << 1) | buf[2];
        //low battery bit
        low_battery = (buf[3] & 0x08) >> 3;
        // temperature
        int16_t temp = (((buf[3] & 0x07) << 8) | buf[4]) - 400;
        temperature = temp * 0.1;
        //humidity
        humidity = buf[5];
        //wind speed in km/h
        //original code had windspeedcorrectionfactor = 1.12.
        //calibration revelaed that windspeed was reported factor 1.27 too high
        //new windspeedcorrectionfactor = 1.12 / 1.27 = 0.88.
        //1/1.12 = 0.89, close to calibrated 0.88. Was original factor applied wrongly?
        windspeed = (((buf[3] & 0x10) << 4) | buf[6]) * 1.12 * 0.125 * 3.6;
        //windspeed correction is applied in stationconfig.h
        //windspeed = (((buf[3] & 0x10) << 4) | buf[6]) * 0.88 * 0.125 * 3.6;
        //wind gust in km/h
        windgust = buf[7] * 1.12 * 3.6;
        //windgust = buf[7] * 0.88 * 3.6;
        //rainfall
        rain = ((buf[8] << 8) | buf[9]) * 0.3;
        //uv intensity
        UVraw = (buf[10] << 8) | buf[11];
        //light intensity
        lightlux = ((buf[12] << 16) | (buf[13] << 8) | buf[14]) / 10.0;

        //WH24 tabel
        // UV value   UVI
        // 0-432      0
        // 433-851    1
        // 852-1210   2
        // 1211-1570  3
        // 1571-2017  4
        // 2018-2450  5
        // 2451-2761  6
        // 2762-3100  7
        // 3101-3512  8
        // 3513-3918  9
        // 3919-4277  10
        // 4278-4650  11
        // 4651-5029  12
        // >=5230     13
        //uint16_t uvi_upper[] = {432, 851, 1210, 1570, 2017, 2450, 2761, 3100, 3512, 3918, 4277, 4650, 5029};

        //derived from WH18
        // 0->98 0 (boundary between 97-98)
        // 99->499 1 (boundary between 468-521)
        // 500->800 2 estimated steps of 400?
        // 751->1200 3
        // 1201->1650 4
        // 1651->2100 5
        // 2101->2750 6
        // 2751->3400 7
        // 3401->4100 8
        // 4101->4950 9
        // 4951->5750 10
        // 5751->6350 11
        // 6351->7000 12
        // >= 7000 13

        uint16_t uvi_upper[] = {98, 499, 800, 1200, 1650, 2100, 2750, 3400, 4100, 4950, 5750, 6350, 7000};

        UVI = 0;
        while (UVI < 13 && uvi_upper[UVI] < UVraw)
            ++UVI;

        return true;
    }
};

class WH1080 : public WSBase
{
public:
    WH1080() {};
    
    WH1080(uint8_t fmt, uint8_t len, uint8_t *buf)
    {
        msgformat = fmt;
        decode(fmt, buf, len);
    }

    ~WH1080(){};

    virtual void printtype()
    {
        printf("Instance of WH1080\n");
    };

    bool decode(uint8_t fmt, uint8_t *sbuf, uint8_t len)
    {
        //char *compass[] = {"N  ", "NNE", "NE ", "ENE", "E  ", "ESE", "SE ", "SSE", "S  ", "SSW", "SW ", "WSW", "W  ", "WNW", "NW ", "NNW"};
        // station id
        stationID = (sbuf[0] << 4) | (sbuf[1] >> 4);
        // temperature in C
        uint8_t sign = (sbuf[1] >> 3) & 1;
        int16_t temp = ((sbuf[1] & 0x07) << 8) | sbuf[2];
        if (sign)
            temp = (~temp) + sign;
        temperature = temp * 0.1;
        //humidity
        humidity = sbuf[3] & 0x7F;
        //wind speed in km/h
        windspeed = sbuf[4] * 0.34 * 3.6;
        //wind gust in km/h
        windgust = sbuf[5] * 0.34 * 3.6;
        //rainfall in mm
        rain = (((sbuf[6] & 0x0F) << 8) | sbuf[7]) * 0.3;

        if (fmt == MSG_WS4000)
        {
            //wind direction convert to degrees by mulitplication with 360/16
            winddir = ((sbuf[8] & 0x0F) * 45) >> 2;
            //low_battery indicator
            low_battery = (sbuf[8] >> 4) == 1;
        }
        else
        {
            // Initialize unsupported fields
            winddir = 0;
            low_battery = false;
        }

        // Initialize unsupported fields
        UVraw = 0;
        UVI = 0;
        lightlux = 0;

        return true;
    };

    virtual void print()
    {
        char fstr[30];
        printf("ID: %02x, ", stationID);
        printf("T=%8s°C, ", ftoa(temperature, fstr, 1));
        printf("relH=%3d%%, ", humidity);
        printf("Wvel=%5skm/h, ", ftoa(windspeed, fstr, 1));
        printf("Wmax=%5skm/h, ", ftoa(windgust, fstr, 1));
        printf("Wdir=%3d°, ", winddir);
        printf("Rain=%6smm, ", ftoa(rain, fstr, 1));
        (low_battery) ? printf("low battery") : printf("battery ok");
        printf("\n");
    }
};

class UnknownFineOffset : public WSBase
{
private:
    uint8_t buf[17];
    uint8_t length = 0;

public:
    UnknownFineOffset() {};

    UnknownFineOffset(uint8_t fmt, uint8_t len, uint8_t *buf)
    {
        msgformat = fmt;
        decode(fmt, buf, len);
    }

    ~UnknownFineOffset(){};

    virtual void printtype()
    {
        printf("Instance of UnknownFineOffset\n");
    };

    bool decode(uint8_t fmt, uint8_t *sbuf, uint8_t len)
    {
        for (int i = 0; i < len; i++)
        {
            buf[i] = sbuf[i];
        }
        length = len;
        return false;
    };

    virtual String mqttPayload()
    {
        char hexstr[3 * length + 1];
        hexstr[3 * length] = 0;
        for (int j = 0; j < length; j++)
            sprintf(&hexstr[3 * j], " %02X", buf[j]);

        String sjson;
        DynamicJsonDocument djson(2048);
        djson["ts"] = at.tv_sec;
        djson["stType"] = msgformat;
        djson["stID"] = stationID;
        djson["buf"] = hexstr;
        djson["rssi"] = rssi / -2.0;
        djson["snr"] = snr;
        djson["lna"] = lna;
        djson["afc"] = afc;

        if (serializeJson(djson, sjson) == 0)
        {
            printf("WSBase JSON serialization error");
        }
        return sjson;
    };
    virtual void print()
    {
        printf("CRC passed for unknown message: ");
        for (int i = 0; i < length; i++)
        {
            printf("%02x ", buf[i]);
        }
        printf("\n");
    }
};

//Singleton class interpeting the raw buffer to determine type of weatherstation
class WeatherStationProcessor
{
    uint32_t nWsSignals = 0;
    uint32_t nWsSignalsOK = 0;

    uint8_t _crc8(volatile uint8_t *addr, uint8_t len)
    {
        uint8_t crc = 0;

        // Indicated changes are from reference CRC-8 function in OneWire library
        while (len--)
        {
            uint8_t inbyte = *addr++;
            uint8_t i;
            for (i = 8; i; i--)
            {
                uint8_t mix = (crc ^ inbyte) & 0x80; // changed from & 0x01
                crc <<= 1;                           // changed from right shift
                if (mix)
                    crc ^= 0x31; // changed from 0x8C;
                inbyte <<= 1;    // changed from right shift
            }
        }
        return crc;
    }

    uint8_t _checksum(volatile uint8_t *addr, uint8_t len)
    {
        uint8_t checksum = 0;
        for (unsigned n = 0; n < len; ++n)
        {
            checksum += addr[n];
        }
        return checksum;
    }

public:
    WSBase *processWSPacket(uint8_t *buf, int length, struct timeval rxAt, int8_t rxrssi, uint8_t rxsnr, uint8_t rxlna, int32_t rxafc)
    {
        WSBase *wsObject = nullptr;

        if (length > 7)
        {
            nWsSignals++;
            uint8_t crc_ok = 0;
            bool checksum_ok = false;
            uint8_t mt = buf[0] >> 4;
            if (buf[0] == 0x24)
            {
                mt = 0x24;
            }
            switch (mt)
            {
            case 0x5:
            case 0x6:
            {
                crc_ok = buf[8] == _crc8(&buf[0], 8);
                if (crc_ok)
                    nWsSignalsOK++;
                printf(crc_ok ? "crc  ok " : "crc nok \n");
                if (crc_ok)
                {
                    WH1080 *wh1080 = new WH1080(MSG_WS3000, LEN_WS3000, buf);
                    wsObject = wh1080;
                }
                break;
            }
            case 0xA:
            case 0xB:
            {
                crc_ok = buf[9] == _crc8(&buf[0], 9);
                if (crc_ok)
                    nWsSignalsOK++;
                printf(crc_ok ? "crc  ok " : "crc nok\n");
                if (crc_ok)
                {
                    WH1080 *wh1080 = new WH1080(MSG_WS4000, LEN_WS4000, buf);
                    wsObject = wh1080;
                }
                break;
            }
            case 0x24:
            {
                crc_ok = buf[15] == _crc8(&buf[0], 15);
                checksum_ok = buf[16] == _checksum(&buf[0], 16);
                if (crc_ok && checksum_ok)
                    nWsSignalsOK++;
                printf(checksum_ok ? "crc + checksum  ok " : "crc + checksum nok\n");
                crc_ok &= checksum_ok;
                if (crc_ok)
                {
                    BR1800 *br1800 = new BR1800(MSG_WH2300, LEN_WH2300, buf);
                    wsObject = br1800;
                }
                break;
            }
            default:
                //Check for unknown weather station type
                //evaluate crc and checksum
                for (int i = 6; i < length; i++)
                {
                    crc_ok = buf[i] == _crc8(&buf[0], i);
                    if (crc_ok)
                    {
                        int unkLen = i + 1;
                        if ((i + 1) < length)
                        {
                            checksum_ok = buf[i + 1] == _checksum(&buf[0], i + 1);
                            unkLen++;
                        }
                        //report out on succesful crc of unknown weather station
                        UnknownFineOffset *unknown = new UnknownFineOffset(0xFF, length, buf);
                        wsObject = unknown;
                        break;
                    }
                }

                break; //crc_ok=0;
            }
        }

        if (wsObject)
        {
            wsObject->setRFStats(rxAt, rxrssi, rxsnr, rxlna, rxafc);
        }

        return wsObject;
    }
};
