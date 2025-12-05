/*
 * RGB LED Driver - Header
 * 
 * Functions for the RGB LEDs using PWM.
 * Pins: RG6/RG7, RG8/RG9, RF4/RF5
 * To control color, write inverse of saturation value to PWM.
 * 
 * Original driver by: kvl@eti.uni-siegen.de
 */
#ifndef RGBLEDS__H
#define	RGBLEDS__H

#include <xc.h>

#define CONVERT_TO_COLOR(x)         (~x & 0xFF)
#define PWM_CONFIGURATION_1         0x0007
#define PWM_CONFIGURATION_2         0x000C
#define PWM_OFF                     0x0000

// set new PWM output
void SetRGBs( uint8_t satR, uint8_t satG, uint8_t satB );

void RGBMapColorPins();

// turns off the LED by turning off the timers, PWMs, and setting pins to inputs
void RGBTurnOffLED();

// turns on the LEDs by turning on timers, PWMs, and setting pins to outputs
void RGBTurnOnLED();

#endif	/* RGBLEDS__H */
