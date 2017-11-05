#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include "TimeLib.h"
#include <Wire.h>

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

// Make my own character which will be a full block to print a kind of cursor on screen
byte block[8] = {B11111, B11111, B11111, B11111, B11111, B11111, B11111, B11111};
// Make more custom characters to display arrows on the screen
byte upArrow[8] = {
  B00000,
  B00000,
  B00000,
  B00100,
  B01110,
  B11111,
  B00000,
  B00000
};
byte downArrow[8] = {
  B00000,
  B00000,
  B00000,
  B11111,
  B01110,
  B00100,
  B00000,
  B00000
};
byte rightArrow[8] = {
  B00000,
  B00000,
  B01000,
  B01100,
  B01110,
  B01100,
  B01000,
  B00000
};
byte leftArrow[8] = {
  B00000,
  B00000,
  B00010,
  B00110,
  B01110,
  B00110,
  B00010,
  B00000
};

// Define currentMode constants.
enum displayModeSelection {
  CURRENTTIME, UPDATETIME, ALARMTIME
} displayMode;

enum currentTimeSelection {
  HOURS, MINUTES, SECONDS
} currentSelection = HOURS;

enum cursorPositionSelection {
  HOUR, MINUTE, SECOND
} cursorPosition = HOUR;




// Used to store if a button has been pressed once already
bool buttonPressed = false;

// Used to store is the alarm is on or off, and hence whether is should go off at the alarmTime
bool alarmSet = false;

//Used to count how many iterations a function has gone through, to allow a blinking cursor to appear every x iterations;
int iterationCount = 0;
int iteration = 0;


struct TIME {
  int minutes = 0;
  int hours = 0;
  int seconds = 0;
} alarmTime;

char text[60];
int textDelay = 0;
bool alarmSounding = false;
bool snooze = false;


void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  lcd.begin(16, 2);

  setTime(23, 59, 55, 1, 1, 2017);

  //  Set the displayMode to CURRENTTIME to display the current time by default
  displayMode = CURRENTTIME;

  lcd.createChar(1, block);
  lcd.createChar(2, upArrow);
  lcd.createChar(3, downArrow);
  lcd.createChar(4, leftArrow);
  lcd.createChar(5, rightArrow);

  // init the scrolling text using sprintf to get custom characters in the string
  // use 8 spaces at the end of the text as a really inefficent lazy way of removing repeat characters as they move across the screen
  // and prevent me having to clear the lcd every frame which would create a flicker on the screen
  sprintf(text, " SEL: Change Mode, %c%c Change Time, %c%c Move Cursor        ", char(2), char(3), char(4), char(5));
  Serial.print(text);
}

void loop() {
  // put your main code here, to run repeatedly:
  int time = millis();




  // -------------- INPUT -------------------


  uint8_t buttons = lcd.readButtons();

  if (buttons && !buttonPressed) {
    switch (buttons) {
      case BUTTON_UP:
        if (alarmSounding) {
          snooze = true;

          // To make sure ths snooze is actually
          alarmTime.seconds = (second() + 30) % 60;
          if (alarmTime.seconds < 30) alarmTime.minutes = (minute() + 1);

          Serial.print(alarmTime.minutes);
          Serial.print(" : ");
          Serial.println(alarmTime.seconds);

          alarmSounding = false;
          lcd.clear();
        } else {
          addTime();
        }
        break;
      case BUTTON_DOWN:
        minusTime();
        break;
      case BUTTON_RIGHT:
        cursorPosition = (cursorPositionSelection)((int)(cursorPosition + 1) % 3);
        break;
      case BUTTON_LEFT:
        cursorPosition = (cursorPositionSelection)((int)(cursorPosition - 1) % 3);
        break;
      case BUTTON_SELECT:
        if ((alarmSounding) || (snooze)) {
          alarmSounding = false;
          snooze = false;
          alarmSet = false;
          displayMode = CURRENTTIME;
          lcd.clear();
        }
        else {
          displayMode = (displayModeSelection)((int)(displayMode + 1) % 3);
          lcd.clear();
        }
        break;

    }
    buttonPressed = true;
  } else if (!buttons) {
    buttonPressed = false;
  }




  // -------------- PROCESSING -------------------

  // Check if the current time matches the alarm time, if so set off the alarm
  if ((hour() == alarmTime.hours) && (minute() == alarmTime.minutes) && (second() == alarmTime.seconds) && (alarmSet)) {
    alarmSounding = true;
    snooze = false;
  }

  // -------------- OUTPUT -------------------
  switch (displayMode) {
    case UPDATETIME:
      printUpdateTime();
      break;
    case CURRENTTIME:
      if ((!alarmSounding) && (!snooze)) printCurrentTime();
      if (alarmSounding) soundAlarm(); // I love unnecessery verbosity apparently
      if (snooze) snoozeDisplay();
      break;
    case ALARMTIME:
      printAlarmTime();
      break;
  }

  if ((displayMode != CURRENTTIME) && (!alarmSounding) && (!snooze)) {
    textDelay = (textDelay + 1) % 10;
    if (textDelay == 9) scrollingText();
  }
  else if ((displayMode == CURRENTTIME) && (!alarmSounding) && (!snooze)) {
    lcd.setCursor(0, 1);
    lcd.print("SEL: Change Mode");
  }


  // -------------- FRAME DELAY --------------

  // If a 'frame' somehow takes less than 1/60 of a second (averages about 3/5th of a second in my tests) delay it to make the max fps 60 fps
  while ((millis() - time) < 16) {
    delayMicroseconds(100);
  }

  // Print time taken to complete a 'frame', and fps using this value (DEBUG)
  //  Serial.print(1000.0/(float)(millis()-time));
  //  Serial.print("  ");
  //  Serial.println(millis()-time);

}

// ---------------------HELPER FUNCTIONS -------------------------

void printCurrentTime() {
  int secDisplay = second();
  int minDisplay = minute();
  int hourDisplay = hour();
  char currentTimeStr[8];

  //  Format currentTimeStr to print hour, minute and second with leading zeros (so every bit is 2 characters)
  sprintf(currentTimeStr, "%02d:%02d:%02d", hourDisplay, minDisplay, secDisplay);

  // Print the time to the middle of the first row of the screen
  lcd.setCursor(4, 0);
  lcd.print(currentTimeStr);
  (alarmSet) ? lcd.setBacklight(0x2) : lcd.setBacklight(0x6); // set backlight to a green if the alarm is set, or blue if the alarm is not set
}

void printUpdateTime() {
  int secDisplay = second();
  int minDisplay = minute();
  int hourDisplay = hour();
  char currentTimeStr[8];

  iterationCount = (iterationCount + 1) % 20;
  // Blink the cursor for 5 iterations out of 20, so a 1/4 per unit of time (15 frames out of 60 if it ran fast enough to be 60fps)
  if (iterationCount >= 15) {
    if (cursorPosition == HOUR) {
      sprintf(currentTimeStr, "%c%c:%02d:%02d", char(1), char(1), minDisplay, secDisplay);
    } else if (cursorPosition == MINUTE) {
      sprintf(currentTimeStr, "%02d:%c%c:%02d", hourDisplay, char(1), char(1), secDisplay);
    } else if (cursorPosition == SECOND) {
      sprintf(currentTimeStr, "%02d:%02d:%c%c", hourDisplay, minDisplay, char(1), char(1));
    }
  }
  else {
    //  Format currentTimeStr to print hour, minute and second with leading zeros (so every section is 2 characters)
    sprintf(currentTimeStr, "%02d:%02d:%02d", hourDisplay, minDisplay, secDisplay);
  }



  // Print the time to the middle of the first row of the screen
  lcd.setCursor(4, 0);
  lcd.print(currentTimeStr);

  lcd.setCursor(0, 0);
  lcd.print("Set ");
  lcd.setBacklight(0x6);
}

void printAlarmTime() {
  char alarmTimeStr[8];

  iterationCount = (iterationCount + 1) % 20;
  // Blink the cursor for 5 iterations out of 20, so a 1/4 per unit of time (15 frames out of 60 if it ran fast enough to be 60fps)
  if (iterationCount > 14) {
    if (cursorPosition == HOUR) {
      sprintf(alarmTimeStr, "%c%c:%02d ", char(1), char(1), alarmTime.minutes);
    } else if (cursorPosition == MINUTE) {
      sprintf(alarmTimeStr, "%02d:%c%c ", alarmTime.hours, char(1), char(1));
    }
  }
  else {
    sprintf(alarmTimeStr, "%02d:%02d ", alarmTime.hours, alarmTime.minutes);
  }

  lcd.setCursor(4, 0);
  lcd.print(alarmTimeStr);
  lcd.setCursor(10, 0);

  if (alarmSet) {
    if ((iterationCount > 14) && (cursorPosition == SECOND)) {
      lcd.write(1);
      lcd.write(1);
    } else {
      lcd.print("ON ");
    }
    lcd.setBacklight(0x2); // Set backlight to green to show alarm is on
  } else {
    if ((iterationCount > 14) && (cursorPosition == SECOND)) {
      lcd.write(1);
      lcd.write(1);
      lcd.write(1);
    } else {
      lcd.print("OFF");
    }
    lcd.setBacklight(0x1); //Set backlight to red to show alarm is off
  }
}

void addTime() {
  switch (displayMode) {
    case UPDATETIME:
      if (cursorPosition == HOUR) {
        adjustTime(3600);
      } else if (cursorPosition == MINUTE) {
        adjustTime(60);
      } else {
        adjustTime(1);
      }
      break;
    case ALARMTIME:
      if (cursorPosition == HOUR) {
        // Mod the number with 24 to loop the number around (values of hour can be anything between 0-23)
        alarmTime.hours = (alarmTime.hours + 1) % 24;
      } else if (cursorPosition == MINUTE) {
        // Mod the number with 60 to loop the number around (values of minute can be anything between 0-23)
        alarmTime.minutes = (alarmTime.minutes + 1) % 60;
      } else {
        // toggle if the alarm is set or not
        alarmSet = !alarmSet;
      }
  }
}

void minusTime() {
  switch (displayMode) {
    case UPDATETIME:
      if (cursorPosition == HOUR) {
        adjustTime(-3600);
      } else if (cursorPosition == MINUTE) {
        adjustTime(-60);
      } else {
        adjustTime(-1);
      }
      break;
    case ALARMTIME:
      if (cursorPosition == HOUR) {
        // Cannot use mod correctly when going into negative numbers, so use ternary operator for immediate-if like statement to keep the code clean
        alarmTime.hours = (alarmTime.hours == 0) ? (23) : (alarmTime.hours - 1);
      } else if (cursorPosition == MINUTE) {
        // Cannot use mod correctly when going into negative numbers, so use ternary operator for immediate-if like statement to keep the code clean
        alarmTime.minutes = (alarmTime.minutes == 0) ? (59) : (alarmTime.minutes - 1);
      } else {
        // toggle if the alarm is set or not
        alarmSet = !alarmSet;
      }
  }
}

String otherStr = "";
String displayText = "";
int start = 0;
int finish = 0;

void scrollingText() {
  otherStr = (String)text;
  start++;
  if (start == otherStr.length() - 16) start = 0;
  finish = start + 16;
  if (finish == otherStr.length() - 16) finish = otherStr.length() - 16;


  displayText = otherStr.substring(start, finish);

  Serial.println(displayText);

  lcd.setCursor(0, 1);
  lcd.print(displayText);



}

int backlightColor = 1;
void soundAlarm() {
  backlightColor = ((backlightColor + 1) % 7) + 1; // Add one because there is no color 0, so backlightColor range will be 1-7 instead of 0-6
  lcd.setBacklight(backlightColor);
  lcd.setCursor(0, 0);
  lcd.print("  ");
  lcd.setCursor(2, 0);
  lcd.print(char(2)); // Print custom up arrow
  lcd.print(": Snooze     ");
  lcd.setCursor(0, 1);
  lcd.print("SEL: Disable    ");
}

void snoozeDisplay() {
  lcd.setBacklight(3);
  lcd.setCursor(0, 0);
  lcd.print("Snoozing");
  lcd.setCursor(0, 1);
  lcd.print("SEL: Stop Alarm");
}





























