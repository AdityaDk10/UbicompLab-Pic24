/*
 * PIC24F Authentication System
 * 
 * A pattern-based authentication system using capacitive touch buttons
 * and OLED display for the PIC24F Starter Kit.
 * 
 * Features:
 *   - 2-digit user ID
 *   - 5-button swipe pattern password (Android-style)
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

// ==================== FUNCTION PROTOTYPES ====================

// UI Functions
void DisplayCentered(const char* text);
void ShowMessage(const char* text, uint8_t seconds);
void DisplayTwoLines(const char* line1, const char* line2);
void DrawMainMenu(uint8_t screenIndex, uint8_t selectedIndex);
void DrawListSubMenu(uint8_t selectedIndex);
void DisplayUserList(uint8_t filterType);

// Input Functions
uint8_t WaitForButton(void);
int16_t CollectDigits(uint8_t numDigits, const char* prompt);
void CollectPattern(uint8_t* pattern, uint16_t* timing);

// Database Functions
void InitDatabase(void);
int8_t FindUser(int16_t userId);
uint8_t RegisterUser(int16_t userId, uint8_t* pattern, uint16_t* timing);
uint8_t ValidateLogin(int16_t userId, uint8_t* pattern, uint16_t* timing, uint8_t* timingWarningOut, uint8_t* segmentMatches);
uint8_t DeleteUser(int16_t userId);

// Pattern Display Functions
void DrawPatternGrid(void);
void DrawPatternLines(uint8_t* pattern, uint8_t length);
void UpdatePatternDisplay(uint8_t* pattern, uint8_t length);

// Visual Feedback Functions
void DrawCheckmark(int16_t x, int16_t y);
void DrawX(int16_t x, int16_t y);
void ShowLoadingAnimation(const char* baseText, uint16_t durationMs);
void ShowSuccess(const char* message);
void ShowError(const char* message);
void ShowTimingAnalysis(uint8_t* segmentMatches, uint8_t totalSegments);

// Utility Functions
void delay(unsigned int milliseconds);
void BlinkRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t times, uint16_t onMs, uint16_t offMs);

// ==================== USER DATABASE ====================

#define PATTERN_LENGTH 5  // Fixed 5-button pattern

// User structure to hold credentials
typedef struct {
    int16_t userId;              // 2-digit ID
    uint8_t pattern[PATTERN_LENGTH];  // 5-button pattern (1-5)
    uint8_t isActive;            // 1 = slot used, 0 = empty
    uint8_t failedAttempts;      // Failed login attempts counter
    uint8_t isLoggedIn;         // 1 = currently logged in, 0 = not logged in
    uint16_t timing[PATTERN_LENGTH - 1]; // Inter-button timing in milliseconds (4 values for 5-button pattern)
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
        userDatabase[i].isLoggedIn = 0;
        for (uint8_t j = 0; j < PATTERN_LENGTH; j++) {
            userDatabase[i].pattern[j] = 0;
        }
        for (uint8_t j = 0; j < PATTERN_LENGTH - 1; j++) {
            userDatabase[i].timing[j] = 0;
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
uint8_t RegisterUser(int16_t userId, uint8_t* pattern, uint16_t* timing) {
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
            for (uint8_t j = 0; j < PATTERN_LENGTH - 1; j++) {
                userDatabase[i].timing[j] = timing[j];
            }
            userDatabase[i].isActive = 1;
            userDatabase[i].failedAttempts = 0;
            userDatabase[i].isLoggedIn = 0;
            userCount++;
            return 1;  // Success
        }
    }
    
    return 0;  // Should never reach here
}

// Validate login using pattern and timing with per-segment analysis.
// Returns 1 on success, 0 on failure.
// timingWarningOut: set to 1 if pattern matches but timing doesn't (warning case)
// segmentMatches: array of 4 values (1=match, 0=mismatch) for each timing segment
uint8_t ValidateLogin(int16_t userId, uint8_t* pattern, uint16_t* timing, uint8_t* timingWarningOut, uint8_t* segmentMatches) {
    const uint8_t TIMING_TOLERANCE_PERCENT = 40; // 40% tolerance for timing match
    const uint8_t MIN_SEGMENTS_REQUIRED = 2;     // At least 2/4 segments must match
    
    *timingWarningOut = 0;
    
    // Initialize segment matches array
    for (uint8_t i = 0; i < PATTERN_LENGTH - 1; i++) {
        segmentMatches[i] = 0;
    }
    
    int8_t index = FindUser(userId);
    if (index == -1) {
        return 0;  // User not found
    }
    
    // Check pattern
    if (!ComparePatterns(userDatabase[index].pattern, pattern)) {
        return 0;  // Wrong pattern
    }
    
    // Check timing with tolerance - analyze each segment individually
    uint8_t segmentsMatched = 0;
    
    for (uint8_t i = 0; i < PATTERN_LENGTH - 1; i++) {
        uint16_t stored = userDatabase[index].timing[i];
        uint16_t input = timing[i];
        
        if (stored == 0 || input == 0) {
            segmentMatches[i] = 0;
            continue;
        }
        
        // Calculate difference percentage
        uint16_t diff = (stored > input) ? (stored - input) : (input - stored);
        uint32_t diffPercent = (uint32_t)diff * 100 / stored;
        
        if (diffPercent <= TIMING_TOLERANCE_PERCENT) {
            segmentMatches[i] = 1;  // This segment matches
            segmentsMatched++;
        } else {
            segmentMatches[i] = 0;  // This segment doesn't match
        }
    }
    
    // Require at least MIN_SEGMENTS_REQUIRED segments to match for successful login
    if (segmentsMatched < MIN_SEGMENTS_REQUIRED) {
        return 0;  // Login failed - not enough segments matched
    }
    
    // If not all segments match, set warning flag (but still allow login)
    if (segmentsMatched < (PATTERN_LENGTH - 1)) {
        *timingWarningOut = 1;
    }
    
    return 1;  // Pattern matches and enough timing segments match
}

// Delete user from database, returns 1 on success, 0 on failure
uint8_t DeleteUser(int16_t userId) {
    int8_t index = FindUser(userId);
    if (index == -1) {
        return 0;  // User not found
    }
    
            // Mark slot as inactive and clear data
    userDatabase[index].isActive = 0;
    userDatabase[index].userId = 0;
    userDatabase[index].failedAttempts = 0;
    userDatabase[index].isLoggedIn = 0;
    for (uint8_t j = 0; j < PATTERN_LENGTH; j++) {
        userDatabase[index].pattern[j] = 0;
    }
    for (uint8_t j = 0; j < PATTERN_LENGTH - 1; j++) {
        userDatabase[index].timing[j] = 0;
    }
    
    userCount--;  // Decrement user count
    return 1;  // Success
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
    // Show pattern progress counter (e.g., "3/5")
    char progress[6];
    sprintf(progress, "%d/5", length);
    DrawString(100, 4, progress);
}

// ==================== VISUAL FEEDBACK ====================

// Draw a checkmark symbol (✓) using lines
void DrawCheckmark(int16_t x, int16_t y) {
    SetColor(WHITE);
    // Draw checkmark: two lines forming a check
    DrawLine(x, y + 2, x + 2, y + 4);
    DrawLine(x + 2, y + 4, x + 5, y + 1);
}

// Draw an X symbol (✗) using lines
void DrawX(int16_t x, int16_t y) {
    SetColor(WHITE);
    // Draw X: two diagonal lines
    DrawLine(x, y, x + 5, y + 5);
    DrawLine(x + 5, y, x, y + 5);
}

// Show loading animation with animated dots
// Returns after specified duration or when interrupted
void ShowLoadingAnimation(const char* baseText, uint16_t durationMs) {
    uint16_t elapsed = 0;
    uint8_t dotCount = 0;
    const uint16_t dotInterval = 300;  // Change dots every 300ms
    
    while (elapsed < durationMs) {
        char loadingText[20];
        uint8_t pos = 0;
        
        // Copy base text
        const char* src = baseText;
        while (*src && pos < 19) {
            loadingText[pos++] = *src++;
        }
        
        // Add dots based on animation state
        uint8_t dots = (dotCount % 4);  // Cycle: 0, 1, 2, 3 dots
        for (uint8_t i = 0; i < dots && pos < 19; i++) {
            loadingText[pos++] = '.';
        }
        loadingText[pos] = '\0';
        
        DisplayCentered(loadingText);
        
        delay(dotInterval);
        elapsed += dotInterval;
        dotCount++;
    }
}

// Show success message with checkmark
void ShowSuccess(const char* message) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    // Draw checkmark at top center
    DrawCheckmark(54, 8);
    
    // Draw message below checkmark
    uint8_t width = GetStringWidth(message);
    int16_t xPos = (DISP_HOR_RESOLUTION - width) / 2;
    DrawString(xPos, 30, message);
}

// Show error message with X symbol
void ShowError(const char* message) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    // Draw X at top center
    DrawX(54, 8);
    
    // Draw message below X
    uint8_t width = GetStringWidth(message);
    int16_t xPos = (DISP_HOR_RESOLUTION - width) / 2;
    DrawString(xPos, 30, message);
}

// Show per-segment timing analysis
// segmentMatches: array indicating which segments match (1=match, 0=mismatch)
// totalSegments: total number of segments (should be 4 for 5-button pattern)
void ShowTimingAnalysis(uint8_t* segmentMatches, uint8_t totalSegments) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    // Count matched segments
    uint8_t matchedCount = 0;
    for (uint8_t i = 0; i < totalSegments; i++) {
        if (segmentMatches[i]) {
            matchedCount++;
        }
    }
    
    // Display header
    DrawString(20, 4, "TIMING ANALYSIS:");
    
    // Display segment-by-segment results
    // Show each segment with ✓ or ✗
    // Adjusted spacing to prevent collision with summary
    for (uint8_t i = 0; i < totalSegments && i < 4; i++) {
        int16_t yPos = 16 + i * 11;  // Reduced spacing from 12 to 11, start at 16
        char segmentText[15];
        sprintf(segmentText, "SEG %d:", i + 1);
        DrawString(8, yPos, segmentText);
        
        // Draw checkmark or X based on match
        if (segmentMatches[i]) {
            DrawCheckmark(50, yPos);
        } else {
            DrawX(50, yPos);
        }
    }
    
    // Display summary - moved down to avoid collision
    char summary[20];
    sprintf(summary, "%d/%d MATCH", matchedCount, totalSegments);
    uint8_t width = GetStringWidth(summary);
    int16_t xPos = (DISP_HOR_RESOLUTION - width) / 2;
    DrawString(xPos, 58, summary);  // Moved from 56 to 58 for better spacing
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

// Blink RGB LED with given color a number of times
void BlinkRGB(uint8_t r, uint8_t g, uint8_t b, uint8_t times, uint16_t onMs, uint16_t offMs) {
    for (uint8_t i = 0; i < times; i++) {
        SetRGBs(r, g, b);       // LED on with requested color
        delay(onMs);
        SetRGBs(0, 0, 0);       // LED off
        delay(offMs);
    }
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

// Draw main menu with two side-by-side options
// screenIndex: 0 = Screen 1 (REGISTER | LOGIN), 1 = Screen 2 (DELETE | LIST)
// selectedIndex: 0 = Left option, 1 = Right option
void DrawMainMenu(uint8_t screenIndex, uint8_t selectedIndex) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);

    const char* leftText;
    const char* rightText;
    
    if (screenIndex == 0) {
        // Screen 1: REGISTER | LOGIN
        leftText = "REGISTER";
        rightText = "LOGIN";
    } else {
        // Screen 2: DELETE | LIST
        leftText = "DELETE";
        rightText = "LIST";
    }

    // Calculate positions for side-by-side layout
    // Screen is 128 pixels wide, 64 pixels tall
    const int16_t yCenter = 32;        // Vertical center of screen
    const int16_t xLeftCenter = 32;     // Center of left half (128/4)
    const int16_t xRightCenter = 96;    // Center of right half (128*3/4)
    
    // Draw left option (centered in left half)
    uint8_t widthLeft = GetStringWidth(leftText);
    int16_t xLeft = xLeftCenter - widthLeft / 2;
    DrawString(xLeft, yCenter, leftText);
    
    // Draw right option (centered in right half)
    uint8_t widthRight = GetStringWidth(rightText);
    int16_t xRight = xRightCenter - widthRight / 2;
    DrawString(xRight, yCenter, rightText);
    
    // Draw vertical separator line in the middle
    DrawLine(64, 20, 64, 44);  // Vertical line at screen center

    // Draw highlight rectangle around the selected option
    const int8_t paddingX = 6;
    const int8_t paddingY = 6;
    int16_t rectX, rectY, rectW, rectH;
    
    if (selectedIndex == 0) {
        // Left option selected
        rectX = xLeft - paddingX;
        rectY = yCenter - paddingY;
        rectW = widthLeft + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    } else {
        // Right option selected
        rectX = xRight - paddingX;
        rectY = yCenter - paddingY;
        rectW = widthRight + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    }

    // Draw rectangle outline using four lines
    DrawLine(rectX, rectY, rectX + rectW, rectY);               // top
    DrawLine(rectX, rectY + rectH, rectX + rectW, rectY + rectH); // bottom
    DrawLine(rectX, rectY, rectX, rectY + rectH);               // left
    DrawLine(rectX + rectW, rectY, rectX + rectW, rectY + rectH); // right
}

// Draw LIST submenu with a rectangular highlight around the selected option
// selectedIndex: 0 = REGISTERED, 1 = LOGGED IN, 2 = LOCKED, 3 = DELETED
void DrawListSubMenu(uint8_t selectedIndex) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);

    const char* regUsersText = "REGISTERED";
    const char* loggedInText = "LOGGED IN";
    const char* lockedText = "LOCKED";
    const char* deletedText = "DELETED";

    // Y positions for the four options (adjusted spacing to fit all 4 items)
    const int16_t yReg = 6;
    const int16_t yLogged = 22;
    const int16_t yLocked = 38;
    const int16_t yDeleted = 54;

    // Draw text centered
    uint8_t widthReg = GetStringWidth(regUsersText);
    int16_t xReg = (DISP_HOR_RESOLUTION - widthReg) / 2;
    DrawString(xReg, yReg, regUsersText);

    uint8_t widthLogged = GetStringWidth(loggedInText);
    int16_t xLogged = (DISP_HOR_RESOLUTION - widthLogged) / 2;
    DrawString(xLogged, yLogged, loggedInText);

    uint8_t widthLocked = GetStringWidth(lockedText);
    int16_t xLocked = (DISP_HOR_RESOLUTION - widthLocked) / 2;
    DrawString(xLocked, yLocked, lockedText);

    uint8_t widthDeleted = GetStringWidth(deletedText);
    int16_t xDeleted = (DISP_HOR_RESOLUTION - widthDeleted) / 2;
    DrawString(xDeleted, yDeleted, deletedText);

    // Draw highlight rectangle around the selected option
    const int8_t paddingX = 4;
    const int8_t paddingY = 2;
    int16_t rectX, rectY, rectW, rectH;

    if (selectedIndex == 0) {
        rectX = xReg - paddingX;
        rectY = yReg - paddingY;
        rectW = widthReg + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    } else if (selectedIndex == 1) {
        rectX = xLogged - paddingX;
        rectY = yLogged - paddingY;
        rectW = widthLogged + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    } else if (selectedIndex == 2) {
        rectX = xLocked - paddingX;
        rectY = yLocked - paddingY;
        rectW = widthLocked + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    } else {  // selectedIndex == 3 (DELETED)
        rectX = xDeleted - paddingX;
        rectY = yDeleted - paddingY;
        rectW = widthDeleted + 2 * paddingX;
        rectH = 12 + 2 * paddingY;
    }

    // Draw rectangle outline using four lines
    DrawLine(rectX, rectY, rectX + rectW, rectY);               // top
    DrawLine(rectX, rectY + rectH, rectX + rectW, rectY + rectH); // bottom
    DrawLine(rectX, rectY, rectX, rectY + rectH);               // left
    DrawLine(rectX + rectW, rectY, rectX + rectW, rectY + rectH); // right
}

// Display list of users based on filter type
// filterType: 0 = all registered, 1 = logged in, 2 = locked, 3 = deleted
void DisplayUserList(uint8_t filterType) {
    SetColor(BLACK);
    ClearDevice();
    SetColor(WHITE);
    
    // Set header based on filter type
    const char* header = "";
    if (filterType == 0) {
        header = "REGISTERED:";
    } else if (filterType == 1) {
        header = "LOGGED IN:";
    } else if (filterType == 2) {
        header = "LOCKED:";
    } else {
        header = "DELETED:";
    }
    
    uint8_t headerWidth = GetStringWidth(header);
    int16_t xHeader = (DISP_HOR_RESOLUTION - headerWidth) / 2;
    DrawString(xHeader, 4, header);
    
    // Count and display users
    uint8_t displayCount = 0;
    uint8_t shownCount = 0;
    
    for (uint8_t i = 0; i < 10 && shownCount < 4; i++) {
        uint8_t shouldDisplay = 0;
        
        if (filterType == 0) {
            // Show all registered users
            shouldDisplay = userDatabase[i].isActive;
        } else if (filterType == 1) {
            // Show logged in users
            shouldDisplay = userDatabase[i].isActive && userDatabase[i].isLoggedIn;
        } else if (filterType == 2) {
            // Show locked users (3 failed attempts)
            shouldDisplay = userDatabase[i].isActive && userDatabase[i].failedAttempts >= 3;
        } else {
            // Deleted users - these are removed from database, so show message
            shouldDisplay = 0;
        }
        
        if (shouldDisplay) {
            char userLine[20];
            sprintf(userLine, "ID: %02d", userDatabase[i].userId);
            DrawString(8, 18 + displayCount * 12, userLine);
            displayCount++;
            shownCount++;
        }
    }
    
    // Handle special cases
    if (filterType == 3) {
        // Deleted users are removed from database
        DrawString(8, 18, "NOT TRACKED");
        DrawString(8, 30, "(REMOVED FROM");
        DrawString(8, 42, "DATABASE)");
    } else if (shownCount == 0 && displayCount == 0) {
        // No users found for this filter
        DrawString(8, 18, "NONE");
    }
    
    // Wait for button press to return
    delay(2000);
    WaitForButton();
}


// ==================== PATTERN INPUT ====================

// Check if button is already in pattern (no repeats)
uint8_t IsInPattern(uint8_t* pattern, uint8_t length, uint8_t button) {
    for (uint8_t i = 0; i < length; i++) {
        if (pattern[i] == button) return 1;
    }
    return 0;
}

// Collect pattern using swipe detection (like ball movement)
// Also captures timing between button presses in milliseconds
void CollectPattern(uint8_t* pattern, uint16_t* timing) {
    uint8_t patternLen = 0;
    int16_t aggr[5] = {0, 0, 0, 0, 0};
    uint8_t lastButton = 0xFF;  // Last button added to pattern
    const int16_t THRESHOLD = 6;
    const uint16_t timeout = 10;
    
    uint32_t lastButtonTime = 0;  // Time when last button was added (in loop iterations)
    uint32_t currentTime = 0;     // Current time counter (in loop iterations)
    
    // Initialize timing array
    for (uint8_t i = 0; i < PATTERN_LENGTH - 1; i++) {
        timing[i] = 0;
    }
    
    // Show initial grid
    SetColor(BLACK);
    ClearDevice();
    DrawPatternGrid();
    
    // Collect pattern until 5 buttons
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
                // Calculate timing since last button (if not first button)
                if (patternLen > 0) {
                    // Time difference in loop iterations, convert to milliseconds
                    uint32_t timeDiff = currentTime - lastButtonTime;
                    timing[patternLen - 1] = (uint16_t)(timeDiff * timeout);  // Convert to milliseconds
                }
                
                // Add to pattern
                pattern[patternLen] = buttonNum;
                patternLen++;
                lastButton = currentButton;
                lastButtonTime = currentTime;  // Record time when this button was added
                
                // Update display with new line
                UpdatePatternDisplay(pattern, patternLen);
            }
        }
        
        delay(timeout);
        currentTime++;  // Increment time counter
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
        // Main menu with two screens, side-by-side options:
        // Screen 0: REGISTER (left) | LOGIN (right)
        // Screen 1: DELETE (left) | LIST (right)
        // Button mapping (index from WaitForButton):
        //   0 = UP (go to screen 0), 2 = DOWN (go to screen 1), 
        //   3 = LEFT (select left option), 1 = RIGHT (select right option), 4 = CENTER (confirm selection)
        uint8_t screenIndex = 0;      // 0 = Screen 1 (REGISTER|LOGIN), 1 = Screen 2 (DELETE|LIST)
        uint8_t selectedIndex = 0;    // 0 = Left option, 1 = Right option
        uint8_t inMenu = 1;

        while (inMenu) {
            DrawMainMenu(screenIndex, selectedIndex);
            uint8_t btn = WaitForButton();

            if (btn == 0) {          // UP (button 1) - go to screen 0
                screenIndex = 0;
                selectedIndex = 0;   // Reset to left option
                // Redraw immediately to show screen change
                DrawMainMenu(screenIndex, selectedIndex);
                delay(100);  // Small delay to ensure screen update is visible
            } else if (btn == 2) {   // DOWN (button 3) - go to screen 1
                screenIndex = 1;
                selectedIndex = 0;   // Reset to left option
                // Redraw immediately to show screen change
                DrawMainMenu(screenIndex, selectedIndex);
                delay(100);  // Small delay to ensure screen update is visible
            } else if (btn == 3) {   // LEFT (button 4) - select left option
                selectedIndex = 0;
            } else if (btn == 1) {   // RIGHT (button 2) - select right option
                selectedIndex = 1;
            } else if (btn == 4) {   // CENTER (button 5) = confirm selection
                inMenu = 0;
            }
        }

        // Determine which option was selected based on screen and position
        uint8_t finalSelection;
        if (screenIndex == 0) {
            // Screen 0: REGISTER (0) or LOGIN (1)
            finalSelection = selectedIndex;
        } else {
            // Screen 1: DELETE (2) or LIST (3)
            finalSelection = selectedIndex + 2;
        }

        if (finalSelection == 0) {  // REGISTER selected
            // Registration flow
            ShowMessage("REGISTER MENU", 1);
            ShowMessage("LOADING...", 2);
            
            // Check if database is full
            if (userCount >= 10) {
                // Failure: database full -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
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
                // Failure: ID already exists -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                DisplayTwoLines("ID ALREADY", "EXISTS!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Prompt for Pattern
            DisplayTwoLines("DRAW YOUR", "PATTERN");
            delay(2000);
            
            // Collect 5-button pattern (swipe-based)
            uint8_t pattern[PATTERN_LENGTH];
            uint16_t timing[PATTERN_LENGTH - 1];
            CollectPattern(pattern, timing);
            
            // Register the user
            if (RegisterUser(userId, pattern, timing)) {
                // Success: registration -> GREEN blink
                BlinkRGB(0, 255, 0, 3, 200, 200);
                ShowSuccess("REGISTRATION SUCCESS");
                delay(2000);
            } else {
                // Failure: registration -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                ShowError("REGISTRATION FAILED");
                delay(2000);
            }
            ShowMessage("REDIRECTING...", 1);
            
        } else if (finalSelection == 1) {  // LOGIN selected
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
                ShowLoadingAnimation("CHECKING", 1000);
                // Failure: invalid user ID -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                ShowError("INVALID USER ID");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Check if account is locked (3 failed attempts)
            if (userDatabase[userIndex].failedAttempts >= 3) {
                ShowLoadingAnimation("CHECKING", 1000);
                // Failure: account already locked -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                ShowError("ACCOUNT LOCKED");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu - don't allow login
            }
            
            // Prompt for Pattern
            DisplayTwoLines("DRAW YOUR", "PATTERN");
            delay(2000);
            
            // Collect 5-button pattern (swipe-based)
            uint8_t pattern[PATTERN_LENGTH];
            uint16_t timing[PATTERN_LENGTH - 1];
            CollectPattern(pattern, timing);
            
            // Validate credentials
            ShowLoadingAnimation("CHECKING", 2000);
            uint8_t timingWarning = 0;
            uint8_t segmentMatches[PATTERN_LENGTH - 1];  // Array to store segment match results
            if (ValidateLogin(userId, pattern, timing, &timingWarning, segmentMatches)) {
                // Login successful - reset failed attempts and mark as logged in
                userDatabase[userIndex].failedAttempts = 0;
                userDatabase[userIndex].isLoggedIn = 1;
                
                // Always show timing analysis after successful login
                ShowTimingAnalysis(segmentMatches, PATTERN_LENGTH - 1);
                delay(5000);
                
                // If timing warning exists, show additional message
                if (timingWarning) {
                    DisplayTwoLines("TIMING WARNING", "BUT LOGIN OK");
                    delay(2000);
                }
                
                // Success: login -> GREEN blink
                BlinkRGB(0, 255, 0, 3, 200, 200);
                ShowSuccess("LOGIN SUCCESS");
                delay(2000);
            } else {
                // Login failed - show timing analysis if pattern was correct but timing failed
                // Calculate segments for display to show why login failed
                uint8_t failedSegments[PATTERN_LENGTH - 1];
                uint8_t segmentsMatched = 0;
                
                for (uint8_t i = 0; i < PATTERN_LENGTH - 1; i++) {
                    uint16_t stored = userDatabase[userIndex].timing[i];
                    uint16_t input = timing[i];
                    if (stored == 0 || input == 0) {
                        failedSegments[i] = 0;
                    } else {
                        uint16_t diff = (stored > input) ? (stored - input) : (input - stored);
                        uint32_t diffPercent = (uint32_t)diff * 100 / stored;
                        if (diffPercent <= 40) {
                            failedSegments[i] = 1;
                            segmentsMatched++;
                        } else {
                            failedSegments[i] = 0;
                        }
                    }
                }
                
                // Show timing analysis to explain failure
                ShowTimingAnalysis(failedSegments, PATTERN_LENGTH - 1);
                delay(3000);
                
                // Show failure reason
                if (segmentsMatched < 2) {
                    DisplayTwoLines("TIMING FAILED", "NEED 2/4 MATCH");
                } else {
                    DisplayTwoLines("LOGIN FAILED", "");
                }
                delay(2000);
                
                // Increment failed attempts
                userDatabase[userIndex].failedAttempts++;
                
                // Check if account should be locked now
                if (userDatabase[userIndex].failedAttempts >= 3) {
                    // Failure: account just locked -> RED blink
                    BlinkRGB(255, 0, 0, 3, 200, 200);
                    ShowError("ACCOUNT LOCKED");
                    delay(3000);
                } else {
                    // Show remaining attempts
                    // Failure: wrong pattern/timing but attempts left -> RED blink
                    BlinkRGB(255, 0, 0, 3, 200, 200);
                    char msg[30];
                    uint8_t remaining = 3 - userDatabase[userIndex].failedAttempts;
                    sprintf(msg, "%u ATTEMPTS LEFT", remaining);
                    DisplayCentered(msg);
                    delay(3000);
                }
            }
            ShowMessage("REDIRECTING...", 1);
            
        } else if (finalSelection == 2) {  // DELETE selected
            // Delete user flow
            ShowMessage("DELETE MENU", 1);
            ShowMessage("LOADING...", 2);
            
            // Check if database is empty
            if (userCount == 0) {
                DisplayTwoLines("FIRST REGISTER", "USERS!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Prompt for ID
            DisplayTwoLines("PLEASE ENTER", "ID");
            delay(2000);
            
            // Collect 2-digit ID (automatically proceeds)
            int16_t userId = CollectDigits(2, "ID");
            
            // Check if user exists
            int8_t userIndex = FindUser(userId);
            if (userIndex == -1) {
                ShowLoadingAnimation("CHECKING", 1000);
                // Failure: invalid user ID -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                ShowError("INVALID USER ID");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // Prompt for Pattern (authentication required)
            DisplayTwoLines("AUTHENTICATE", "TO DELETE");
            delay(2000);
            
            // Collect 5-button pattern (swipe-based)
            uint8_t pattern[PATTERN_LENGTH];
            uint16_t timing[PATTERN_LENGTH - 1];
            CollectPattern(pattern, timing);
            
            // Validate credentials
            ShowLoadingAnimation("CHECKING", 2000);
            uint8_t timingWarning = 0;
            uint8_t segmentMatches[PATTERN_LENGTH - 1];  // Array to store segment match results
            if (ValidateLogin(userId, pattern, timing, &timingWarning, segmentMatches)) {
                // Always show timing analysis after successful authentication
                ShowTimingAnalysis(segmentMatches, PATTERN_LENGTH - 1);
                delay(5000);
                
                // If timing warning exists, show additional message
                if (timingWarning) {
                    DisplayTwoLines("TIMING WARNING", "BUT AUTH OK");
                    delay(2000);
                }
                
                // Authentication successful - show confirmation alert
                DisplayTwoLines("DELETE USER?", "CENTER=YES");
                delay(2000);
                
                // Wait for confirmation (CENTER = confirm, any other = cancel)
                uint8_t confirmBtn = WaitForButton();
                
                if (confirmBtn == 4) {  // CENTER = YES, confirm deletion
                    // Delete the user
                    if (DeleteUser(userId)) {
                        // Success: deletion -> GREEN blink
                        BlinkRGB(0, 255, 0, 3, 200, 200);
                        ShowSuccess("USER DELETED");
                        delay(2000);
                    } else {
                        // Failure: deletion failed -> RED blink
                        BlinkRGB(255, 0, 0, 3, 200, 200);
                        ShowError("DELETE FAILED");
                        delay(2000);
                    }
                } else {
                    // User cancelled - no action
                    DisplayTwoLines("CANCELLED", "");
                    delay(2000);
                }
            } else {
                // Authentication failed - don't allow deletion
                // Failure: wrong pattern -> RED blink
                BlinkRGB(255, 0, 0, 3, 200, 200);
                ShowError("AUTH FAILED");
                delay(3000);
            }
            ShowMessage("REDIRECTING...", 1);
            
        } else if (finalSelection == 3) {  // LIST selected
            // LIST submenu navigation
            ShowMessage("LIST MENU", 1);
            
            // Check if database is empty
            if (userCount == 0) {
                DisplayTwoLines("NO USERS", "REGISTERED!");
                delay(3000);
                ShowMessage("REDIRECTING...", 1);
                continue;  // Back to menu
            }
            
            // LIST submenu with navigable highlight box:
            // Button mapping (index from WaitForButton):
            //   0 = UP, 2 = DOWN, 4 = CENTER (select), 3 = LEFT (back)
            uint8_t listSubIndex = 0;   // 0 = REGISTERED, 1 = LOGGED IN, 2 = LOCKED, 3 = DELETED
            uint8_t inListSubMenu = 1;
            
            while (inListSubMenu) {
                DrawListSubMenu(listSubIndex);
                uint8_t btn = WaitForButton();
                
                if (btn == 0) {          // UP
                    if (listSubIndex > 0) {
                        listSubIndex--;
                    }
                } else if (btn == 2) {   // DOWN
                    if (listSubIndex < 3) {
                        listSubIndex++;
                    }
                } else if (btn == 4) {   // CENTER = select
                    inListSubMenu = 0;
                } else if (btn == 3) {   // LEFT = back to main menu
                    inListSubMenu = 0;
                    listSubIndex = 255;  // Special value to indicate back
                }
                // Other buttons (1=RIGHT) are ignored in menu
            }
            
            // If user selected an option (not back), display the list
            if (listSubIndex != 255) {
                DisplayUserList(listSubIndex);
            }
            
            ShowMessage("REDIRECTING...", 1);
    }
    }
    
    RGBTurnOffLED();
}
