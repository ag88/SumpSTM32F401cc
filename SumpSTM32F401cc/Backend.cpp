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

#include <Arduino.h>
#include "PwmOut.h"
#include "Sump.h"

uint8_t  buffer[BUFFER_SIZE];

enum runstate g_runstate;

void dma_handler() {
	Timer1.pause();
	uint8_t flags = dma_get_isr_bits(DMA2, DMA_STREAM5);
	dma_clear_isr_bits(DMA2, DMA_STREAM5);

	flags &= ( ~ ( DMA_ISR_TCIF | DMA_ISR_HTIF ) ) & DMA_ISR_BIT_MASK;

	if (flags)
		g_runstate = runstate::RERR;
	else
		g_runstate = runstate::RCMPLT;
}

SUMPbackend::SUMPbackend()
{
    //init gpiob port for input
    GPIOB_BASE->OSPEEDR = 0xaaaa;    // High Speed
    GPIOB_BASE->MODER = 0;           // Input
    GPIOB_BASE->PUPDR = 0;           // pushpull
    GPIOB_BASE->ODR = 0;			 // clear output

    reset();
}

void SUMPbackend::reset()
{
    triggerMask = 0;
    triggerValue = 0;
    triggerEnabled = 0;
    sampleperiod = 100;
    flags = 0;
    delay = 0;
    g_runstate = runstate::RINIT;
}


void SUMPbackend::init() {
	Timer1.init();
	Timer1.pause();
	setupDMA();
	//trigger dma on update request
	timer_dma_enable_upd_req(TIMER1);
	(TIMER11->regs).adv->CR2 |= TIMER_CR2_CCDS;
}


void SUMPbackend::setupDMA() {
	uint32_t dma_flags;

	dma_init(DMA2);
	dma_disable(DMA2, dmastream);
	dma_clear_isr_bits(DMA2, dmastream);
	dma_flags = DMA_CR_MINC | DMA_CR_DIR_P2M | DMA_CR_TCIE |DMA_CR_PL_MEDIUM;
	dma_flags |= DMA_CR_DMEIE | DMA_CR_TEIE;
	dma_setup_transfer(DMA2, dmastream, DMA_CH6,
			DMA_SIZE_8BITS,
			&GPIOB_BASE->IDR , //IDR input register
			buffer, buffer, //note the 2nd address is for dbl buffer mode, not used but set the same
			dma_flags);
	//use direct mode, no flags set
	dma_set_fifo_flags(DMA2, dmastream, 0U);
	dma_attach_interrupt(DMA2, dmastream, &dma_handler);
	dma_enable(DMA2, dmastream);

}


#define MAX_RELOAD ((1 << 16) - 1)

uint16_t SUMPbackend::setTimerPeriod(uint32_t nsecs) {
    // Not the best way to handle this edge case?
	// limit to 84 mhz / 2 = 42 mhz ~ 24 nsecs
    if (!nsecs || nsecs < 24) {
        Timer1.setPrescaleFactor(1);
        Timer1.setOverflow(1);
        return Timer1.getOverflow();
    }

    uint32 period_cyc = nsecs * CYCLES_PER_MICROSECOND / 1000;
    uint16 prescaler = (uint16)(period_cyc / MAX_RELOAD + 1);
    uint16 overflow = (uint16)(period_cyc / prescaler);
    overflow = overflow==0?1:overflow;
    Timer1.setPrescaleFactor(prescaler);
    Timer1.setOverflow(overflow);
    return overflow;

}

void SUMPbackend::start()
{
    g_runstate = runstate::RINIT;
    
    //set dma transfer size, note check dma xfer word size
    dma_disable(DMA2, dmastream);
    dma_set_num_transfers(DMA2, dmastream, count);
    dma_enable(DMA2, dmastream);

    g_runstate = runstate::RUNNING;
    // get timer ready to run
    Timer1.refresh();

    /*
     * the specs states if read count > delay count, data before
     * the trigger fires are returned.
     * note delay isn't implemented, that in part as dma is used
     */

    //triggering done here, only parallel no serial
    if(triggerEnabled == 1){
        while((GPIOB_BASE->IDR & triggerMask) != triggerValue);
    }

    // start the timer
    Timer1.resume();

    //spinlock till the run is complete
    while(g_runstate == runstate::RUNNING) {
    	asm("wfi");
    }

    Timer1.pause();

    // g_runstate == runstate::RCMPLT; when it reach here
    if (g_runstate == runstate::RERR) {
    	//dma error
    	blinks(100, 100, 10);
    }
    g_runstate = runstate::RINIT;
}



void SUMPbackend::arm()
{
    if (flags & FLAGS_TEST) {
    	pwmout.start();
        start();
        pwmout.stop();
    }else{
        start();
    }

    /* read buffer in reverse, for some reason,
     * gui seemed to display last item first
    for(int i=count-1; i>=0; i--)
    	Serial.write((uint8_t) buffer[i]);
    */

    Serial.write(buffer,count);

}

void SUMPbackend::stop()
{
}


void SUMPbackend::setDivider(uint32_t divider)
{
	/* Sump use a 100 Mhz clock in the original specs
	 * sampling frequency = 100 mhz / (divider + 1)
	 */

	//samplingPeriod in nsecs
	sampleperiod = (divider+1)*10;

	setTimerPeriod(sampleperiod);
}

void SUMPbackend::setCount(uint32_t n)
{
	if(n > BUFFER_SIZE)
		count = BUFFER_SIZE;
	else
		count = n;
}

void SUMPbackend::setDelay(uint16_t d)
{
    delay = d;
}

void SUMPbackend::setTrigMask(uint32_t m)
{
    triggerMask = m;
}
void SUMPbackend::setTrigValue(uint32_t v)
{
    triggerValue = v & 0xFF;
}

void SUMPbackend::setTrigEnabled(uint8_t state){
    triggerEnabled = state;
}

void SUMPbackend::setFlags(uint32_t f)
{
    flags = f;
}

void SUMPbackend::sendMetadata(){
	//name
	Serial.write(0x01);
	Serial.print("F401CCLA");
	Serial.write((uint8_t) 0x00);
	//mem
	Serial.write(0x21);
	for(uint8_t i=0;i<4;i++)
		Serial.write(i2lebyte(BUFFER_SIZE,i));
	//dynmem
	Serial.write(0x22);
	for(uint8_t i=0;i<4;i++)
		Serial.write((uint8_t) 0);
	//samp rate
	Serial.write(0x23);
	for(uint8_t i=0;i<4;i++)
		Serial.write(i2lebyte(SAMPRATE_MAX,i));
	//no of probes
	Serial.write(0x40);
	Serial.write(0x08);
	//protocol
	Serial.write(0x41);
	Serial.write(0x02);
	//end
	Serial.write((uint8_t) 0x00);
}

void SUMPbackend::printParam() {
	Serial.write("Sample Period:");
    Serial.println(sampleperiod);
    Serial.write("Sample count:");
    Serial.println(count);
    Serial.write("Sample delay:");
    Serial.println(delay);
    Serial.write("Trigger mask:");
    Serial.println(triggerMask);
    Serial.write("triggerValue:");
    Serial.println(triggerValue);
    Serial.write("triggerState:");
    Serial.println(triggerEnabled);
    Serial.write("flags:");
    Serial.println(flags,HEX);

    Serial.write("Timer1 prescaler:");
    Serial.println(Timer1.getPrescaleFactor());
    Serial.write("Timer1 overflow (ARR):");
    Serial.println(Timer1.getOverflow());
}


