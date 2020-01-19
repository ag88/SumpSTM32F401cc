/*
 * Copyright 2020
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Author: Andrew Goh
 */
#ifndef SAMPLERDMA_H
#define SAMPLERDMA_H
#include <Arduino.h>
#include <HardwareTimer.h>
#include "PwmOut.h"
#include <libmaple/dmaF4.h>

#define SUMP_RESET 0x00
#define SUMP_ARM   0x01
#define SUMP_QUERY 0x02
#define SUMP_SELFTEST   0x03
#define SUMP_GET_METADATA 0x04
#define SUMP_RLE_FINISH 0x05
#define SUMP_XON 0x11
#define SUMP_XOFF 0x13
#define SUMP_SET_DIVIDER 0x80
#define SUMP_SET_READ_DELAY_COUNT 0x81
#define SUMP_SET_FLAGS 0x82
#define SUMP_SET_TRIGGER_MASK 0xC0
#define SUMP_SET_TRIGGER_VALUES 0xC1
#define SUMP_SET_TRIGGER_CONFIG 0xC2

#define FLAGS_DEMUX 1
#define FLAGS_FILTER 2
#define FLAGS_CHANNEL_GROUPS 0x3C
#define FLAGS_EXTERNAL 0x40
#define FLAGS_INVERTED 0x80
#define FLAGS_TEST 0x400

#define BUFFER_SIZE 32768
#define SAMPRATE_MAX 10000000

void blinks(unsigned int onTime, unsigned int offTime, unsigned int num);

uint8_t i2lebyte(uint32_t value, uint8_t pos);
uint16_t lea2short(uint8_t *buf, uint8_t pos);
uint32_t lea2uint(uint8_t *buf, uint8_t pos);
uint8_t getbit(uint32_t v, uint8_t pos);


class SUMPbackend{

public:

    SUMPbackend();

    void init();

    void start();
    void arm();
    void stop();
    void reset();

    void setDivider(uint32_t);
    void setCount(uint32_t);
    void setDelay(uint16_t);
    void setTrigMask(uint32_t);
    void setTrigValue(uint32_t);
    void setTrigEnabled(uint8_t);
    void setFlags(uint32_t);

    void sendMetadata();
    void printParam();

private:

    uint32_t sampleperiod;
    uint32_t count;
    uint32_t delay;
    uint32_t triggerMask;
    uint32_t triggerValue;
    uint8_t  triggerEnabled;
    uint32_t flags;

    //dma stream for timer trigger
    const dma_stream dmastream = DMA_STREAM5;

    uint16_t setTimerPeriod(uint32_t nsecs);
    void setupDMA();

};

extern enum runstate {
	RINIT = 0,
	RUNNING,
	RCMPLT,
	RERR
} g_runstate;


#endif
