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
 *
 */

/*
 * Author: Andrew Goh
 */
#include <Arduino.h>
#include "PwmOut.h"
#include "Sump.h"

SUMPbackend backend;

uint8_t cmd_buffer[5];
uint8_t cmd_index = 0;
uint32_t last_millis;

#define LED_ON 0
#define LED_OFF 1

// the setup() method runs once when the sketch starts
void setup() {

	//disableDebugPorts();
	SerialUSB.begin();

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LED_ON);
	//Flush it
	while(!Serial.available());
	if(Serial.available())
		Serial.read();

	blinks(50, 100, 5);

	pwmout.init();
	backend.init();
	last_millis = 0;
}

uint8_t read4() {
	uint32_t start = millis();
	while(cmd_index < 5) {
		if(Serial.available()) {
			cmd_buffer[cmd_index++] = Serial.read();
		} else {
			if(millis() - start > 100) break; //timeout
			asm("wfi");
		}
	}
	return cmd_index;
}


//the loop() method runs over and over again,
//as long as maple has power
void loop() {

	if (Serial.available()) {
		digitalWrite(LED_BUILTIN, LED_ON); //
		memset(cmd_buffer, 0, sizeof(cmd_buffer));
		cmd_index = 0;
		cmd_buffer[cmd_index++] = Serial.read();
		switch (cmd_buffer[0]) {
		case SUMP_RESET:
			//ignore repeat resets received in last 10 ms
			if((millis() - last_millis) < 10) break;
			backend.reset();
			backend.setDelay(11);
			last_millis = millis();
			break;

		case SUMP_QUERY:
			Serial.print("1ALS");
			break;

		case SUMP_ARM:
			//set test flag
			//this generates sample data
			//backend.setFlags(FLAGS_TEST);
			backend.arm();
			break;

		case SUMP_XON:
			//set test flag
			backend.start();
			break;

		case SUMP_XOFF:
			backend.stop();
			break;

		case SUMP_SELFTEST:
			backend.setFlags(FLAGS_TEST);
			backend.arm();
			break;

		case SUMP_GET_METADATA:
			backend.sendMetadata();
			break;

		case SUMP_SET_DIVIDER: {
			if(read4() < 5) break;
			// in the specs, divider is 24 bits, but here it reads the full 32 bits
			uint32_t divider = lea2uint(cmd_buffer, 1);
			backend.setDivider(divider);
			break;
		}
		case SUMP_SET_READ_DELAY_COUNT: {
			if(read4() < 5) break;
			//strangely sump gui sends the readcount less 1, hence adding it back here
			uint16_t readCount = 4 * (lea2short(cmd_buffer, 1) + 1);
			uint16_t delayCount = 4 * (lea2short(cmd_buffer, 3) + 1);
			backend.setCount(readCount);
			//note delay count is not implemented
			backend.setDelay(delayCount);
			break;
		}
		case SUMP_SET_FLAGS: {
			if(read4()<5) break;
			uint32_t flags = lea2uint(cmd_buffer, 1);
			backend.setFlags(flags);
			break;
		}
		case SUMP_SET_TRIGGER_MASK: {
			if(read4() < 5) break;
			uint32_t mask = lea2uint(cmd_buffer, 1);
			backend.setTrigMask(mask);
			break;
		}
		case SUMP_SET_TRIGGER_VALUES: {
			if(read4() < 5) break;
			uint32_t value = lea2uint(cmd_buffer, 1);
			backend.setTrigValue(value);
			break;
		}
		case SUMP_SET_TRIGGER_CONFIG: {
			if(read4() < 5) break;
			//strangely trigger is always sent as enabled even if it is flagged
			//disabled in sump gui
			uint8_t serial = getbit(cmd_buffer[4], 2); //3rd bit
			uint8_t enabled = getbit(cmd_buffer[4], 3); //4th bit
			uint16_t channel = (cmd_buffer[3] & 0xf0U) >> 4;
			channel |= (cmd_buffer[4] & 1U) << 4;

			//note serial triggering is not supported
			if(!serial) backend.setTrigEnabled(enabled);
			break;
		}
		/* these are other set trigger mask commands
		 * they are long (5 byte) commands
		 * this is not supported but the extra bytes need to be read
		 * so that it would not accidentally trigger resets i.e. 0x00
		 */
		case 0xC4:
		case 0xC8:
		case 0xCC:
			read4();
			break;
		// other trigger values commands, same as above
		case 0xC5:
		case 0xC9:
		case 0xCD:
			read4();
			break;
			// other trigger config commands, same as above
		case 0xC6:
		case 0xCA:
		case 0xCE:
			read4();
			break;
		case 'b':
			backend.printParam();
			blinks(100,100,5);
			break;
		default:
			break;
		}
		cmd_index = 0;
		digitalWrite(LED_BUILTIN, LED_OFF);
	}
	asm("wfi"); //wait for systick
}


/*
 * @param on - on time in msecs
 * @param off - off time in msecs
 * @param num - number of blinks
 */
void blinks(unsigned int on, unsigned int off, unsigned int num) {
	for (unsigned int i = 0; i < num; i++) {
		digitalWrite(LED_BUILTIN, LED_ON);
		delay(on);
		digitalWrite(LED_BUILTIN, LED_OFF);
		delay(off);
	}
}

/* return byte of int32 in little endian order given position
 *
 * @param value the int32
 * @param pos byte position to return
 */
uint8_t i2lebyte(uint32_t value, uint8_t pos) {
	return (uint8_t) (value >> (8 * pos)) & 0xffU;
}

/* return uint16_t given buf & pos
 *
 * @param buf pointer to cmd buffer
 * @param pos byte position
 */
uint16_t lea2short(uint8_t *buf, uint8_t pos) {
	uint16_t v = *(buf + pos);
	v |= *(buf + pos +1) << 8;
	return v;
}

/* return uint32_t given buf & pos
 *
 * @param buf pointer to cmd buffer
 * @param pos byte position
 */

uint32_t lea2uint(uint8_t *buf, uint8_t pos) {
	uint32_t v = *(buf + pos);
	v |= *(buf + pos + 1) << 8;
	v |= *(buf + pos + 2) << 16;
	v |= *(buf + pos + 3) << 24;
	return v;
}

/* return a bit from a given position
 *
 * @param v value
 * @param pos bit position
 */
uint8_t getbit(uint32_t v, uint8_t pos) {
	return (uint8_t) ((v >> pos ) & 1U);
}


