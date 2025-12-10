/*
 * PIC24F Authentication System
 * 
 * A pattern-based authentication system using capacitive touch buttons
 * and OLED display for the PIC24F Starter Kit.
 * 
 * Features:
 *   - 2-digit user ID
 *   - 4-button swipe pattern password (Android-style)
 *   - Real-time pattern visualization
 *   - Multi-user support (up to 10 users)
 * 
 * Hardware: PIC24F Starter Kit 1
 * 
 * Original hardware drivers by: kvl@eti.uni-siegen.de
 * Authentication system by: AdityaDk10
 * Repository: https://github.com/AdityaDk10/UbicompLab-Pic24
 */

#include "PIC24FStarter.h"
#include <stdio.h>

// ==================== USER DATABASE ====================

#define PATTERN_LENGTH 4  // Fixed 4-button pattern

// User structure to hold credentials
typedef struct {
    int16_t userId;              // 2-digit ID
    uint8_t pattern[PATTERN_LENGTH];  // 4-button pattern (1-5)
    uint8_t isActive;            // 1 = slot used, 0 = empty
    uint8_t failedAttempts;      // Failed login attempts counter
} User;

// Database to store up to 10 users
User userDatabase[10];
uint8_t userCount = 0;

// Button positions on screen (for pattern display)
// Button order: 0=UP, 1=RIGHT, 2=DOWN, 3=LEFT, 4=CENTER
const uint8_t buttonX[5] = {64, 100, 64, 28, 64};   // X coordinates
const uint8_t buttonY[5] = {12, 32, 52, 32, 32};    // Y coordinates

// Initialize database (mark all slots as empty)
void InitDatabase() {
    for (uint8_t i = 0; i < 10; i++) {
        userDatabase[i].userId = 0;
        userDatabase[i].isActive = 0;
        userDatabase[i].failedAttempts = 0;
        for (uint8_t j = 0; j < PATTERN_LENGTH; j++) {
            userDatabase[i].pattern[j] = 0;
        }
    }
    userCount = 0;
}

// Find user by ID, returns index (0-9) or -1 if not found
int8_t FindUser(int16_t userId) {
    for (uint8_t i = 0; i < 10; i++) {
        if (userDatabase[i].isActive && userDatabase[i].userId == userId) {
            return i;  // Found at index i
        }
    }
    return -1;  // Not found
}

// Compare two patterns, returns 1 if match, 0 if different
uint8_t ComparePatterns(uint8_t* pattern1, uint8_t* pattern2) {
    for (uint8_t i = 0; i < PATTERN_LENGTH; i++) {
        if (pattern1[i] != pattern2[i]) {
            return 0;  // Mismatch
        }
    }
    return 1;  // Match
}

// Register new user, returns 1 on success, 0 on failure
uint8_t RegisterUser(int16_t userId, uint8_t* pattern) {
    // Check if database is full
    if (userCount >= 10) {
        return 0;  // Database full
    }
    
    // Check if user ID already exists
    if (FindUser(userId) != -1) {
        return 0;  // ID already exists
    }
    
    // Find first empty slot and add user
    for (uint8_t i = 0; i < 10; i++) {
        if (!userDatabase[i].isActive) {
            userDatabase[i].userId = userId;
            for (uint8_t j = 0; j < PATTERN_LENGTH; j++) {
                userDatabase[i].pattern[j] = pattern[j];
            }
            userDatabase[i].isActive = 1;
            userDatabase[i].failedAttempts = 0;
            userCount++;
            return 1;  // Success
        }
    }
    
    return 0;  // Should never reach here
}

// Validate login, returns 1 on success, 0 on failure
uint8_t ValidateLogin(int16_t userId, uint8_t* pattern) {
    int8_t index = FindUser(userId);
    
    if (index == -1) {
        return 0;  // User not found
    }
    
    // Check pattern
    if (ComparePatterns(userDatabase[index].pattern, pattern)) {
        return 1;  // Login successful
    }
    
    return 0;  // Wrong pattern
}

// ==================== PATTERN DISPLAY ====================

// Draw the 5 button positions as dots on screen
void DrawPatternGrid() {
    SetColor(WHITE);
    for (uint8_t i = 0; i < 5; i++) {
        DrawFilledCircle(buttonX[i], buttonY[i], 3);
    }
}

// Draw pattern lines connecting buttons
void DrawPatternLines(uint8_t* pattern, uint8_t length) {
    SetColor(WHITE);
    for (uint8_t i = 1; i < length; i++) {
        uint8_t prev = pattern[i-1] - 1;  // Convert 1-5 to 0-4 index
        uint8_t curr = pattern[i] - 1;
        DrawLine(buttonX[prev], buttonY[prev], buttonX[curr], buttonY[curr]);
    }
}

// Draw current pattern state (grid + lines so far)
void UpdatePatternDisplay(uint8_t* pattern, uint8_t length) {
    SetColor(BLACK);
    ClearDevice();
    DrawPatternGrid();
    if (length > 0) {
        DrawPatternLines(pattern, length);
        // Highlight last touched button
        uint8_t last = pattern[length-1] - 1;
        DrawFilledCircle(buttonX[last], buttonY[last], 5);
    }
}

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

// ==================== PATTERN INPUT ====================

// Check if button is already in pattern (no repeats)
uint8_t IsInPattern(uint8_t* pattern, uint8_t length, uint8_t button) {
    for (uint8_t i = 0; i < length; i++) {
        if (pattern[i] == button) return 1;
    }
    return 0;
}

// Collect 4-button pattern using swipe detection (like ball movement)
void CollectPattern(uint8_t* pattern) {
    uint8_t patternLen = 0;
    int16_t aggr[5] = {0, 0, 0, 0, 0};
    uint8_t lastButton = 0xFF;  // Last button added to pattern
    const int16_t THRESHOLD = 6;
    const int16_t RELEASE_THRESHOLD = 2;
    const uint16_t timeout = 10;
    
    // Show initial grid
    SetColor(BLACK);
    ClearDevice();
    DrawPatternGrid();
    
    // Collect pattern until 4 buttons
    while (patternLen < PATTERN_LENGTH) {
        ReadCTMU();
        
        // Update aggregate values (same as ball movement logic)
        for (uint8_t i = 0; i < 5; i++) {
            if (buttons[i]) aggr[i]++;
            else aggr[i]--;
            if (aggr[i] < 0) aggr[i] = 0;
            if (aggr[i] > 30) aggr[i] = 30;
        }
        
        // Find which button currently has highest aggregate
        int16_t maxVal = THRESHOLD;
        uint8_t currentButton = 0xFF;
        for (uint8_t i = 0; i < 5; i++) {
            if (aggr[i] > maxVal) {
                maxVal = aggr[i];
                currentButton = i;
            }
        }
        
        // If touching a valid button
        if (currentButton != 0xFF) {
            uint8_t buttonNum = currentButton + 1;  // Convert 0-4 to 1-5
            
            // Check if it's a NEW button (not the same as last, not already in pattern)
            if (currentButton != lastButton && !IsInPattern(pattern, patternLen, buttonNum)) {
                // Add to pattern
                pattern[patternLen] = buttonNum;
                patternLen++;
                lastButton = currentButton;
                
                // Update display with new line
                UpdatePatternDisplay(pattern, patternLen);
            }
        }
        
        delay(timeout);
    }
    
    // Pattern complete - show final result for a moment
    delay(500);
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
    
    // Initialize user database
    InitDatabase();
    
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
            
            // Check if database is full
            if (userCount >= 10) {
                DisplayTwoLines("DATABASE", "FULL!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Prompt for ID
            DisplayTwoLines("PLEASE ENTER", "ID");
            delay(2000);
            
            // Collect 2-digit ID (automatically proceeds)
            int16_t userId = CollectDigits(2, "ID");
            
            // Check if ID already exists
            if (FindUser(userId) != -1) {
                DisplayTwoLines("ID ALREADY", "EXISTS!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Prompt for Pattern
            DisplayTwoLines("DRAW YOUR", "PATTERN");
            delay(2000);
            
            // Collect 4-button pattern (swipe-based)
            uint8_t pattern[PATTERN_LENGTH];
            CollectPattern(pattern);
            
            // Register the user
            if (RegisterUser(userId, pattern)) {
                DisplayTwoLines("REGISTRATION", "SUCCESSFUL!");
                delay(2000);
            } else {
                DisplayTwoLines("REGISTRATION", "FAILED!");
                delay(2000);
            }
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
            
            // Check if user exists
            int8_t userIndex = FindUser(userId);
            if (userIndex == -1) {
                ShowMessage("CHECKING...", 1);
                DisplayTwoLines("INVALID", "USER ID!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Check if account is locked (3 failed attempts)
            if (userDatabase[userIndex].failedAttempts >= 3) {
                ShowMessage("CHECKING...", 1);
                DisplayTwoLines("ACCOUNT", "LOCKED!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu - don't allow login
            }
            
            // Prompt for Pattern
            DisplayTwoLines("DRAW YOUR", "PATTERN");
            delay(2000);
            
            // Collect 4-button pattern (swipe-based)
            uint8_t pattern[PATTERN_LENGTH];
            CollectPattern(pattern);
            
            // Validate credentials
            ShowMessage("CHECKING...", 2);
            if (ValidateLogin(userId, pattern)) {
                // Login successful - reset failed attempts
                userDatabase[userIndex].failedAttempts = 0;
                DisplayTwoLines("LOGIN", "SUCCESSFUL!");
                delay(2000);
            } else {
                // Login failed - increment failed attempts
                userDatabase[userIndex].failedAttempts++;
                
                // Check if account should be locked now
                if (userDatabase[userIndex].failedAttempts >= 3) {
                    DisplayTwoLines("ACCOUNT", "LOCKED!");
                    delay(3000);
                } else {
                    // Show remaining attempts
                    char msg[30];
                    uint8_t remaining = 3 - userDatabase[userIndex].failedAttempts;
                    sprintf(msg, "%d ATTEMPTS LEFT", remaining);
                    DisplayTwoLines("WRONG PATTERN!", msg);
                    delay(3000);
                }
            }
            ShowMessage("REDIRECTING...", 1);
    }
    }
    
    RGBTurnOffLED();
}
