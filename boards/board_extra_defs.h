/*
*
* Ugly, old style header file for extra board definitions.
*
*/


#pragma once

#ifndef BOARD_EXTRA_DEFS_H
#define BOARD_EXTRA_DEFS_H

#define MDNS_NAME "opensprinkler" // mDNS name for OS controler

// Rotary Encoder instead of buttons - not used for now, testing/development
//#define USE_ROTARY_ENCODER
#define ROTARY_ENCODER_A_PIN 35 // must be interrupt capable PIN!
#define ROTARY_ENCODER_B_PIN 34
#define ROTARY_ENCODER_BUTTON_PIN 5 // this should be same, BUTTON_2, default 33

//#define BOOT_MENU_V2

#define ESP32_FS_BLOCK_SIZE	4096
// for testing only - factory reset should do it anyway
#define ESP32_FORMAT_FS_IF_FAILED true
#define ESP32_FS_PARTITION_LABEL "spiffs"

#endif
