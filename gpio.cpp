/* OpenSprinkler Unified Firmware
 * Copyright (C) 2014 by Ray Wang (ray@opensprinkler.com)
 *
 * GPIO functions
 * Feb 2015 @ OpenSprinkler.com
 *
 * This file is part of the OpenSprinkler library
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 */

#include "gpio.h"

#if defined(ARDUINO)

#if defined(ESP8266) || defined(ESP32)

#include <Wire.h>
#include "defines.h"

unsigned char IOEXP::detectType(uint8_t address) {
	Wire.beginTransmission(address);
	if(Wire.endTransmission()!=0) return IOEXP_TYPE_NONEXIST; // this I2C address does not exist

	Wire.beginTransmission(address);
	Wire.write(NXP_INVERT_REG); // ask for polarity register
	Wire.endTransmission();

	if(Wire.requestFrom(address, (uint8_t)2) != 2) return IOEXP_TYPE_UNKNOWN;
	uint8_t low = Wire.read();
	uint8_t high = Wire.read();
	if(low==0x00 && high==0x00) {
		return IOEXP_TYPE_9555; // PCA9555 has polarity register which inits to 0
	}
	return IOEXP_TYPE_8575;
}

void PCA9555::pinMode(uint8_t pin, uint8_t IOMode) {
	uint16_t config = i2c_read(NXP_CONFIG_REG);
	if(IOMode == OUTPUT) {
		config &= ~(1 << pin); // config bit set to 0 for output pin
	} else {
		config |= (1 << pin);  // config bit set to 1 for input pin
	}
	i2c_write(NXP_CONFIG_REG, config);
}

uint16_t PCA9555::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.endTransmission();
	if(Wire.requestFrom(address, (uint8_t)2) != 2) {return 0xFFFF; DEBUG_PRINTLN("GPIO error");}
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	return data0+(data1<<8);
}

void PCA9555::i2c_write(uint8_t reg, uint16_t v){
	if(address==255)	return;
	Wire.beginTransmission(address);
	Wire.write(reg);
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

void PCA9555::shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v) {
	if(plat<IOEXP_PIN || pclk<IOEXP_PIN || pdat<IOEXP_PIN)
		return; // the definition of each pin must be offset by IOEXP_PIN to begin with

	plat-=IOEXP_PIN;
	pclk-=IOEXP_PIN;
	pdat-=IOEXP_PIN;

	uint16_t output = i2c_read(NXP_OUTPUT_REG); // keep a copy of the current output registers

	output &= ~(1<<plat); i2c_write(NXP_OUTPUT_REG, output); // set latch low

	for(uint8_t s=0;s<8;s++) {
		output &= ~(1<<pclk); i2c_write(NXP_OUTPUT_REG, output); // set clock low

		if(v & ((unsigned char)1<<(7-s))) {
			output |= (1<<pdat);
		} else {
			output &= ~(1<<pdat);
		}
		i2c_write(NXP_OUTPUT_REG, output); // set data pin according to bits in v

		output |= (1<<pclk); i2c_write(NXP_OUTPUT_REG, output); // set clock high
	}

	output |= (1<<plat); i2c_write(NXP_OUTPUT_REG, output); // set latch high
}

uint16_t PCF8575::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)2) != 2) return 0xFFFF;
	uint16_t data0 = Wire.read();
	uint16_t data1 = Wire.read();
	Wire.endTransmission();
	return data0+(data1<<8);
}

void PCF8575::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255)	return;
	Wire.beginTransmission(address);
	// todo: handle inputmask (not necessary unless if using any pin as input)
	Wire.write(v&0xff);
	Wire.write(v>>8);
	Wire.endTransmission();
}

uint16_t PCF8574::i2c_read(uint8_t reg) {
	if(address==255)	return 0xFFFF;
	Wire.beginTransmission(address);
	if(Wire.requestFrom(address, (uint8_t)1) != 1) return 0xFFFF;
	uint16_t data = Wire.read();
	Wire.endTransmission();
	return data;
}

void PCF8574::i2c_write(uint8_t reg, uint16_t v) {
	if(address==255)	return;
	Wire.beginTransmission(address);
	Wire.write((uint8_t)(v&0xFF) | inputmask);
	Wire.endTransmission();
}

#if defined(ESP32)
void BUILD_IN_GPIO::set_pins_output_mode() {
  int i;
  for (i=0; i<8; i++)
    if ( on_board_gpin_list[i] != 255){
		DEBUG_PRINT("Setting PIN ");
		DEBUG_PRINTLN(on_board_gpin_list[i]);
      pinModeExt( on_board_gpin_list[i], OUTPUT);
    }
}

void BUILD_IN_GPIO::i2c_write(uint8_t reg, uint16_t v) {
  v = (uint8_t)(v&0xFF) | inputmask;
  int i;
  for (i=0; i<8; i++)
    if ( on_board_gpin_list[i] != 255){
      digitalWriteExt( on_board_gpin_list[i], ((v)>>(i)) & 1);
    }
}

void IOEXP_SR::set_pins_output_mode(){
	
	// not needed, this is done before anything else to prevent unstable behavior
	DEBUG_PRINTLN("SR PINs already inited before anything else");
	return;

	DEBUG_PRINTLN("Setting up SR pins");
	DEBUG_PRINTLN(IOEXP_SR_LATCH_PIN);
	DEBUG_PRINTLN(IOEXP_SR_CLK_PIN);
	// shift register setup
	// pinMode(PIN_SR_OE, OUTPUT);
	// pull shift register OE high to disable output
	// digitalWrite(PIN_SR_OE, HIGH);
	pinMode(IOEXP_SR_DATA_PIN, OUTPUT);
	digitalWrite(IOEXP_SR_DATA_PIN, LOW);
	DEBUG_PRINTLN("DATA OK");
	
	pinMode(IOEXP_SR_CLK_PIN, OUTPUT);
	digitalWrite(IOEXP_SR_CLK_PIN, HIGH);
	DEBUG_PRINTLN("CLK OK");
	
	pinMode(IOEXP_SR_LATCH_PIN, OUTPUT);
	DEBUG_PRINTLN("Latch 1");
	digitalWrite(IOEXP_SR_LATCH_PIN, HIGH);
	DEBUG_PRINTLN("Latch OK");

	#if defined(SEPARATE_MASTER_VALVE)
	DEBUG_PRINTLN("Enabling separate master valve");
	pinMode(SEPARATE_MASTER_VALVE,OUTPUT);
	DEBUG_PRINT("Setting station logic to ");
	DEBUG_PRINTLN(STATION_LOGIC);
	if ( STATION_LOGIC ) {
		digitalWrite(SEPARATE_MASTER_VALVE,LOW);
	} else {
		digitalWrite(SEPARATE_MASTER_VALVE,HIGH);
	}
	#endif
}


void IOEXP_SR::i2c_write(uint8_t reg, uint16_t v){
	v = (uint8_t)(v&0xFF) | inputmask;
  	digitalWrite(IOEXP_SR_LATCH_PIN, LOW);
	//byte s, sbits;
	/*
	for(s=0;s<8;s++) {
		digitalWrite(IOEXP_SR_CLK_PIN, LOW);
		digitalWrite(IOEXP_SR_DATA_PIN, (v & ((byte)1<<(7-s))) ? HIGH : LOW );
	}*/
	
	shiftOut(IOEXP_SR_DATA_PIN, IOEXP_SR_CLK_PIN, MSBFIRST, v);
	digitalWrite(IOEXP_SR_LATCH_PIN, HIGH);
}
#endif

#include "OpenSprinkler.h"

extern OpenSprinkler os;

void pinModeExt(unsigned char pin, unsigned char mode) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {
		os.mainio->pinMode(pin-IOEXP_PIN, mode);
	} else {
		pinMode(pin, mode);
	}
}

void digitalWriteExt(unsigned char pin, unsigned char value) {
	if(pin==255) return;
	if(pin>=IOEXP_PIN) {

		os.mainio->digitalWrite(pin-IOEXP_PIN, value);
	} else {
		digitalWrite(pin, value);
	}
}

unsigned char digitalReadExt(unsigned char pin) {
	if(pin==255) return HIGH;
	if(pin>=IOEXP_PIN) {
		return os.mainio->digitalRead(pin-IOEXP_PIN);
		// a pin on IO expander
		//return pcf_read(MAIN_I2CADDR)&(1<<(pin-IOEXP_PIN));
	} else {
		return digitalRead(pin);
	}
}
#endif

#elif defined(OSPI)

/**
 * GPIO Implementation for Raspberry Pi using lgpio library.
 * This replaces the previous libgpiod implementation for simplicity and robustness.
 */
#include <stdio.h>
#include <lgpio.h>

#include "utils.h"

// GPIO handle for lgpio library
int lgpio_handle = -1;

// Initialize lgpio connection
int init_lgpio() {
	if (lgpio_handle >= 0) {
		return 0; // Already initialized
	}

	int chip_id = 0; // Default to gpiochip0 for older Raspberry Pi models

	// On Raspberry Pi 5 (bcm2712), the primary GPIOs are on gpiochip4
	if (get_board_type() == BoardType::RaspberryPi_bcm2712) {
		chip_id = 4;
	}

	lgpio_handle = lgGpiochipOpen(chip_id);
	if (lgpio_handle < 0) {
		// If we tried the non-default chip and failed, try the default one as a fallback.
		if (chip_id != 0) {
			DEBUG_PRINTLN("Could not open gpiochip4, falling back to gpiochip0...");
			chip_id = 0;
			lgpio_handle = lgGpiochipOpen(chip_id);
		}
	}

	// If handle is still invalid after potential fallback, report error and exit.
	if (lgpio_handle < 0) {
		DEBUG_PRINTLN("Could not open a valid GPIO chip.");
		return -1;
	}

	DEBUG_PRINT("Successfully opened gpiochip");
	DEBUG_PRINTLN(chip_id);
	return 0;
}

/** Set pin mode, in or out */
void pinMode(int pin, unsigned char mode) {
	if (init_lgpio() != 0) return;

	switch(mode) {
		case INPUT:
			// int lgGpioClaimInput(int h, unsigned lFlags, unsigned gpio);
			lgGpioClaimInput(lgpio_handle, 0, pin);
			break;
		case INPUT_PULLUP:
			// int lgGpioClaimInput(int h, unsigned lFlags, unsigned gpio);
			lgGpioClaimInput(lgpio_handle, LG_SET_PULL_UP, pin);
			break;
		case OUTPUT:
			// int lgGpioClaimOutput(int h, unsigned lFlags, unsigned gpio, unsigned level);
			// Claim pin as output, with a default state of LOW
			lgGpioClaimOutput(lgpio_handle, 0, pin, LOW);
			break;
		default:
			DEBUG_PRINTLN("invalid pin direction");
			break;
	}
	return;
}

/** Read digital value */
unsigned char digitalRead(int pin) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to read from uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return 0;
	}
	int val = lgGpioRead(lgpio_handle, pin);
	if (val < 0) {
		DEBUG_PRINT("failed to read value on pin ");
		DEBUG_PRINTLN(pin);
		return 0; // Return LOW on error
	}
	return (unsigned char)val;
}

/** Write digital value */
void digitalWrite(int pin, unsigned char value) {
	if (lgpio_handle < 0) {
		DEBUG_PRINT("tried to write to uninitialized lgpio handle for pin ");
		DEBUG_PRINTLN(pin);
		return;
	}

	int res = lgGpioWrite(lgpio_handle, pin, value);
	if (res < 0) {
		DEBUG_PRINT("failed to write value on pin ");
		DEBUG_PRINTLN(pin);
	}
}

#else

void pinMode(int pin, unsigned char mode) {}
void digitalWrite(int pin, unsigned char value) {}
unsigned char digitalRead(int pin) {return 0;}

#endif

