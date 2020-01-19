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
#include "PwmOut.h"
#include <Arduino.h>
#include <HardwareTimer.h>


PwmOut pwmout;

uint32_t count;

void timer_handle() {
	count++;
	GPIOB_BASE->ODR = (~ count) & 0xFFU;
}

PwmOut::PwmOut() {
}

void PwmOut::init() {
	Timer2.init();
	Timer2.pause();
	// 1us
	Timer2.setPeriod(1);
	Timer2.attachInterrupt(0, &timer_handle);
	GPIOB_BASE->MODER = 0x5555UL;           // Output
	GPIOB_BASE->ODR = 0;
	count = 0;
}

void PwmOut::start() {
	GPIOB_BASE->ODR = 0;
	Timer2.refresh();
	Timer2.resume();
}

void PwmOut::stop() {
	Timer2.pause();
}

PwmOut::~PwmOut() {

}

