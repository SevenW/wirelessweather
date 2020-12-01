//template <typename SPI>
class SX1276ws : public SX1276fsk
{
    //uint8_t myId;
    //uint8_t parity;

    //This can be removed when puul request to tve is accepted.
    enum
    {
        IRQ2_FIFOEMPTY = 1 << 6
    };

public:
    SX1276ws(SPIClass &spi_, int8_t ss_, int8_t reset_ = -1)
        : SX1276fsk(spi_, ss_, reset_){};
    void init(uint8_t id, uint8_t group, int freq);
    int receive(void *ptr, int len);
    int readPacket(void *ptr, int len);
};

//template <typename SPI>
//SX1276ws::SX1276ws()
//{
//}

//template <typename SPI>
void SX1276ws::init(uint8_t id, uint8_t group, int freq)
{
    SX1276fsk::init(id, group, freq);
    this->writeReg(0x02, 0x07); // bitrate 17.241
    this->writeReg(0x03, 0x40);
    this->writeReg(0x04, 0x03); // Fdev 60kHz
    this->writeReg(0x05, 0xD7);
    this->writeReg(0x06, 0xD9); // F 868.35MHz
    this->writeReg(0x07, 0x16);
    this->writeReg(0x08, 0x66);

    //TODO: Can be removed when SX1276FSK pull request is accepted.
    this->writeReg(0x19, 0x12); //BW88
    this->writeReg(0x1a, 0x0a); //AFCBW100
    //this->writeReg(0x19, 0x11); //BW166
    //this->writeReg(0x1a, 0x01); //AFCBW250

    //this->writeReg(0x1E, 0x08); //NoAFC

    //TODO: Can be removed when SX1276FSK pull request is accepted.
    this->writeReg(0x1F, 0xA8); // 2 byte preamble detector, tolerate 8 chip errors

    //this->writeReg(0x29, 0xB0); // RssiThresh - dynamic in base class driver!

    //this form of timeout is not supported on sx1276!
    //this->writeReg(0x2B, 0x0C); // timeout after RSSI detected when payloadready does not occur

    this->writeReg(0x27, 0x11); // 2 syncwords (instead of 3)
    this->writeReg(0x28, 0x2D); // Sync values
    this->writeReg(0x29, 0xD4);

    this->writeReg(0x30, 0x00); // PacketConfig1 = fixed, no whitening, no crc, no filtering

    this->writeReg(0x32, 0x11); // PayloadLength = 66 max

    this->restartRx();

    printf("init done\n");
}

int SX1276ws::receive(void *ptr, int len)
{
    if (mode != MODE_RECEIVE)
    {
        if (transmitting())
            return -1;
        rssi = 0;
        snr = 0;
        lna = 0;
        afc = 0;
        rxAt.tv_sec = 0;
        rxAt.tv_usec = 0;
        setMode(MODE_RECEIVE);
        return -1;
    }

    // if a packet has started, read the RSSI, AFC, etc stats
    if ((readReg(REG_IRQFLAGS1) & IRQ1_PREAMBLEDETECT) != lastFlag)
    {
        lastFlag ^= IRQ1_PREAMBLEDETECT;
        if (lastFlag)
        {
            // got a preamble-detect interrupt, need to read RSSI, AFC, etc.
            readRSSI();
            printf("[RSSI%d]", -rssi / 2);
        }
    }

    // if a packet of maximum length has been received, fetch it
    if (readReg(REG_IRQFLAGS2) & IRQ2_PAYLOADREADY)
    {
        printf("[full RX]");
        return this->readPacket(ptr, len);
    }

    // uint8_t F1 = readReg(REG_IRQFLAGS1);
    // //if (F1 & IRQ1_MODEREADY)
    // //    printf("M");
    // if (F1 & IRQ1_RXREADY)
    //     printf("X");
    // //if (F1 & 0b1000)
    // //    printf("R");
    // if (F1 & IRQ1_PREAMBLEDETECT)
    //     printf("P");
    // if (F1 & IRQ1_SYNADDRMATCH)
    //     printf("S ");

    // if the radio detected a preamble and it doesn't get a packet (sync address match) within a
    // few ms restart RX so it performs a fresh AFC/AGC for the next actual packet.
    // Preamble+sync is 5+3=8 bytes, @49230 baud that's 1.3ms.
    bool synAddrMatch = (readReg(REG_IRQFLAGS1) & IRQ1_SYNADDRMATCH) != 0;
    uint32_t uNow = micros();
    //Timeout after preamble detection
    // - received proper package with length shorter than maximum length
    // - or did not pass the SYNCWORD check
    // - or did, but did not receive further bytes (FifoEmpty)
    //TODO: This timeout needs to be implemented using actual bit rate and maximum packet length
    if (rssiAt != 0 && uNow - rssiAt > 12000)
        if (synAddrMatch && !(readReg(REG_IRQFLAGS2) & IRQ2_FIFOEMPTY))
        {
            printf("[shorter RX]");
            return this->readPacket(ptr, len);
        }
        else
        {
            restartRx();
            printf("SX1276fsk: RX restart (RSSI thres is %ddBm)\n", -readReg(REG_RSSITHRES) / 2);
        }
    else if (rssiAt == 0 && !synAddrMatch && uNow - bgRssiAt > 10 * 1000)
    {
        // read RSSI and do some smoothed tracking of background noise
        uint16_t r = bgRssi >> 4;
        uint16_t v = readReg(REG_RSSIVALUE);
        if (v > 2 * 70 && v < 2 * 100)
        {                                             // reject non-sensical values
            bgRssi = ((bgRssi * 15) + (v << 4)) >> 4; // exponential smoothing
            bgRssiAt = uNow;
            if ((bgRssi >> 4) != r)
            {
                //printf("SD1276fsk: bgRssi %d->%d (%d)\n", r, bgRssi>>4, v);
                writeReg(REG_RSSITHRES, (bgRssi >> 4) + 2 * 2); // set threshold a couple of dB above noise
            }
        }
    }
    return -1;
}

int SX1276ws::readPacket(void *ptr, int len)
{
    //uint32_t dt = micros() - intr0At;
    gettimeofday(&rxAt, 0);
    if (rxAt.tv_sec < 1500000000)
    {
        // we don't seem to have the real time :-(
        rxAt.tv_sec = 0;
        rxAt.tv_usec = 0;
    }
    // With polling IRQ1Flags this accuracy is not reached
    // else
    // {
    //     // adjust current time for the delta since we saw the RX interrupt
    //     rxAt.tv_sec -= dt / 1000000;
    //     rxAt.tv_usec -= dt % 1000000;
    //     while (rxAt.tv_usec < 0)
    //     {
    //         rxAt.tv_usec += 1000000;
    //         rxAt.tv_sec--;
    //     }
    // }

    //delay(10); //wait for the packet?
    //printf("ÏRQ2 %02x", (this->readReg(this->REG_IRQFLAGS2)));
    int i = 0;
    while (!(this->readReg(this->REG_IRQFLAGS2) & this->IRQ2_FIFOEMPTY))
    {
        uint8_t v = this->readReg(this->REG_FIFO);
        printf("%02x ", v);
        if (i < len)
        {
            ((uint8_t *)ptr)[i++] = v;
            //printf("%02x ", v);
        }
    }
    printf("\n");

    /*
    spi.beginTransaction(spiSettings);
    digitalWrite(ss, LOW);
    spi.write(REG_FIFO);

    uint8_t count = spi.transfer(0); // first byte of packet is length
    if (count <= len)
    {
        spi.transferBytes((uint8_t *)ptr, (uint8_t *)ptr, count);
    }
    else
    {
        spi.transferBytes((uint8_t *)ptr, (uint8_t *)ptr, len);
        for (int i = len; i < count; i++)
            spi.transfer(0);
    }
    digitalWrite(ss, HIGH);
    spi.endTransaction();
    */

    // This work is done by restartRx();
    // // flag stale RSSI
    // if (micros() - rssiAt > 60000)
    // {
    //     printf("!RSSI stale:%ldus!", micros() - rssiAt);
    //     rssi = 0;
    //     snr = 0;
    //     afc = 0;
    //     lna = 0;
    // }
    // need to restartRX to get proper fresh AFC/AGC
    //uint32_t dt2 = micros() - intr0At;
    //printf("!RX:%luus:%luus!", dt, micros()-intr0At);
    printf("restarting after succesful packet\n");
    restartRx();

    return (i == 0)
               ? -1
               : i;

    // // only accept packets intended for us, or broadcasts
    // // ... or any packet if we're the special catch-all node
    // uint8_t dest = *(uint8_t *)ptr;
    // if ((dest & 0xC0) == parity)
    // {
    //     uint8_t destId = dest & 0x3F;
    //     if (destId == myId || destId == 0 || myId == 63)
    //         return count;
    // }
    // return -1;
}
