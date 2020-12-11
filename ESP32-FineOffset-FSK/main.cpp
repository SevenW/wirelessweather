// ESP32 FineOffset Weather Station Internet Bridge
// Copyright (c) 2020 SevenWatt.com , all rights reserved
//
// Connect to WiFi.
// Connect to MQTT server
// Report weather to
// - Wunderground / Weather.com WPS API
// - Windguru
// - Domoticz
// - MQTT
// The above rporting is configurable over MQTT
// OTA flash updates announced over MQTT
//
//

#include <Arduino.h>
#include <SPI.h>
#include <SX1276fsk.h>
#include <WiFi.h>
#include <ESPSecureBase.h>
#include <libb64/cencode.h>
#include <lwip/apps/sntp.h>
#include "analog.h"
#include "weather.h"
#include <WiFiClientSecure.h>
#include "stationconfig.h"
#include "SX1276ws.h"

#if defined BOARD_HELTEC
#include "heltec.h"
#endif

//===== I/O pins/devices

#if defined BOARD_RFGW2

#define RF_SS 25
#define RF_RESET 27
#define RF_CLK 32
#define RF_MISO 19
#define RF_MOSI 33
#define RF_DIO0 26
#define RF_DIO4 22

#define LED_MQTT 4 // pulsed for each MQTT message received
#define LED_RF 2   // pulsed for each RF packet received
#define LED_WIFI LED_MQTT
#define LED_ON 1
#define LED_OFF 0

#define VBATT 35

#elif defined BOARD_EZSBC

#define RF_SS 25
#define RF_RESET 27
#define RF_CLK 32
#define RF_MISO 35
#define RF_MOSI 33
#define RF_DIO0 26
#define RF_DIO4 39

#define LED_WIFI 16 // lit while there is no Wifi/MQTT connection
#define LED_RF 17   // pulsed for each MQTT message received
#define LED_MQTT 18 // pulsed for each RF packet received
#define LED_ON 0
#define LED_OFF 1

#elif defined BOARD_HELTEC

#define RF_SS 18
#define RF_RESET 14
#define RF_CLK 5
#define RF_MISO 19
#define RF_MOSI 27
#define RF_DIO0 26
#define RF_DIO1 33 //35
#define RF_DIO2 32 //34
#define RF_DIO4 -1

#define LED 25 // pulsed for each RF packet or MQTT message received
#define LED_RF LED
#define LED_MQTT LED
#define LED_WIFI LED
#define LED_ON 1
#define LED_OFF 0

#else

#error "Board is not defined"
#endif

SPIClass spi;
SX1276ws radio(spi, RF_SS, RF_RESET); // ss and reset pins

uint8_t rfId = 63; // 61=tx-only node, 63=promisc node
//uint8_t rfGroup = 178;
//uint32_t rfFreq = 868000000;
uint8_t rfGroup = 42;
uint32_t rfFreq = 868300000;
int8_t rfPow = 17;
DV(rfId);
DV(rfGroup);
DV(rfFreq);
DV(rfPow);

uint32_t rfLed = 0;
uint32_t mqttLed = 0;
uint32_t vBatt = 1;

time_t lastWSts = 0;

ESBConfig config;
CommandParser cmdP(&Serial);
ESBCLI cmd(config, cmdP);
ESBDebug dbg(cmdP);

#define GW_TOPIC "rfgw/reports"
bool mqttConn = false; // whether mqtt is connected or not
DV(mqttConn);

//NodeRegistryWorker nrw(mqTopic, GW_TOPIC);

//Singleton instance of WSConfig
WSConfig wsConfig;
//Singleton class to detect type of weatherstation
WeatherStationProcessor wsProcessor;

//Global http clients to avoid opening many times
WiFiClient HTTPClient;
WiFiClientSecure HTTPSClient;

// MQTT message handling

uint32_t mqttTxNum = 0,
         mqttRxNum = 0;

void onMqttMessage(char *topic, char *payload, MqttProps properties,
                   size_t len, size_t index, size_t total)
{
    mqttRxNum++;

    // Handle over-the-air update messages
    if (strlen(topic) == mqTopicLen + 4 && len == total &&
        strncmp(topic, mqTopic, mqTopicLen) == 0 &&
        strcmp(topic + mqTopicLen, "/ota") == 0)
    {
        ESBOTA::begin(payload, len);
    }

    // Handle weather station config messages
    if (strlen(topic) == mqTopicLen + 9 && len == total &&
        strncmp(topic, mqTopic, mqTopicLen) == 0 &&
        strcmp(topic + mqTopicLen, "/wsconfig") == 0)
    {
        wsConfig.add(payload);
    }

    // Handle weather station config messages
    if (strlen(topic) == mqTopicLen + 9 && len == total &&
        strncmp(topic, mqTopic, mqTopicLen) == 0 &&
        strcmp(topic + mqTopicLen, "/wsdelete") == 0)
    {
        wsConfig.remove(payload);
    }

    digitalWrite(LED_MQTT, LED_ON);
    mqttLed = millis();
}

void onMqttConnect(bool sessionPresent)
{
    printf("Connected to MQTT, session %spresent\n", sessionPresent ? "" : "not ");
    char topic[128];

    strncpy(topic, mqTopic, 32);
    strcat(topic, "/ota");
    mqttClient.subscribe(topic, 1);
    printf("Subscribed to %s for OTA\n", topic);

    strncpy(topic, mqTopic, 32);
    strcat(topic, "/wsconfig");
    mqttClient.subscribe(topic, 1);
    printf("Subscribed to %s for configuration of report out of weather stations\n", topic);

    strncpy(topic, mqTopic, 32);
    strcat(topic, "/wsdelete");
    mqttClient.subscribe(topic, 1);
    printf("Subscribed to %s for deleting a reporting weather stations\n", topic);
}

void publishWS(const char *payload)
{
    char topic[41 + 6];
    strcpy(topic, mqTopic);
    strcat(topic, "/ws");
    uint16_t id = mqttClient.publish(topic, 1, false, payload);
    printf("MQTT %d %s %s\n", id, topic, payload);
    mqttTxNum++;
}

uint32_t rfRxNum = 0;

boolean UploadToWebAPI(const char *host, int httpsPort, bool secure, const char *url)
{
    WiFiClient *client;
    // if (secure)
    // {
    //     client = new WiFiClientSecure();
    // }
    // else
    // {
    //     client = new WiFiClient();
    // }

    if (secure)
    {
        client = &HTTPSClient;
    }
    else
    {
        client = &HTTPClient;
    }

    // Use WiFiClientSecure class to create SSL connection
    printf("Connecting to   : %s\n", host);
    //Serial.println("Connecting to   : " + String(host));
    if (!client->connect(host, httpsPort))
    {
        Serial.println("Connection failed");
        return false;
    }
    printf("Requesting      : %s\n", url);
    //Serial.println("Requesting      : " + String(url));
    client->print(String("GET ") + url + " HTTP/1.1\r\n" +
                  "Host: " + host + "\r\n" +
                  "User-Agent: G6EJDFailureDetectionFunction\r\n" +
                  "Connection: close\r\n\r\n");
    printf("Request sent    : ");
    //Serial.print("Request sent    : ");
    while (client->connected())
    {
        String line = client->readStringUntil('\n');
        if (line == "\r")
        {
           printf("Headers received\n");
            //Serial.println("Headers received");
            break;
        }
    }

    // if there are incoming bytes available
    // from the server, read them and print them:
    while (client->available())
    {
        char c = client->read();
        printf("%c", c);
        //Serial.write(c);
    }

    client->stop();
    //delete client;
    return true;

    // String line = client->readStringUntil('\n');
    // //Serial.println(line);

    // if (line == "success")
    //     line = "Server confirmed all data received";
    // if (line == "INVALIDPASSWORDID|Password or key and/or id are incorrect")
    // {
    //     line = "Invalid PWS/User data entered in the ID and PASSWORD or GET parameters";
    //     Status = false;
    // }
    // if (line == "RapidFire Server")
    // {
    //     line = "The minimum GET parameters of ID, PASSWORD, action and dateutc were not set correctly";
    //     Status = false;
    // }
    // Serial.println("Server Response : " + line);
    // Serial.println("Status          : Closing connection");
    // return Status;
}

void displayTest()
{
#if defined BOARD_HELTEC
    //clear full display
    Heltec.display->clear();

    ////partial clear display
    //Heltec.display->setColor(BLACK);
    //Heltec.display->fillRect(0, 0, 128, 20);
    //Heltec.display->setColor(WHITE);

    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
    Heltec.display->setFont(ArialMT_Plain_24);

    Heltec.display->drawString(43, 0, "144");
    Heltec.display->drawString(94, 0, "100");
    Heltec.display->drawString(128, 0, "12");

    Heltec.display->setFont(ArialMT_Plain_10);

    Heltec.display->drawString(41, 22, "km/h");
    Heltec.display->drawString(92, 22, "knots");
    Heltec.display->drawString(126, 22, "bft");

    Heltec.display->setFont(ArialMT_Plain_24);

    Heltec.display->drawString(128, 32, "-2.5°C");

    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);

    Heltec.display->drawString(0, 27, "www");

    Heltec.display->setFont(ArialMT_Plain_16);
    Heltec.display->drawString(0, 49, "«360»");

    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);

    bool conn = WiFi.isConnected();
    bool mqConn = mqttClient.connected();

    char statusmsg[64];
    statusmsg[0]=0;
    if (conn)
        strcat(statusmsg, "wifi ok ");
    else
        strcat(statusmsg, "wifi nk ");
    if (mqConn)
        strcat(statusmsg, "mq 2435");
    else
        strcat(statusmsg, "mq 2435");

    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(128, 54, statusmsg);
    Heltec.display->display();
#endif
}

void displayStale()
{
#if defined BOARD_HELTEC
    //clear full display
    Heltec.display->clear();

    ////partial clear display
    //Heltec.display->setColor(BLACK);
    //Heltec.display->fillRect(0, 0, 128, 20);
    //Heltec.display->setColor(WHITE);

    struct timeval tv;
    struct tm *nowtm;
    char tmbuf[64];

    gettimeofday(&tv, NULL);
    nowtm = localtime(&lastWSts);
    strftime(tmbuf, sizeof tmbuf, "%Y-%m-%d %H:%M:%S", nowtm);

    Heltec.display->drawString(0, 0, "laatste update op ");
    Heltec.display->drawString(0, 10, tmbuf);

    bool conn = WiFi.isConnected();
    bool mqConn = mqttClient.connected();

    char statusmsg[64];
    statusmsg[0] = 0;
    if (conn)
        strcat(statusmsg, "wifi ok ");
    else
        strcat(statusmsg, "wifi -- ");
    if (mqConn)
        strcat(statusmsg, "mq ok  ");
    else
        strcat(statusmsg, "mq --  ");

    Heltec.display->setFont(ArialMT_Plain_10);
    Heltec.display->drawString(0, 54, statusmsg);
    Heltec.display->display();
#endif
}

int beaufort(double windspeed)
{
    double bft_kmh[] = {0.999, 5.5, 11.5, 19.5, 28.5, 38.5, 49.5, 61.5, 74.5, 88.5, 102.5, 117.5, 1000};

    int bft = 0;
    while (bft < 13 && bft_kmh[bft] < windspeed)
        ++bft;
    return bft;
}

void display(WSBase *wsp)
{
#if defined BOARD_HELTEC
    //clear full display
    Heltec.display->clear();

    ////partial clear display
    //Heltec.display->setColor(BLACK);
    //Heltec.display->fillRect(0, 0, 128, 20);
    //Heltec.display->setColor(WHITE);

    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
    Heltec.display->setFont(ArialMT_Plain_24);

    char oledmsg[60];
    double value = wsp->windspeed;
    if (value > 19.95)
        sprintf(oledmsg, "%ld", lround(value));
    else
        sprintf(oledmsg, "%2.1f", value);
    Heltec.display->drawString(43, 0, oledmsg);

    value = 0.540 * wsp->windspeed;
    if (value > 19.95)
        sprintf(oledmsg, "%ld", lround(value));
    else
        sprintf(oledmsg, "%2.1f", value);
    Heltec.display->drawString(94, 0, oledmsg);

    sprintf(oledmsg, "%d", beaufort(wsp->windspeed));
    Heltec.display->drawString(128, 0, oledmsg);

    Heltec.display->setFont(ArialMT_Plain_10);

    Heltec.display->drawString(41, 22, "km / h");
    Heltec.display->drawString(92, 22, "knopen");
    Heltec.display->drawString(126, 22, "bft");

    Heltec.display->setFont(ArialMT_Plain_24);

    sprintf(oledmsg, "%2.1f°C", wsp->temperature);
    Heltec.display->drawString(128, 32, oledmsg);

    Heltec.display->setTextAlignment(TEXT_ALIGN_LEFT);

    const char *compass[] = {"n  ", "nno", "no ", "ono", "o  ", "ozo", "zo ", "zzo", "z  ", "zzw", "zw ", "wzw", "w  ", "wnw", "nw ", "nnw"};
    sprintf(oledmsg, "%3s", compass[((4 * wsp->winddir + 45) / 90) & 0x0F]);
    Heltec.display->drawString(0, 27, oledmsg);

    Heltec.display->setFont(ArialMT_Plain_16);
    sprintf(oledmsg, "«%3d»", wsp->winddir);
    Heltec.display->drawString(0, 49, oledmsg);

    Heltec.display->setTextAlignment(TEXT_ALIGN_RIGHT);
    Heltec.display->setFont(ArialMT_Plain_10);

    sprintf(oledmsg, "%02X%02X ", (wsp->msgformat & 0xFF), (wsp->stationID & 0xFF));
    bool conn = WiFi.isConnected();
    bool mqConn = mqttClient.connected();

    char statusmsg[64];
    statusmsg[0] = 0;
    if (conn)
        strcat(statusmsg, "wifi ok mq ");
    else
        strcat(statusmsg, "wifi -- mq ");
    if (mqConn)
        strncpy(statusmsg, oledmsg, 6);
    else
        strcat(statusmsg, "xxxx");
    Heltec.display->drawString(128, 54, statusmsg);
    Heltec.display->display();

    // static bool alt = true;
    // alt = !alt;

    // //clear full display
    // Heltec.display->clear();

    // ////partial clear display
    // //Heltec.display->setColor(BLACK);
    // //Heltec.display->fillRect(0, 0, 128, 20);
    // //Heltec.display->setColor(WHITE);

    // Heltec.display->setFont(ArialMT_Plain_24);

    // char oledmsg[60];
    // if (alt)
    //     sprintf(oledmsg, "%2.1fkt", (0.540 * wsp->windspeed));
    // else
    //     sprintf(oledmsg, "%2.1fkm", wsp->windspeed);
    // Heltec.display->drawString(0, 0, oledmsg);

    // if (alt)
    //     sprintf(oledmsg, "%3d°", wsp->winddir);
    // else
    // {
    //     //const char *compass[] = {"N  ", "NNO", "NO ", "ONO", "O  ", "OZO", "ZO ", "ZZO", "Z  ", "ZZW", "ZW ", "WZW", "W  ", "WNW", "NW ", "NNW"};
    //     const char *compass[] = {"n  ", "nno", "no ", "ono", "o  ", "ozo", "zo ", "zzo", "z  ", "zzw", "zw ", "wzw", "w  ", "wnw", "nw ", "nnw"};

    //     sprintf(oledmsg, "%3s", compass[((4 * wsp->winddir + 45) / 90) & 0x0F]);
    // }

    // Heltec.display->drawString(80, 0, oledmsg);

    // sprintf(oledmsg, "%2.1f°C  %2d%%", wsp->temperature, wsp->humidity);
    // Heltec.display->drawString(0, 24, oledmsg);

    // sprintf(oledmsg, "%02X%02X ", (wsp->msgformat & 0xFF), (wsp->stationID & 0xFF));
    // printf("%s\n", oledmsg);

    // bool conn = WiFi.isConnected();
    // bool mqConn = mqttClient.connected();
    // String statusmsg = oledmsg;
    // if (conn)
    //     statusmsg += "WIFI OK, ";
    // else
    //     statusmsg += "WIFI --, ";
    // if (mqConn)
    //     statusmsg += "MQTT OK";
    // else
    //     statusmsg += "MQTT --";

    // Heltec.display->setFont(ArialMT_Plain_10);
    // Heltec.display->drawString(0, 54, statusmsg);
    // Heltec.display->display();
#endif
}

void rfLoop(bool mqConn)
{
    static uint8_t pktbuf[70];
    int len = radio.receive(pktbuf, sizeof(pktbuf));
    if (len <= 0)
        return;
    rfRxNum++;
    digitalWrite(LED_RF, LED_ON);
    rfLed = millis();

    WSBase *ws = wsProcessor.processWSPacket(pktbuf, len, radio.rxAt, radio.rssi, radio.snr, radio.lna, radio.afc);
    if (ws)
    {
        ws->print();

        WSSetting *thisStation = wsConfig.lookup(ws->msgformat, ws->stationID);

        if (thisStation)
        {
            thisStation->update(ws, pktbuf);

            //for OLED display: last configured good packet.
            struct timeval tvnow;
            gettimeofday(&tvnow, NULL);
            lastWSts = tvnow.tv_sec;
        }
        else
        {
            //It is a weather station, but not configured.
            //It may be decoded or unknown but with succesful CRC check
            //report succesful and unknown packets on MQTT. Note: WH1080 burst of upto 6 repeating signals.
            publishWS(ws->mqttPayload().c_str());
        }
        delete ws;
    };

    //report updated stations
    for (int i = 0; i < MAX_WS; i++)
    {
        printf("Reporting %d\n", i);
        WSSetting *thisStation = wsConfig.stations[i];
        printf("ptr %d\n", thisStation);
        if (thisStation && thisStation->reportable())
        {
            printf("reportable\n");
            //report succesful packets on MQTT, but at most one per WH1080 burst of upto 6 repeating signals
            if ((millis() - thisStation->lastReported > 500) && thisStation->wsp)
            {
                printf("MQTT\n");
                publishWS(thisStation->wsp->mqttPayload().c_str());
                display(thisStation->wsp);
            }

            //report to API's at most once per 60 seconds
            if (millis() - thisStation->lastReported > 60000)
            {
                //struct timeval tvnow;
                //gettimeofday(&tvnow, NULL);
                //printf("time %ld s\n", tvnow.tv_sec);

                thisStation->lastReported = millis();
                if (thisStation->wunderground)
                {
                    UploadToWebAPI("weatherstation.wunderground.com", 443, true,
                                   thisStation->urlWunderground(thisStation->wuID, thisStation->wuPW).c_str());
                    //printf("%s\n", ws->urlWunderground().c_str());
                };
                if (thisStation->domoticz)
                {
                    if (thisStation->dzTHidx > 0)
                    {
                        UploadToWebAPI(thisStation->dzURL, thisStation->dzPort, thisStation->dzSecure,
                                       thisStation->urlDomoticzTemp(thisStation->dzTHidx, thisStation->dzID, thisStation->dzPW).c_str());
                        //printf("%s\n", ws->urlDomoticzTemp(thisStation->dzTHidx, thisStation->dzID, thisStation->dzPW).c_str());
                    }
                    if (thisStation->dzWidx > 0)
                    {
                        UploadToWebAPI(thisStation->dzURL, thisStation->dzPort, thisStation->dzSecure,
                                       thisStation->urlDomoticzWind(thisStation->dzWidx, thisStation->dzID, thisStation->dzPW).c_str());
                        //printf("%s\n", ws->urlDomoticzWind(thisStation->dzWidx, thisStation->dzID, thisStation->dzPW).c_str());
                    }
                    if (thisStation->dzRidx > 0)
                    {
                        UploadToWebAPI(thisStation->dzURL, thisStation->dzPort, thisStation->dzSecure,
                                       thisStation->urlDomoticzRain(thisStation->dzRidx, thisStation->dzID, thisStation->dzPW).c_str());
                        //printf("%s\n", ws->urlDomoticzRain(thisStation->dzRidx, thisStation->dzID, thisStation->dzPW).c_str());
                    }
                    if (thisStation->dzLidx > 0)
                    {
                        UploadToWebAPI(thisStation->dzURL, thisStation->dzPort, thisStation->dzSecure,
                                       thisStation->urlDomoticzLight(thisStation->dzLidx, thisStation->dzID, thisStation->dzPW).c_str());
                        //printf("%s\n", ws->urlDomoticzLight (thisStation->dzLidx, thisStation->dzID, thisStation->dzPW).c_str());
                    }
                    if (thisStation->dzUVidx > 0)
                    {
                        UploadToWebAPI(thisStation->dzURL, thisStation->dzPort, thisStation->dzSecure,
                                       thisStation->urlDomoticzUV(thisStation->dzUVidx, thisStation->dzID, thisStation->dzPW).c_str());
                        //printf("%s\n", ws->urlDomoticzUV(thisStation->dzUVidx, thisStation->dzID, thisStation->dzPW).c_str());
                    }
                }
                if (thisStation->windguru)
                {
                    UploadToWebAPI("www.windguru.cz", 80, false, thisStation->urlWindguru(thisStation->wgUID, thisStation->wgPW).c_str());
                    printf("%s\n", thisStation->urlWindguru(thisStation->wgUID, thisStation->wgPW).c_str());
                    // printf("%s\n", ws->urlWindguru("stationXY", "supersecret").c_str());
                }
                printf("\n\nDone report\n");
            }
        }
    }
    //TODO:
    //publish failed packets to MQTT

    //clear display when no recent data is received.
    struct timeval tvnow;
    gettimeofday(&tvnow, NULL);
    if (tvnow.tv_sec - lastWSts > 300)
    {
        displayStale();
    }
}

extern uint32_t mqPingMs;

void report()
{
    printf("vBatt = %dmV\n", vBatt);

    char buf[256];
    int len = snprintf(buf, sizeof(buf),
                       "{\"uptime\":%d,\"rssi\":%d,\"heap\":%d,\"mVbatt\":%d,\"version\":\"%s\"",
                       uint32_t(esp_timer_get_time() / 1000000), WiFi.RSSI(), ESP.getFreeHeap(), vBatt, __DATE__);
    len += snprintf(buf + len, sizeof(buf) - len, ",\"rfRx\":%d,\"rfNoise\":%d",
                    rfRxNum, -(radio.bgRssi >> 5));
    len += snprintf(buf + len, sizeof(buf) - len,
                    ",\"mqttTx\":%d,\"mqttRx\":%d,\"ping\":%d",
                    mqttTxNum, mqttRxNum, mqPingMs);
    buf[len++] = '}';
    buf[len] = 0;

    // send off the packet
    char topic[41 + 6];
    strcpy(topic, mqTopic);
    strcat(topic, "/stats");
    printf("MQTT TX stats len=%d\n", len);
    mqttClient.publish(topic, 1, false, buf, len, false);
    //printf("JSON: %s\n", buf);
}

//DEBUG wifi disconencts
void WiFiEvent(WiFiEvent_t event, system_event_info_t info)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_START:
        printf("Station Mode Started\n");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        printf("Connected to : %s\n", WiFi.SSID().c_str());
        printf("Got IP: %s\n", WiFi.localIP().toString().c_str());
        //Serial.print("Got IP: ");
        //Serial.println(WiFi.localIP());
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        printf("Disconnected from station, attempting reconnection\n");
        WiFi.reconnect();
        break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
        printf("WPS Successful, stopping WPS and connecting to: %s", WiFi.SSID().c_str());
        //Serial.println("WPS Successful, stopping WPS and connecting to: " + WiFi.SSID());
        //esp_wifi_wps_disable();
        //delay(10);
        //WiFi.begin();
        break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
        printf("WPS Failed, retrying\n");
        //esp_wifi_wps_disable();
        //esp_wifi_wps_enable(&config);
        //esp_wifi_wps_start(0);
        break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
        printf("WPS Timeout, retrying\n");
        //esp_wifi_wps_disable();
        //esp_wifi_wps_enable(&config);
        //esp_wifi_wps_start(0);
        break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
        char wps_pin[9];
        for (int i = 0; i < 8; i++)
        {
            wps_pin[i] = info.sta_er_pin.pin_code[i];
        }
        wps_pin[8] = '\0';
        printf("WPS_PIN = %s\n", wps_pin);
        //Serial.println("WPS_PIN = " + wpspin2string(info.sta_er_pin.pin_code));
        break;
    default:
        break;
    }
}

//===== Setup

void setup()
{
    Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);

    Serial.begin(115200);

    printf("\n===== ESP32 RF Gateway =====\n");
    printf("Running ESP-IDF %s\n", ESP.getSdkVersion());
    printf("Board type: %s\n", ARDUINO_BOARD);

    displayTest();

    config.read(); // read config file from flash
    cmd.init();    // init CLI
    mqttSetup(config);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onMessage(onMqttMessage);
    //mqttClient.onPublish(onMqttPublish);

    //DEBUG
    WiFi.onEvent(WiFiEvent);
    //DEBUG

    WiFi.mode(WIFI_STA); // start getting wifi to connect
    WiFi.begin();

    setenv("TZ", "UTC-01:00", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    //NOTE: commented this out to investigate unexpected WiFi disconnect
    //ip_addr_t sntpip = IPADDR4_INIT_BYTES(192, 168, 1, 1); // will be updated when wifi connects
    //sntp_setserver(1, &sntpip);
    ////sntp_servermode_dhcp(1); // not supported in arduino-esp32
    sntp_setservername(0, (char *)"pool.ntp.org"); // esp-idf compiled with max 1 NTP server...
    sntp_init();

    // radio init
    printf("Initializing radio\n");
    spi.begin(RF_CLK, RF_MISO, RF_MOSI);
    radio.init(rfId, rfGroup, rfFreq);
    //Do not use interrupts for weatherstation application
    //radio.setIntrPins(RF_DIO0, RF_DIO4);
    radio.setIntrPins(-1, -1);
    radio.txPower(rfPow);
    radio.setMode(SX1276fsk::MODE_STANDBY);

    //nrw.setup();
    //printf("pktBuf size = %d\n", pktBuffer.size());

    pinMode(LED_MQTT, OUTPUT);
    digitalWrite(LED_MQTT, LED_OFF);
    pinMode(LED_RF, OUTPUT);
    digitalWrite(LED_RF, LED_OFF);
    pinMode(LED_WIFI, OUTPUT);
    digitalWrite(LED_WIFI, LED_ON);

#ifdef VBATT
    analogSetup(VBATT);
#endif

    delay(200);

    // //TODO: Remove this testcode
    printf("stationconfig constr\n");
    WSConfigTest myTest; //should print some json text
    // printf("stationconfig test\n");
    myTest.test();

    std::string firstStation = myTest.ws.serialize();
    // //WSConfig wsConfig;

    // //Load the weather station configuration from flash memory
    wsConfig.load();

    // //for the moment update settings first entry from  stationconfig.h
    wsConfig.add(firstStation);

    // printf("wsconfig setup code\n");

    // //check whether we can find station in list
    // WSSetting *myStation = wsConfig.lookup(0x24, 0x3C);
    // if (myStation)
    // {
    //     printf("found it: %s\n", myStation->dzURL);
    // }
    // else
    // {
    //     printf("not found\n");
    // }

    // WSSetting *myStation2 = wsConfig.lookup(0x23, 0x3C);
    // if (myStation2)
    // {
    //     printf("found it: %s", myStation2->dzURL);
    // }
    // else
    // {
    //     printf("not found\n");
    // }

    //Load the weather station configuration from flash memory
    wsConfig.load();

    printf("===== Setup complete\n");
}

uint32_t lastWiFiConn = millis();
uint32_t lastInfo = -1000000;
bool wifiConn = false;
uint32_t lastReport = -50 * 1000;

void loop()
{
    // print wifi/mqtt info every now and then
    bool conn = WiFi.isConnected();
    bool mqConn = mqttClient.connected();
    if (conn != wifiConn || mqConn != mqttConn || millis() - lastInfo > 20000)
    {
        //WiFi.printDiag(Serial);
        printf("* Wifi:%s %s | MQTT:%s\n",
               conn ? WiFi.SSID().c_str() : "---",
               WiFi.localIP().toString().c_str(),
               mqConn ? config.mqtt_server : "---");
        if (conn != wifiConn && conn)
        {
            // when we connect we set the SNTP server #0 to the IP address of the gateway, which
            // tends to be the NTP server on the LAN in 99% of cases. It would be nice if
            // arduino-esp32 supported DHCP discovery of the NTP serevr, but it doesn't...
            // NOTE: commented this out to investigate unexpected wifi disconnects.
            // ip_addr_t sntpip = IPADDR4_INIT((uint32_t)(WiFi.gatewayIP()));
            // sntp_setserver(1, &sntpip);
        }
        lastInfo = millis();
        wifiConn = conn;
        mqttConn = mqConn;
        if (mqttLed == 0)
            digitalWrite(LED_WIFI, (wifiConn && mqConn) ? LED_OFF : LED_ON);
    }
    //handle WiFi reconnect
    if (conn)
    {
        lastWiFiConn = millis();
    }
    else
    {
        if (millis() - lastWiFiConn > 20000)
        {
            //reconnect code

            //at least wait another 20 secs before retry
            lastWiFiConn = millis();
        }
    }

#ifdef VBATT
    uint32_t vB = 2 * analogSample(VBATT);
    vB = vB * 4046 / 4096;
    vBatt = (vBatt * 15 + vB) / 16;
#endif

    rfLoop(mqConn);
    if (mqConn && millis() - lastReport > 20 * 1000)
    {
        report();
        lastReport = millis();
    }

    if (mqttLed != 0 && millis() - mqttLed > 200)
    {
        digitalWrite(LED_MQTT, LED_OFF);
        mqttLed = 0;
    }

    if (rfLed != 0 && millis() - rfLed > 200)
    {
        digitalWrite(LED_RF, LED_OFF);
        rfLed = 0;
    }

    //mqtt ping
    mqttLoop();
    //process CLI commands
    cmd.loop();
}
