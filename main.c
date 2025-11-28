/* Author: kvl@eti.uni-siegen.de
 * Created on July 25, 2025, 11:18 AM
 * Modified for 5-button authentication display
 */

#include "PIC24FStarter.h"

void delay(unsigned int milliseconds) {
    T1CONbits.TCKPS = 0b11; // Prescale 1:256
    PR1 = 47; TMR1 = 0; // Reset timer counter
    T1CONbits.TON = 1; // Turn on Timer1
    unsigned int count = 0;
    while (count < milliseconds) {
        while (!IFS0bits.T1IF); // Wait for Timer1 interrupt flag
        IFS0bits.T1IF = 0; // Clear Timer1 interrupt flag
        count++;
    }
    T1CONbits.TON = 0; // Turn off Timer1
}

// Display text centered on screen
void DisplayCentered(const char* text) {
    uint8_t textWidth = GetStringWidth(text);
    int16_t xPos = (DISP_HOR_RESOLUTION - textWidth) / 2;
    int16_t yPos = (DISP_VER_RESOLUTION - 7) / 2; // 7 is font height
    
    // Clear display and draw text
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    DrawString(xPos, yPos, text);
}

int main(void) {
    INIT_CLOCK(); 
    CTMUInit(); 
    RGBMapColorPins();
    
    uint16_t timeout = 10;  // milliseconds delay for button polling
    int16_t aggr[5]; 
    aggr[0] = aggr[1] = aggr[2] = aggr[3] = aggr[4] = 0; // Aggregate values
    uint8_t lastDetected = 0xFF; // Track last detected button to avoid flicker
    const int16_t THRESHOLD = 6; // Minimum aggregate value to register
    
    RGBTurnOnLED();
    ResetDevice();
    
    // Initial display
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    DrawString(20, 28, "Press a button");
    
    while(1) { 
        ReadCTMU();  // Read touch buttons and potentiometer
        
        // Update aggregate values (same logic as original code)
        for (uint8_t i = 0; i < 5; i++) {
            if (buttons[i]) aggr[i]++; 
            else aggr[i]--;
            if (aggr[i] < 0) aggr[i] = 0; 
            if (aggr[i] > 30) aggr[i] = 30; // Cap at 30
        }
        
        // Find which button has highest aggregate value
        int16_t maxVal = THRESHOLD;
        uint8_t maxButton = 0xFF;
        for (uint8_t i = 0; i < 5; i++) {
            if (aggr[i] > maxVal) {
                maxVal = aggr[i];
                maxButton = i;
            }
        }
        
        // Display number only if button changed and exceeds threshold
        if (maxButton != 0xFF && maxButton != lastDetected) {
            switch(maxButton) {
                case 0:
                    DisplayCentered("1");  // UP = 1
                    break;
                case 1:
                    DisplayCentered("2");  // RIGHT = 2
                    break;
                case 2:
                    DisplayCentered("3");  // DOWN = 3
                    break;
                case 3:
                    DisplayCentered("4");  // LEFT = 4
                    break;
                case 4:
                    DisplayCentered("5");  // CENTER = 5
                    break;
            }
            lastDetected = maxButton;
        } else if (maxButton == 0xFF) {
            lastDetected = 0xFF; // Reset when no button pressed
        }
        
        delay(timeout);
    }
    
    RGBTurnOffLED();
}
