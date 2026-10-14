/*
*
* Ugly, old style header file for extra board definitions.
*
*/


#pragma once

#ifndef BOARD_EXTRA_DEFS_H
#define BOARD_EXTRA_DEFS_H

// ON_BOARD_GPIN_LIST - added to board_profile.h as BoardPins.on_board_gpin_list[8] to allow for different boards to have different on-board gpins.  This is used by BUILD_IN_GPIO and IOEXP_SR drivers.
// SYSTEM_LED_PIN - added to board_profile.h as BoardPins.system_led

#define MDNS_NAME "opensprinkler" // mDNS name for OS controler

#define USE_IOEXP_SR 0 // '1' uses Shift-register for stations. To use built-in GPIO pins instead, change to  '0' - v1pr's board: 1

// v1pr's board, these are the GPIO pins user for stations - IOEXP PCF/PCA not (yet) supported
#define ON_BOARD_GPIN_LIST     {2,4,255,255,255,255,255,255} // was 2,4

// deprecated, should be followed in the codebase #define STATION_LOGIC 1 // Zone output logic for relays - 1 => HIGH in ON, 0 => LOW is ON - v1pr board: 1

// Rotary Encoder instead of buttons - not used for now, testing/development
//#define USE_ROTARY_ENCODER
#define ROTARY_ENCODER_A_PIN 35 // must be interrupt capable PIN!
#define ROTARY_ENCODER_B_PIN 34
#define ROTARY_ENCODER_BUTTON_PIN 5 // this should be same, BUTTON_2, default 33

//#define BOOT_MENU_V2

#define SEPARATE_MASTER_VALVE 19

// 74HC595 shift reg
// #define IOEXP_SR_OE_PIN // output enable pin, not used now
#define IOEXP_SR_DATA_PIN 25 // DS default 18
#define IOEXP_SR_CLK_PIN 27 // SH_CP
#define IOEXP_SR_LATCH_PIN 32 // ST_CP

#define ESP32_FS_BLOCK_SIZE	4096
// for testing only - factory reset should do it anyway
#define ESP32_FORMAT_FS_IF_FAILED true
#define ESP32_FS_PARTITION_LABEL "spiffs"

#endif
