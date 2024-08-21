#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DS3232RTC.h>
#include <time.h>
#include <SD.h>
#include <SPI.h>

// Initialize the LCD with the I2C address (typically 0x27 or 0x3F)
LiquidCrystal_I2C lcd(0x3F, 16, 2); // Change the address if needed

File myFile;
const int button1 = 3;  
const int led = 5;
const int button2 = 2;
const int button3 = 4;  // New button for resetting to the start of the file
int initialstate;
bool fileOpen = false;
bool isReadingFile = false;
bool isPaused = false;
const unsigned long displayDelay = 2000; // 2 seconds delay between lines
const unsigned long scrollDelay = 3000; // 3 seconds delay for scrolling
DS3232RTC myRTC;
const int PinCS = 10;

void setup() {
  Serial.begin(115200);

  // SD card module initialization
  pinMode(PinCS, OUTPUT);
  if (!SD.begin(PinCS)) {
    Serial.println("SD card initialization failed!");
    while (1); // Stop execution if SD card fails
  }
  Serial.println("SD card initialized.");

  // Pin settings
  pinMode(button1, INPUT_PULLUP);
  pinMode(led, OUTPUT);
  pinMode(button2, INPUT_PULLUP);
  pinMode(button3, INPUT_PULLUP);  // Initialize new button

  // LCD Setup
  lcd.init();       // Initialize the LCD
  lcd.backlight();  // Turn on the backlight
  lcd.clear();      // Clear the display initially

  // RTC module initialization
  myRTC.begin();
  setSyncProvider(myRTC.get);   // Set the RTC as the time provider
  
  initialstate = digitalRead(button1); // Initialize initialstate here
}

String getCurrentTime() {
  return String(hour()) + ':' +
         (minute() < 10 ? "0" : "") + String(minute()) + ':' +
         (second() < 10 ? "0" : "") + String(second());
}

String getCurrentDate() {
  return String(day()) + '/' +
         month() + '/' +
         year();
}

void openFileForReading() {
  if (!fileOpen) {
    myFile = SD.open("Log.txt");
    if (!myFile) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Error opening file");
      return;
    }
    fileOpen = true;
  }
}

void closeFileIfOpen() {
  if (fileOpen) {
    myFile.close();
    fileOpen = false;
  }
}

void displayNextLine() {
  if (!fileOpen) {
    openFileForReading();
  }

  lcd.clear();

  if (myFile.available()) {
    String line1 = "";
    String line2 = "";
    String currentLine = "";

    while (myFile.available()) {
      char c = myFile.read();
      if (c == '\n' || c == '\r') {
        if (currentLine.length() > 0) {
          currentLine.trim(); // Remove leading/trailing whitespace
          if (currentLine.length() > 16) {
            // Split the line into two
            line1 = currentLine.substring(0, 16); // First 16 characters
            line2 = currentLine.substring(16); // Remaining characters
          } else {
            line1 = currentLine;
          }
          break; // Stop reading after processing one line
        }
      } else {
        currentLine += c;
      }
    }

    if (currentLine.length() > 0) {
      currentLine.trim();
      if (currentLine.length() > 16) {
        line1 = currentLine.substring(0, 16);
        line2 = currentLine.substring(16);
      } else {
        line1 = currentLine;
      }
    }

    // Display the lines
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);

    Serial.println(line1);
    Serial.println(line2);
    delay(displayDelay); // Display delay for full content
  } else {
    // End of file reached
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("End of file");
    lcd.setCursor(0, 1);
    lcd.print("Resetting...");
    delay(2000); // Display the reset message for 2 seconds

    closeFileIfOpen(); // Close the current file
    isReadingFile = false; // Stop reading file
  }
}

void writeToFile(String logEntry) {
  File writeFile = SD.open("Log.txt", FILE_WRITE);
  if (!writeFile) {
    Serial.println("Error opening file for writing");
    return;
  }
  
  writeFile.println(logEntry);
  writeFile.close(); // Close the file after writing
  
  // Print the log entry to the Serial Monitor
  Serial.println("Log entry saved: " + logEntry);
}

void displayClock() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(getCurrentTime()); // Display time on the first line
  lcd.setCursor(0, 1);
  lcd.print(getCurrentDate()); // Display date on the second line
}

void loop() {
  int state = digitalRead(button1);

  // When switch is turned from On to Off
  if (state == LOW && initialstate == HIGH) {
    String logEntry = getCurrentTime() + " " + getCurrentDate() + ", on";
    writeToFile(logEntry);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Switch ON"); // Display ON status on the LCD
    digitalWrite(led, HIGH);
    delay(5000); // Display the status for 5 seconds
  }

  // When switch is turned from Off to On
  if (state == HIGH && initialstate == LOW) {
    digitalWrite(led, LOW);
    String logEntry = getCurrentTime() + " " + getCurrentDate() + ", off";
    writeToFile(logEntry);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Switch OFF"); // Display OFF status on the LCD
    delay(5000); // Display the status for 5 seconds
  }

  initialstate = state;

  // Button 2 logic for pause/resume
  static int lastButtonState2 = HIGH;
  int buttonState2 = digitalRead(button2);

  if (buttonState2 == LOW && lastButtonState2 == HIGH) {
    // Button 2 was pressed
    if (isReadingFile) {
      isPaused = !isPaused; // Toggle pause/resume
    } else {
      openFileForReading(); // Open file if not already opened
      isReadingFile = true; // Start scrolling
    }
    delay(200); // Debounce delay
  }

  lastButtonState2 = buttonState2;

  // Button 3 logic for resetting to the start of the file
  static int lastButtonState3 = HIGH;
  int buttonState3 = digitalRead(button3);

  if (buttonState3 == LOW && lastButtonState3 == HIGH) {
    // Button 3 was pressed
    closeFileIfOpen(); // Close the file if open
    openFileForReading(); // Reopen the file to reset to the start
    isPaused = true; // Pause immediately after resetting
    isReadingFile = false; // Stop reading file
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("File Reset");
    delay(2000); // Display reset message for 2 seconds
    delay(200); // Debounce delay
  }

  lastButtonState3 = buttonState3;

  if (isReadingFile) {
    if (!isPaused) {
      displayNextLine(); // Continuously display the file content
    }
  } else {
    displayClock();  // Display the clock when not scrolling
    delay(1000); // Adjust delay as needed
  }
}
