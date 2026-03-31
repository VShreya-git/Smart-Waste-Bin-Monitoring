#include <LiquidCrystal.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Keypad.h>

// LCD
LiquidCrystal lcd(14, 27, 26, 25, 33, 32);

// Servo
Servo myServo;

// Pins
int servoPin = 13;
int irPin    = 4;
int trigPin  = 5;
int echoPin  = 18;
int gasPin   = 15;

// Keypad
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {23, 22, 21, 19};
byte colPins[COLS] = {16, 17, 2, 12};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// WiFi
const char* ssid     = ".....";
const char* password = ".....";

// Telegram
String botToken   = ".....";
String chatID     = ".....";

// PIN
String enteredPIN = "";
String correctPIN = "2787";
int wrongAttempts = 0;

// States
bool isLocked      = false;
bool fullSent      = false;
bool gasSent       = false;
bool lockDisplayed = false;
bool isGasLock     = false;

// Forward declaration
void handleKeypad(char key);
void checkAndOpenBin();

// Ultrasonic (getting distance)
long getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  long duration = pulseIn(echoPin, HIGH, 30000);
  
  if (duration == 0) return -1;
  
  return duration * 0.0343 / 2;
}

// Telegram 
void sendMessage(String msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + botToken +
               "/sendMessage?chat_id=" + chatID + "&text=" + msg;
  
  url.replace(" ", "%20");
  
  http.begin(url);
  int httpCode = http.GET();
  http.end();
}

// LCD
void drawFullScreen() { //when ultrasonic detects 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Full! Enter PIN:");
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
}

void drawStars() { //for every key we press, it is displayed as * on LCD
  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  for (int i = 0; i < enteredPIN.length(); i++) {
    lcd.print("*");
  }
}

// Bin Opening Functions
void openBin() {
  lcd.clear();
  lcd.print("Bin Open");
  myServo.write(100);
  delay(5000);
  myServo.write(0);
  
  lcd.clear();
  lcd.print("Thank you");
  lcd.setCursor(0, 1);
  lcd.print("for using me :)");
  delay(2000);
  
  lcd.clear();
  lcd.print("Use me,");
  lcd.setCursor(0, 1);
  lcd.print("save Earth");
  
  // Reset all states after opening
  isLocked = false;
  lockDisplayed = false;
  fullSent = false;
  gasSent = false;
  enteredPIN = "";
  wrongAttempts = 0;
}

// setup (setting up pins as input and output)
void setup() {
  Serial.begin(115200);
  
  pinMode(irPin,   INPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(gasPin,  INPUT);
  
  digitalWrite(trigPin, LOW);
  
  myServo.setPeriodHertz(50);
  myServo.attach(servoPin, 500, 2400);
  myServo.write(0);
  
  lcd.begin(16, 2);
  
  lcd.print("Connecting...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
  
  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("Use me,");
    lcd.setCursor(0, 1);
    lcd.print("save Earth");
    Serial.println("\nWiFi connected!");
  } else {
    lcd.print("WiFi Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Running offline");
    Serial.println("\nWiFi connection failed!");
  }
}

// Keypad
void loop() {
  // Always check for keypad input
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    handleKeypad(key);
  }
  
  // Read sensors periodically
  static unsigned long lastSensorRead = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastSensorRead >= 200) {
    lastSensorRead = currentMillis;
    
    long distance = getDistance();
    bool gas = (digitalRead(gasPin) == LOW);
    
    // Debug output
    if (distance != -1) {
      Serial.print("Distance: ");
      Serial.print(distance);
      Serial.println(" cm");
    }
    
    // Check for lock conditions
    bool full = (distance != -1 && distance < 5);  // 5cm threshold for full
    
    // Gas detection takes priority over full bin
    if (gas && !isLocked) {
      isLocked = true;
      isGasLock = true;
      lockDisplayed = false;
      enteredPIN = "";
      
      if (!gasSent) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Warning:");
        lcd.setCursor(0, 1);
        lcd.print("Toxic gases!");
        sendMessage("Dustbin at AB1, 6th floor, VIT Chennai has detected toxic gases");
        gasSent = true;
      }
    }
    else if (full && !isLocked && !gas) {
      isLocked = true;
      isGasLock = false;
      lockDisplayed = false;
      enteredPIN = "";
      
      if (!fullSent) {
        drawFullScreen();
        sendMessage("Dustbin at AB1, 6th floor, VIT Chennai is full");
        fullSent = true;
      }
    }
    
    // If bin is locked due to gas, show warning
    if (isLocked && isGasLock) {
      if (!lockDisplayed) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Warning:");
        lcd.setCursor(0, 1);
        lcd.print("Toxic gases!");
        lockDisplayed = true;
      }
    }
    
    // Display PIN entry screen if locked due to full bin
    if (isLocked && !isGasLock && !lockDisplayed) {
      drawFullScreen();
      lockDisplayed = true;
    }
  }
  
  // Handle IR sensor for automatic opening (only when not locked)
  static unsigned long lastIRRead = 0;
  if (!isLocked && currentMillis - lastIRRead >= 100) {
    lastIRRead = currentMillis;
    
    if (digitalRead(irPin) == LOW) {
      openBin();
      delay(1000);  // Debounce delay
    }
  }
}

// Keypad handler (considering gas and ultrasonic sensor)
void handleKeypad(char key) {
  // Only accept PIN input if bin is locked due to being full
  if (isLocked && !isGasLock) {
    
    // Handle '*' as clear
    if (key == '*') {
      enteredPIN = "";
      drawFullScreen();
      return;
    }
    
    // Handle numeric input (0-9)
    if (key >= '0' && key <= '9') {
      if (enteredPIN.length() < 4) {
        enteredPIN += key;
        drawStars();
        Serial.print("PIN entered: ");
        Serial.println(enteredPIN);
        
        // AUTO-SUBMIT WHEN 4 DIGITS ARE ENTERED
        if (enteredPIN.length() == 4) {
          Serial.println("4 digits entered, checking PIN...");
          delay(100);  // Small delay to show the last star
          
          if (enteredPIN == correctPIN) {
            Serial.println("PIN correct! Opening bin...");
            openBin();
          } else {
            Serial.println("PIN incorrect!");
            wrongAttempts++;
            
            if (wrongAttempts >= 3) {
              sendMessage("Unauthorized attempt to open dustbin at AB1, 6th floor, VIT Chennai");
              wrongAttempts = 0;
            }
            
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Wrong PIN!");
            lcd.setCursor(0, 1);
            lcd.print("Try again...");
            delay(2000);
            
            enteredPIN = "";
            drawFullScreen();
          }
        }
      } else {
        // Already have 4 digits, show message
        lcd.setCursor(0, 1);
        lcd.print("Press * to clear");
        delay(1000);
        drawStars();
      }
    }
  }
  
  // Handle other keypad presses when not in PIN mode
  else if (!isLocked) {
    // Optional: Add functionality for unlocked state
    Serial.println("Bin is unlocked");
  }
  else if (isLocked && isGasLock) {
    // Gas lock - show warning when any key is pressed
    lcd.setCursor(0, 1);
    lcd.print("Gas Hazard!    ");
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print("Toxic gases!   ");
    delay(500);
  }
}
