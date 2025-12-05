/*
 * Capacitive Touch Sensor Driver - Header
 * 
 * Original driver by: kvl@eti.uni-siegen.de
 */
#ifndef TOUCHSENSE__H
#define	TOUCHSENSE__H

#include <xc.h>

#define TRIP_VALUE          0x500  // go to 1000 for more sensitive behaviors
#define HYSTERESIS_VALUE    0x65

#define NUM_TOUCHPADS 5
#define STARTING_ADC_CHANNEL 8

extern uint8_t buttons[NUM_TOUCHPADS];  // up, right, down, left, center
extern uint16_t _potADC;

void ReadPotentiometer();
void CTMUInit();
void ReadCTMU();

#endif	/* TOUCHSENSE__H */
