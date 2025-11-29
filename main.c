/* Author: kvl@eti.uni-siegen.de
 * Created on July 25, 2025, 11:18 AM
 * Modified for 5-button authentication system
 */

#include "PIC24FStarter.h"
#include <stdio.h>

// ==================== TIMER DELAY ====================
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

// ==================== UI HELPER FUNCTIONS ====================

// Display text centered horizontally on screen
void DisplayCentered(const char* text) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    uint8_t textWidth = GetStringWidth(text);
    int16_t xPos = (DISP_HOR_RESOLUTION - textWidth) / 2;
    int16_t yPos = 24; // Middle page
    
    DrawString(xPos, yPos, text);
}

// Display message for specified duration (in seconds)
void ShowMessage(const char* text, uint8_t seconds) {
    DisplayCentered(text);
    delay(seconds * 1000);
}

// Display two lines of text
void DisplayTwoLines(const char* line1, const char* line2) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    uint8_t width1 = GetStringWidth(line1);
    int16_t x1 = (DISP_HOR_RESOLUTION - width1) / 2;
    DrawString(x1, 16, line1);
    
    uint8_t width2 = GetStringWidth(line2);
    int16_t x2 = (DISP_HOR_RESOLUTION - width2) / 2;
    DrawString(x2, 40, line2);
}

// ==================== INPUT COLLECTION ====================

// Collect multi-digit number from button presses
int16_t CollectDigits(uint8_t numDigits, const char* prompt) {
    char input[10] = {0};
    char display[30] = {0};
    uint8_t digitCount = 0;
    int16_t aggr[5] = {0, 0, 0, 0, 0};
    uint8_t digitRegistered = 0; // Flag to prevent multiple registration
    const int16_t THRESHOLD = 6;
    const int16_t RELEASE_THRESHOLD = 2; // More strict release detection
    const uint16_t timeout = 10;
    
    while (digitCount < numDigits) {
        ReadCTMU();
        
        // Update aggregate values
        for (uint8_t i = 0; i < 5; i++) {
            if (buttons[i]) aggr[i]++;
            else aggr[i]--;
            if (aggr[i] < 0) aggr[i] = 0;
            if (aggr[i] > 30) aggr[i] = 30;
        }
        
        // Check if all buttons are truly released (all aggregates near zero)
        uint8_t allReleased = 1;
        for (uint8_t i = 0; i < 5; i++) {
            if (aggr[i] > RELEASE_THRESHOLD) {
                allReleased = 0;
                break;
            }
        }
        
        // Reset flag when fully released
        if (allReleased && digitRegistered) {
            digitRegistered = 0;
        }
        
        // Only process new digit if not already registered
        if (!digitRegistered) {
            // Find highest aggregate button
            int16_t maxVal = THRESHOLD;
            uint8_t maxButton = 0xFF;
            for (uint8_t i = 0; i < 5; i++) {
                if (aggr[i] > maxVal) {
                    maxVal = aggr[i];
                    maxButton = i;
                }
            }
            
            // Register digit if button detected
            if (maxButton != 0xFF) {
                uint8_t digit = maxButton + 1; // Map buttons 0-4 to digits 1-5
                input[digitCount] = '0' + digit;
                digitCount++;
                
                // Update display
                sprintf(display, "%s: %s", prompt, input);
                DisplayCentered(display);
                
                digitRegistered = 1; // Lock further input until full release
            }
        }
        
        delay(timeout);
    }
    
    // Wait half a second after completing input before proceeding
    delay(500);
    
    // Convert string to number
    int16_t result = 0;
    for (uint8_t i = 0; i < numDigits; i++) {
        result = result * 10 + (input[i] - '0');
    }
    
    return result;
}

// ==================== MENU NAVIGATION ====================

// Wait for button press and return button number (0-4)
uint8_t WaitForButton() {
    int16_t aggr[5] = {0, 0, 0, 0, 0};
    uint8_t lastDetected = 0xFF;
    const int16_t THRESHOLD = 6;
    const uint16_t timeout = 10;
    
    while(1) {
        ReadCTMU();
        
        for (uint8_t i = 0; i < 5; i++) {
            if (buttons[i]) aggr[i]++;
            else aggr[i]--;
            if (aggr[i] < 0) aggr[i] = 0;
            if (aggr[i] > 30) aggr[i] = 30;
        }
        
        int16_t maxVal = THRESHOLD;
        uint8_t maxButton = 0xFF;
        for (uint8_t i = 0; i < 5; i++) {
            if (aggr[i] > maxVal) {
                maxVal = aggr[i];
                maxButton = i;
            }
        }
        
        if (maxButton != 0xFF && maxButton != lastDetected) {
            delay(200); // Debounce
            return maxButton;
        } else if (maxButton == 0xFF) {
            lastDetected = 0xFF;
        }
        
        delay(timeout);
    }
}

// ==================== MAIN APPLICATION ====================

int main(void) {
    INIT_CLOCK(); 
    CTMUInit(); 
    RGBMapColorPins();
    
    RGBTurnOnLED();
    ResetDevice();
    
    // Startup greeting
    ShowMessage("HELLO!", 3);
    
    // Main application loop
    while(1) {
        // Display main menu
        DisplayTwoLines("4=REGISTER", "2=LOGIN");
        
        // Wait for menu choice
        uint8_t choice = WaitForButton();
        
        if (choice == 3) {  // LEFT button = 4 = Register
            // Registration flow
            ShowMessage("REGISTER MENU", 1);
            ShowMessage("LOADING...", 2);
            
            // Prompt for ID
            DisplayTwoLines("PLEASE ENTER", "ID");
            delay(2000);
            
            // Collect 2-digit ID (automatically proceeds)
            int16_t userId = CollectDigits(2, "ID");
            
            // Prompt for Password
            DisplayTwoLines("PLEASE ENTER", "PWD");
            delay(2000);
            
            // Collect 4-digit password (automatically proceeds)
            int16_t password = CollectDigits(4, "PWD");
            
            // Show success
            DisplayTwoLines("REGISTRATION", "SUCCESSFUL!");
            delay(2000);
            ShowMessage("REDIRECTING...", 1);
            
        } else if (choice == 1) {  // RIGHT button = 2 = Login
            // Login flow
            ShowMessage("LOGIN MENU", 1);
            ShowMessage("LOADING...", 2);
            
            // Prompt for ID
            DisplayTwoLines("PLEASE ENTER", "ID");
            delay(2000);
            
            // Collect 2-digit ID (automatically proceeds)
            int16_t userId = CollectDigits(2, "ID");
            
            // Prompt for Password
            DisplayTwoLines("PLEASE ENTER", "PWD");
            delay(2000);
            
            // Collect 4-digit password (automatically proceeds)
            int16_t password = CollectDigits(4, "PWD");
            
            // Show result (validation will be added in Phase 2)
            ShowMessage("CHECKING...", 2);
            DisplayTwoLines("LOGIN", "SUCCESSFUL!");
            delay(2000);
            ShowMessage("REDIRECTING...", 1);
        }
    }
    
    RGBTurnOffLED();
}
