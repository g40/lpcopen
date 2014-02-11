/*

	Config file for commonly modified settings

*/

#ifndef CONFIG_H
#define CONFIG_H

// if 1 then we will set to full-speed (12MBS)
//#define FULL_SPEED

// verified working W7x64 WASAPI USB3 hub
// #define CHANNEL_COUNT 12

// verified working W7x64 WASAPI USB3 hub
//#define CHANNEL_COUNT 24

// verified working W7x64 WASAPI USB3 hub
#define CHANNEL_COUNT 36

// valid sizes are 2 and 3 for 16 and 24 bit respectively
// do not change for purposes of resolving channel mask issue
#define BYTES_PER_SAMPLE 2

// sets the isoch endpoint size
#define EP_SIZE_BYTES 1024

// set to 1 to use channel mask, set to 0 to zero it out
#define USE_CHANNEL_MASK 0

// set this to 1 to experiment with device controls
// like volume etc.
// Do not modify. This will break things/
#define USE_FEATURE 0

#endif

