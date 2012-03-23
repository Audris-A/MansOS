/**
 * Copyright (c) 2011, Institute of Electronics and Computer Science
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of  conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * cc1101.c -- CC1101 radio driver
 *
 * TODO: Replace busy-wait loops with something better
 */

#include <hil/gpio.h>
#include <hil/errors.h>
#include <hil/spi.h>
#include <cc1101_pins.h>

#include "cc1101.h"


/*
 * SPI access shortcuts
 */

static inline uint8_t readByte(void)
{
    return spiReadByte(CC1101_SPI_ID);
}

static inline void writeByte(uint8_t data)
{
    spiWriteByte(CC1101_SPI_ID, data);
}

static inline uint8_t exchByte(uint8_t data)
{
    return spiExchByte(CC1101_SPI_ID, data);
}

static inline void chipSelect(void)
{
    pinClear(CC1101_CSN_PORT, CC1101_CSN_PIN);
}

static inline void chipRelease(void)
{
    pinSet(CC1101_CSN_PORT, CC1101_CSN_PIN);
}


/*
 * SPI header byte
 */

#define HEADER_RW    (1 << 7)
#define HEADER_BURST (1 << 6)


/*
 * Radio registers and strobes
 */
enum reg {
    /* Configuration registers */
    REG_IOCFG2     = 0x01,
    REG_IOCFG1     = 0x02,
    REG_IOCFG0     = 0x03,
    REG_FIFOTHR    = 0x04,
    REG_SYNC1      = 0x05,
    REG_SYNC0      = 0x06,
    REG_PKTLEN     = 0x07,
    REG_PKTCTRL1   = 0x08,
    REG_PKTCTRL0   = 0x09,
    REG_ADDR       = 0x0A,
    REG_CHANNR     = 0x0B,
    REG_FSCTRL1    = 0x0C,
    REG_FSCTRL0    = 0x0D,
    REG_FREQ2      = 0x0E,
    REG_FREQ1      = 0x0F,
    REG_FREQ0      = 0x10,
    REG_MDMCFG4    = 0x11,
    REG_MDMCFG3    = 0x12,
    REG_MDMCFG2    = 0x13,
    REG_MDMCFG1    = 0x14,
    REG_MDMCFG0    = 0x15,
    REG_DEVIATN    = 0x16,
    REG_MCSM2      = 0x17,
    REG_MCSM1      = 0x18,
    REG_MCSM0      = 0x19,
    REG_FOCCFG     = 0x1A,
    REG_BSCFG      = 0x1B,
    REG_AGCCTRL2   = 0x1C,
    REG_AGCCTRL1   = 0x1D,
    REG_AGCCTRL0   = 0x1E,
    REG_WOREVT1    = 0x1F,
    REG_WOREVT0    = 0x20,
    REG_WORCTRL    = 0x21,
    REG_FREND1     = 0x22,
    REG_FREND0     = 0x23,
    REG_FSCAL3     = 0x24,
    REG_FSCAL2     = 0x25,
    REG_FSCAL1     = 0x26,
    REG_FSCAL0     = 0x27,
    REG_RCCTRL1    = 0x28,
    REG_RCCTRL0    = 0x29,
    REG_FSTEST     = 0x2A,
    REG_PTEST      = 0x2B,
    REG_AGCTEST    = 0x2C,
    REG_TEST2      = 0x2D,
    REG_TEST1      = 0x2E,
    REG_TEST0      = 0x2F,

    /* Command strobes */
    STROBE_SRES    = 0x30,
    STROBE_SFSTXON = 0x31,
    STROBE_SXOFF   = 0x32,
    STROBE_SCAL    = 0x33,
    STROBE_SRX     = 0x34,
    STROBE_STX     = 0x35,
    STROBE_SIDLE   = 0x36,
    STROBE_SWOR    = 0x38,
    STROBE_SPWD    = 0x39,
    STROBE_SFRX    = 0x3A,
    STROBE_SFTX    = 0x3B,
    STROBE_SWORRST = 0x3C,
    STROBE_SNOP    = 0x3D,

    /*
     * Status registers. The burst bit is used to tell them apart from strobes
     * with the same number.
     */
#define STREG(n) ((n) | HEADER_BURST)
    REG_PARTNUM    = STREG(0x30),
    REG_VERSION    = STREG(0x31),
    REG_FREQEST    = STREG(0x32),
    REG_LQI        = STREG(0x33),
    REG_RSSI       = STREG(0x34),
    REG_MARCSTAT   = STREG(0x35),
    REG_WORTIME1   = STREG(0x36),
    REG_WORTIME2   = STREG(0x37),
    REG_PKTSTATUS  = STREG(0x38),
    REG_VCO_VC_DAC = STREG(0x39),
    REG_TXBYTES    = STREG(0x3A),
    REG_RXBYTES    = STREG(0x3B),
    REG_RCCTRL1_STATUS = STREG(0x3C),
    REG_RCCTRL0_STATUS = STREG(0x3D),
#undef STREG

    /* Multibyte registers */
    REG_PATABLE    = 0x3E,
    REG_FIFO       = 0x3F, /* RX/TX FIFO */
};

typedef enum reg reg_t;


/*
 * Various register settings
 */
 
#define IOCFG_HIGH_IMP      0x2E       /* High impedance */
#define IOCFG_PACKET_CRC_OK 0x7        /* Assert on valid packet received */
                                       /* TODO: Can we also handle RX overflow
                                          with only this interrupt? */

#define PKTCTRL_CRC_EN      (1 << 2)   /* Enable hardware CRC checking */
#define PKTCTRL_VARLENGTH   0x1        /* Variable packet length mode */
#define PKTCTRL_AUTOFLUSH   (1 << 3)   /* Auto flush RX FIFO if CRC fails */
#define PKTCTRL_APPEND      (1 << 2)   /* Append packet status to the payload */

#define MDMCFG_SYNC_MODE    0x3        /* 30/32 bits sync mode */
#define MDMCFG_NUM_PREAMBLE (0x2 << 4) /* 4 preamble bytes */

#define MCSM_FS_AUTOCAL     (0x1 << 4) /* Recalibrate FS every time */
#define MCSM_TXOFF_MODE     0x3        /* Return to RX after completing send */


/*
 * Radio status byte
 */

#define STATUS_READY (1 << 7)

typedef enum state {
    STATE_IDLE         = 0,
    STATE_RX           = 1,
    STATE_TX           = 2,
    STATE_FSTXON       = 3,
    STATE_CALIBRATE    = 4,
    STATE_SETTLING     = 5,
    STATE_RX_OVERFLOW  = 6,
    STATE_TX_UNDERFLOW = 7
} state_t;

static inline state_t getState(uint8_t status)
{
    return (status >> 4) & 0x7;
}

static inline uint8_t getFIFOAvail(uint8_t status)
{
    return status & 0xf;
}


/*
 * Radio register access
 */

static inline uint8_t getreg(reg_t reg)
{
    writeByte(reg | HEADER_RW);
    return readByte();
}

static inline void setreg(reg_t reg, uint8_t data)
{
    writeByte(reg);
    writeByte(data);
}

static void burstRead(reg_t start, void *buf, uint8_t len)
{
    writeByte(start | HEADER_BURST | HEADER_RW);
    spiRead(CC1101_SPI_ID, buf, len);
        
    chipRelease(); /* End burst access */
    chipSelect();
}

static void burstWrite(reg_t start, const void *data, uint8_t len)
{
    writeByte(start | HEADER_BURST);
    spiWrite(CC1101_SPI_ID, data, len);
    
    chipRelease(); /* End burst access */
    chipSelect();
}

static inline void strobe(reg_t num)
{
    writeByte(num);
}


/*
 * Status handling
 */

static inline uint8_t status(void)
{
    return exchByte(STROBE_SNOP);
}

static inline void waitForReady(void)
{
    /*
     * Sometimes we need to wait until the SO pin on the radio chip goes low,
     * which indicates that the radio crystal is up (p. 29). But the SO pin is
     * used by the SPI module. This code temporarily steals the pin, then
     * reverts it to its original function. Improvements welcome.
     */
    pinAsInput(CC1101_SO_PORT, CC1101_SO_PIN);
    while (pinRead(CC1101_SO_PORT, CC1101_SO_PIN));
    pinAsFunction(CC1101_SO_PORT, CC1101_SO_PIN);
}

static inline void waitForListen(void)
{
    /*
     * Wait until the chip is in the RX state, which means that no outgoing
     * packets are pending
     */
    while (getState(status()) != STATE_RX);
}


/*
 * Radio initialization
 */

void cc1101Init(void)
{
    spiBusInit(CC1101_SPI_ID, SPI_MODE_MASTER);

    pinAsOutput(CC1101_CSN_PORT, CC1101_CSN_PIN);
    pinEnableInt(CC1101_INTR_PORT, CC1101_INTR_PIN);
    pinIntRising(CC1101_INTR_PORT, CC1101_INTR_PIN);

    chipSelect();

    /* We rely on automatic module reset */
    waitForReady();

    /* Setup registers */
    setreg(REG_IOCFG0,   IOCFG_HIGH_IMP);
#define XREG_IOCFG(x) REG_IOCFG ## x
#define REG_IOCFG(x)  XREG_IOCFG(x)
    setreg(REG_IOCFG(CC1101_GDO_INTR), IOCFG_PACKET_CRC_OK);
#undef REG_IOCFG
#undef XREG_IOCFG
    setreg(REG_SYNC1,    CC1101_SYNC_WORD >> 8);
    setreg(REG_SYNC0,    CC1101_SYNC_WORD & 0xff);
    setreg(REG_PKTCTRL1, PKTCTRL_AUTOFLUSH | PKTCTRL_APPEND);
    setreg(REG_PKTCTRL0, PKTCTRL_CRC_EN | PKTCTRL_VARLENGTH);
    setreg(REG_CHANNR,   CC1101_DEFAULT_CHANNEL);
    setreg(REG_MDMCFG2,  MDMCFG_SYNC_MODE);
    setreg(REG_MDMCFG1,  MDMCFG_NUM_PREAMBLE);
    setreg(REG_MCSM1,    MCSM_TXOFF_MODE);
    setreg(REG_MCSM0,    MCSM_FS_AUTOCAL);
    setreg(REG_PATABLE,  CC1101_TX_POWER);

    /* Wait for the user to enable RX */
    strobe(STROBE_SPWD);

    chipRelease();
}


/*
 * On/off functions
 */

void cc1101On(void)
{
    chipSelect();       /* Wakes the radio up */
    waitForReady();
    strobe(STROBE_SRX); /* Go into receive mode */
    chipRelease();
}

void cc1101Off(void)
{
    chipSelect();
    waitForListen();
    strobe(STROBE_SIDLE); /* Might drop a partially received packet */
    strobe(STROBE_SPWD);  /* Will also flush FIFOs */
    chipRelease();
}


/*
 * Configuration functions
 */

static cc1101Callback_t callback;

void cc1101SetChannel(int channel)
{
    chipSelect();
    setreg(REG_CHANNR, channel);
    chipRelease();
}

void cc1101SetTxPower(uint8_t power)
{
    chipSelect();
    setreg(REG_PATABLE, power);
    chipRelease();
}

void cc1101SetAddress(uint8_t addr)
{
    chipSelect();
    setreg(REG_ADDR, addr);
    chipRelease();
}

void cc1101SetRecvCallback(cc1101Callback_t func)
{
    callback = func;
}


/*
 * Signal analysis functions
 */

static int8_t  lastRSSI;
static uint8_t lastLQI;

int8_t cc1101GetRSSI(void)
{
    int8_t res;

    chipSelect();
    res = getreg(REG_RSSI);
    chipRelease();

    return res;
}

int8_t cc1101GetLastRSSI(void)
{
    return lastRSSI;
}

uint8_t cc1101GetLastLQI(void)
{
    return lastLQI;
}


/*
 * Send/receive functions
 */

int8_t cc1101Send(const uint8_t *header, uint8_t hlen,
                  const uint8_t *data,   uint8_t dlen)
{
    uint8_t len = hlen + dlen; /* FIXME: might overflow */

    if (len > CC1101_MAX_PACKET_LEN)
        return -EMSGSIZE;

    chipSelect();
    waitForListen();
    
    setreg(REG_FIFO, len);
    if (header)
        burstWrite(REG_FIFO, header, hlen);
    if (data)
        burstWrite(REG_FIFO, data, dlen);
    
    strobe(STROBE_STX);
    
    chipRelease();
    
    return 0;
}

static inline void flushrx(void)
{
    strobe(STROBE_SIDLE);
    strobe(STROBE_SFRX);
    strobe(STROBE_SRX);
}

int8_t cc1101Read(uint8_t *buf, uint8_t buflen)
{
    uint8_t res, len;
    uint8_t aux[2];

    chipSelect();
    
    len = getreg(REG_FIFO);
    if (len > buflen)
    {
        res = -EMSGSIZE;
        flushrx();
        goto end;
    }

    burstRead(REG_FIFO, buf, len);
    burstRead(REG_FIFO, aux, sizeof(aux));
    lastRSSI = aux[0];
    /*
     * No need to check or remove the CRC bit -- if we got here, the hardware
     * already checked the CRC
     */
    lastLQI  = aux[1]; 
    res = len;

    if (getState(status()) == STATE_RX_OVERFLOW)
    {
        /* If we had overflow, the next packet is damaged */
        flushrx();
    }

end:
    chipRelease();
    return res;
}

void cc1101Discard(void)
{
    chipSelect();
    flushrx();
    chipRelease();
}

bool cc1101IsChannelClear(void)
{
    return true; // XXX TODO
}


/*
 * Interrupt handling
 */

#define XXISR(port, func) ISR(PORT ## port, func)
#define XISR(port, func)  XXISR(port, func)

XISR(CC1101_INTR_PORT, cc1011Interrupt)
{
    if (!callback)
        cc1101Discard();
    else
        callback();
}