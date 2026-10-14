#pragma once

#if defined(ARDUINO)

// PCA9555 register defines
#include <Arduino.h>
#include "../boards/board_profile.h"
#include "../boards/board_extra_defs.h"

// PCA9555 register addresses.
#define NXP_INPUT_REG  0
#define NXP_OUTPUT_REG 2
#define NXP_INVERT_REG 4
#define NXP_CONFIG_REG 6

#define IOEXP_TYPE_8574 0
#define IOEXP_TYPE_8575 1
#define IOEXP_TYPE_9555 2
#if defined(ESP32)
	#define IOEXP_TYPE_BUILD_IN_GPIO 3
	#define IOEXP_TYPE_SR 4
#endif
#define IOEXP_TYPE_UNKNOWN 254
#define IOEXP_TYPE_NONEXIST 255

class IOEXP {
public:
	IOEXP(uint8_t addr=255) { address = addr; type = IOEXP_TYPE_NONEXIST; }
	virtual ~IOEXP() {}

	virtual void pinMode(uint8_t pin, uint8_t IOMode) { }
	virtual uint16_t i2c_read(uint8_t reg) { return 0xFFFF; }
	virtual void i2c_write(uint8_t reg, uint16_t v) { }
	virtual void shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v) { }

	void digitalWrite(uint16_t v) {
		i2c_write(NXP_OUTPUT_REG, v);
	}

	uint16_t digitalRead() {
		return i2c_read(NXP_INPUT_REG);
	}

	uint8_t digitalRead(uint8_t pin) {
		return (digitalRead() & (1<<pin)) ? HIGH : LOW;
	}

	void digitalWrite(uint8_t pin, uint8_t v) {
		uint16_t values = i2c_read(NXP_OUTPUT_REG);
		if(v > 0) values |= (1<<pin);
		else values &= ~(1 << pin);
		i2c_write(NXP_OUTPUT_REG, values);
	}

	static unsigned char detectType(uint8_t address);
	uint8_t address;
	uint8_t type;
};

class PCA9555 : public IOEXP {
public:
	PCA9555(uint8_t addr) { address = addr; type = IOEXP_TYPE_9555; }
	void pinMode(uint8_t pin, uint8_t IOMode);
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
	void shift_out(uint8_t plat, uint8_t pclk, uint8_t pdat, uint8_t v);
};

class PCF8575 : public IOEXP {
public:
	PCF8575(uint8_t addr) { address = addr; type = IOEXP_TYPE_8575; }
	void pinMode(uint8_t pin, uint8_t IOMode) {
		if(IOMode!=OUTPUT) inputmask |= (1<<pin);
	}
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
private:
	uint16_t inputmask = 0;
};

class PCF8574 : public IOEXP {
public:
	PCF8574(uint8_t addr) { address = addr; type = IOEXP_TYPE_8574; }
	void pinMode(uint8_t pin, uint8_t IOMode) {
		if(IOMode!=OUTPUT) inputmask |= (1<<pin);
	}
	uint16_t i2c_read(uint8_t reg);
	void i2c_write(uint8_t reg, uint16_t v);
private:
	uint8_t inputmask = 0;
};

class BUILD_IN_GPIO : public IOEXP {
public:
  BUILD_IN_GPIO() { 
		type = IOEXP_TYPE_BUILD_IN_GPIO; 
		
		for (uint8_t i = 0; i < 8; ++i) {
			on_board_gpin_list[i] = osboard::active().pins.on_board_gpin_list[i];
		}

		// not needed, this is done before anything else to prevent unstable behavior
		return;
/*
	int i;
	for (i=0; i<8; i++)
		if ( on_board_gpin_list[i] != 255){
			DEBUG_PRINT("Setting PIN ");
			DEBUG_PRINTLN(on_board_gpin_list[i]);
			pinMode( on_board_gpin_list[i], OUTPUT);
		}

		*/
	}
/*  void pinMode(uint8_t pin, uint8_t IOMode) { 
    if(IOMode!=OUTPUT) inputmask |= (1<<pin);
  }
  uint16_t i2c_read(uint8_t reg);*/
  void i2c_write(uint8_t reg, uint16_t v);
private:
  uint8_t inputmask = 0;  // mask bits for input pins
  uint8_t on_board_gpin_list[8] = {255,255,255,255,255,255,255,255}; 

  void digitalWrite(uint8_t pin, uint8_t v) {
	::digitalWrite(pin,v);
  }
};

class IOEXP_SR : public IOEXP {
public:
  IOEXP_SR() { 
		type = IOEXP_TYPE_SR;

		// not needed, this is done before anything else to prevent unstable behavior
		return;

		/*
		
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
		*/
	}
/*  void pinMode(uint8_t pin, uint8_t IOMode) { 
    if(IOMode!=OUTPUT) inputmask |= (1<<pin);
  }
  uint16_t i2c_read(uint8_t reg);*/
  void i2c_write(uint8_t reg, uint16_t v);
  void digitalWrite(uint8_t pin, uint8_t v) {
	::digitalWrite(pin,v);
  }
private:
  uint8_t inputmask = 0;  // mask bits for input pins
};

#endif
