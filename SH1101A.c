/*
 * SH1101A OLED Display Driver
 * 
 * Driver for SH1101A 128x64 OLED display connected via PMP interface.
 * Includes text rendering with 5x7 font and graphics primitives.
 * 
 * Original driver by: kvl@eti.uni-siegen.de
 * Extended with text/graphics by: AdityaDk10
 */
#include "SH1101A.h"

uint8_t _color;

// sets page + lower and higher address pointer of display buffer
#define SetAddress(page, lowerAddr, higherAddr) \
	DisplaySetCommand(); DeviceWrite(page); DeviceWrite(lowerAddr); \
    DeviceWrite(higherAddr); DisplaySetData();

#define AssignPageAddress(y) \
    if (y < 8) page = 0xB0; else if (y < 16) page = 0xB1; \
    else if (y < 24) page = 0xB2; else if (y < 32) page = 0xB3; \
    else if (y < 40) page = 0xB4; else if (y < 48) page = 0xB5; \
    else if (y < 56) page = 0xB6; else page = 0xB7

#define PMPWaitBusy()   while(PMMODEbits.BUSY)  // wait for PMP cycle end

// a software delay in intervals of 10 microseconds.
void Delay10us( uint32_t tenMicroSecondCounter ) {
    volatile int32_t cyclesRequiredForDelay;  //7 cycles burned to this point 
    cyclesRequiredForDelay = (int32_t)(CLOCK_FREQ/100000)*tenMicroSecondCounter;
    // subtract all  cycles used up til while loop below, each loop cycle count
    // is subtracted (subtract the 5 cycle function return)
    cyclesRequiredForDelay -= 44; //(29 + 5) + 10 cycles padding
    if(cyclesRequiredForDelay <= 0) {
        // cycle count exceeded, exit function
    } else {   
        while(cyclesRequiredForDelay>0) { //19 cycles used to this point.
            cyclesRequiredForDelay -= 11; 
            // subtract cycles in each delay stage, 12 + 1 as padding
        }
    }
}

// performs a software delay in intervals of 1 millisecond
void DelayMs( uint16_t ms ) {
    volatile uint8_t i;        
    while (ms--) {
        i = 4;
        while (i--) {
            Delay10us( 25 );
        }
    }
}

// write data into controller's RAM, chip select should be enabled
extern inline void __attribute__ ((always_inline)) DeviceWrite(uint8_t data) {
	PMDIN1 = data;
	PMPWaitBusy();
}

// read data from controller's RAM. chip select should be enabled
extern inline uint8_t __attribute__ ((always_inline)) DeviceRead() {
    uint8_t value;
	value = PMDIN1;
	PMPWaitBusy();
	PMCONbits.PMPEN = 0; // disable PMP
	value = PMDIN1;
	PMCONbits.PMPEN = 1; // enable  PMP
	return value;
}

// single read is performed; Useful in issuing one read access only.
extern inline uint8_t __attribute__ ((always_inline)) SingleDeviceRead() {
    uint8_t value;
	value = PMDIN1;
	PMPWaitBusy();
	return value;
}

// Reads a word from the device
extern inline uint16_t __attribute__ ((always_inline)) DeviceReadWord() {
    uint16_t value; uint8_t temp;
    value = PMDIN1;
    value = value << 8;
    PMPWaitBusy();
    temp = PMDIN1;
    value = value & temp;
    PMPWaitBusy();
    return value;
}

// initializes the OLED device
extern inline void __attribute__ ((always_inline)) DriverInterfaceInit(void) { 
    // variable for PMP timing calculation
	// CLOCK_FREQ in MHz => pClockPeriod in nanoseconds
    uint32_t pClockPeriod = (1000000000ul) / CLOCK_FREQ;
	DisplayResetEnable();               // hold in reset by default
    DisplayResetConfig();               // enable RESET line
    DisplayCmdDataConfig();             // enable RS line
    DisplayDisable();                   // not selected by default
    DisplayConfig();                    // enable chip select line
    // PMP setup
    PMMODE = 0; PMAEN = 0; PMCON = 0;
    PMMODEbits.MODE = 2;                // Intel 80 master interface
    PMMODEbits.WAITB = 0;
    #if (PMP_DATA_WAIT_TIME == 0)
        PMMODEbits.WAITM = 0;
    #else    
        if (PMP_DATA_WAIT_TIME <= pClockPeriod)
            PMMODEbits.WAITM = 1;
        else if (PMP_DATA_WAIT_TIME > pClockPeriod)
            PMMODEbits.WAITM = (PMP_DATA_WAIT_TIME / pClockPeriod) + 1;
    #endif
    #if (PMP_DATA_HOLD_TIME == 0)
        PMMODEbits.WAITE = 0;
    #else
        if (PMP_DATA_HOLD_TIME <= pClockPeriod)
            PMMODEbits.WAITE = 0;
        else if (PMP_DATA_HOLD_TIME > pClockPeriod)
            PMMODEbits.WAITE = (PMP_DATA_HOLD_TIME / pClockPeriod) + 1;
    #endif
    PMMODEbits.MODE16 = 0;              // 8 bit mode
    PMCONbits.PTRDEN =  PMCONbits.PTWREN = 1;  // enable WR & RD line
    PMCONbits.PMPEN = 1;                // enable PMP
    DisplayResetDisable();              // release from reset
    Delay10us(20);  // hard delay for devices that need it after reset
}

void ResetDevice(void) {
	DriverInterfaceInit();  // Initialize the device
    DisplayEnable();
	DisplaySetCommand();
    DeviceWrite(0xAE);             // turn off the display (AF=ON, AE=OFF)
    DeviceWrite(0xDB); DeviceWrite(0x23); // set  VCOMH
    DeviceWrite(0xD9); DeviceWrite(0x22); // set  VP    
    DeviceWrite(0xA1);             // [A0]:column address 0 is map to SEG0
                                   // [A1]:column address 131 is map to SEG0
    DeviceWrite(0xC8);             // C0 is COM0 to COMn, C8 is COMn to COM0
    DeviceWrite(0xDA);             // set COM pins hardware configuration
    DeviceWrite(0x12);
    DeviceWrite(0xA8);             // set multiplex ratio
    DeviceWrite(0x3F);             // set to 64 mux
    DeviceWrite(0xD5);             // set display clock divide
    DeviceWrite(0xA0);             // set to 100Hz
    DeviceWrite(0x81);             // Set contrast control
    DeviceWrite(0x60);             // display 0 ~ 127; 2C
    DeviceWrite(0xD3);             // Display Offset: set display offset
    DeviceWrite(0x00);             // no offset
    DeviceWrite(0xA6);             //Normal or Inverse Display: Normal display
    DeviceWrite(0xAD);             // Set DC-DC
    DeviceWrite(0x8B);             // 8B=ON, 8A=OFF
    DeviceWrite(0xAF);             // Display ON/OFF: AF=ON, AE=OFF
    DelayMs(150);
    DeviceWrite(0xA4);             // Entire Display ON/OFF: A4=ON
    DeviceWrite(0x40);             // Set display start line
    DeviceWrite(0x00 + OFFSET);    // Set lower column address
    DeviceWrite(0x10);             // Set higher column address
    DelayMs(1);
    DisplayDisable(); DisplaySetData();
}

// puts pixel
void PutPixel(int16_t x, int16_t y) {
    uint8_t page, add, lAddr, hAddr;
    uint8_t mask, display;
    // Assign a page address
    AssignPageAddress(y);
    add = x + OFFSET;
    lAddr = 0x0F & add; hAddr = 0x10 | (add >> 4);  // low + high address
    // Calculate mask from rows basically do a y%8 and remainder is bit position
    add = y >> 3;                   // Divide by 8
    add <<= 3;                      // Multiply by 8
    add = y - add;                  // Calculate bit position
    mask = 1 << add;                // Left shift 1 by bit position
    DisplayEnable();
    SetAddress(page, lAddr, hAddr); // Set the address (sets the page,
    // lower and higher column address pointers)
    display = SingleDeviceRead();	// initiate Read transaction on PMP  								
    display = SingleDeviceRead();	// for synchronization in the controller
    display = SingleDeviceRead();	// read actual data from display buffer
    if (_color > 0)  display |= mask;   // pixel on -> or in mask
    else display &= ~mask;           //    pixel off -> and with inverted mask
    SetAddress(page, lAddr, hAddr); // Set the address (sets the page,
    // lower and higher column address pointers)
    DeviceWrite(display);             // restore the byte with manipulated bit
    DisplayDisable();
}

// return pixel color at x,y position
uint8_t GetPixel(int16_t x, int16_t y) {
    uint8_t page, add, lAddr, hAddr, mask, temp, display;
    AssignPageAddress(y);
    add = x + OFFSET;
    lAddr = 0x0F & add; hAddr = 0x10 | (add >> 4); // low + high address
    // Calculate mask from rows basically do a y%8 and remainder is bit position
    temp = y >> 3;                  // Divide by 8
    temp <<= 3;                     // Multiply by 8
    temp = y - temp;                // Calculate bit position
    mask = 1 << temp;               // Left shift 1 by bit position
    DisplayEnable();
    SetAddress(page, lAddr, hAddr); // set page, lower, higher column address
    display = SingleDeviceRead();	// Read to initiate Read transaction on PMP
    display = DeviceRead();         // Read data from display buffer
    DisplayDisable();
    return (display & mask);        // mask all other bits and return the result
}

// clears screen with _color
void ClearDevice(void) {
    DisplayEnable();
    for(uint8_t i = 0xB0; i < 0xB8; i++) {  // go through all 8 pages
        SetAddress(i, 0x00, 0x10);
        for(uint8_t j = 0; j < 132; j++) // write to all 132 bytes
            DeviceWrite(_color);
    }
    DisplayDisable();
}

// Simple 5x7 font for ASCII characters 32-126
const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 (space)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 )
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ;
    {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 =
    {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 Z
    {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 backslash
    {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 _
};

// Draw a single character at position (x, y) - pixel by pixel
void DrawChar(int16_t x, int16_t y, char c) {
    if (c < 32 || c > 95) c = 32; // Limit to printable ASCII
    const uint8_t* glyph = font5x7[c - 32];
    
    // Draw each column of the character
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t columnData = glyph[col];
        for (uint8_t row = 0; row < 8; row++) {
            if (columnData & (1 << row)) {
                PutPixel(x + col, y + row);
            }
        }
    }
}

// Draw a string at position (x, y)
void DrawString(int16_t x, int16_t y, const char* str) {
    int16_t cursorX = x;
    while (*str) {
        DrawChar(cursorX, y, *str);
        cursorX += 6; // 5 pixels wide + 1 pixel spacing
        str++;
    }
}

// Get the width of a string in pixels
uint8_t GetStringWidth(const char* str) {
    uint8_t len = 0;
    while (*str++) len++;
    return (len > 0) ? (len * 6 - 1) : 0; // 6 pixels per char, minus last spacing
}

// Draw a line using Bresenham's algorithm
void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t sx = (dx > 0) ? 1 : -1;
    int16_t sy = (dy > 0) ? 1 : -1;
    
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    
    int16_t err = dx - dy;
    int16_t e2;
    
    while (1) {
        PutPixel(x0, y0);
        
        if (x0 == x1 && y0 == y1) break;
        
        e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Draw a filled circle
void DrawFilledCircle(int16_t cx, int16_t cy, int16_t r) {
    for (int16_t y = -r; y <= r; y++) {
        for (int16_t x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                PutPixel(cx + x, cy + y);
            }
        }
    }
}
