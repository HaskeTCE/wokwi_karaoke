#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <ESP32Servo.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
// Operator LCD I2C Pins
const int SDA_OP = 21;
const int SCL_OP = 22;

// Booth LCD I2C Pins
const int SDA_BOOTH = 18;
const int SCL_BOOTH = 19;

// We only need ONE LCD object for both screens, data routed dynamically
LiquidCrystal_I2C lcd(0x27, 16, 2); 

// Keypad Pins
const uint8_t ROWS = 4;
const uint8_t COLS = 4;
char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};
uint8_t rowPins[ROWS] = { 13, 12, 14, 27 };
uint8_t colPins[COLS] = { 26, 25, 33, 32 };
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Other Components
const int SERVO_PIN = 15;
const int RED_LED = 2;
const int GREEN_LED = 4;
const int BUZZER = 16;
const int BOOTH_BTN = 17;
const int MAIN_SWITCH = 34; 

Servo lockServo; 

// ==========================================
// SYSTEM STATES & VARIABLES
// ==========================================
enum SystemState {
  POWER_OFF,
  INPUT_HOURS,
  INPUT_MINUTES,
  CONFIRM_PRESET,
  READY_TO_START,
  RUNNING,
  TIME_UP
};
SystemState currentState = POWER_OFF;

bool lcdNeedsUpdate = true;
String inputBuffer = "0";

int targetHours = 0;
int targetMinutes = 0;
int presetH = 0;
int presetM = 0;

unsigned long remainingSeconds = 0;
unsigned long lastTickMillis = 0;
unsigned long buzzerCycleStart = 0;

// ==========================================
// FUNCTION PROTOTYPES
// ==========================================
void checkPowerSwitch();
void handleNumberInput(char key, const char* prompt, SystemState nextState);
void triggerPreset(char key);
void handlePresetConfirmation(char key);
void prepareBooth();
void handleReadyState(char key);
void handleRunningState();
void handleTimeUpState(char key);
void resetToInputHours();
void triggerTimeUp();
void keypadEvent(KeypadEvent key);
void printToOperator(String line1, String line2);
void printToBooth(String line1, String line2);
void setDisplaysPower(bool powerOn);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize Operator LCD
  Wire.begin(SDA_OP, SCL_OP);
  lcd.init();
  lcd.backlight();
  Wire.end();
  
  // Initialize Booth LCD
  Wire.begin(SDA_BOOTH, SCL_BOOTH);
  lcd.init();
  lcd.backlight();
  
  // Initialize Components
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(BOOTH_BTN, INPUT_PULLUP);
  pinMode(MAIN_SWITCH, INPUT); 

  lockServo.attach(SERVO_PIN);
  lockServo.write(0); // Locked state

  // Keypad Hold Timer - 3 seconds for ending timer prematurely
  keypad.setHoldTime(3000);
  keypad.addEventListener(keypadEvent);

  checkPowerSwitch();
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  checkPowerSwitch();

  if (currentState == POWER_OFF) {
    return; // Avoid logic processing when switch is off
  }

  char key = keypad.getKey(); 
  
  switch (currentState) {
    case INPUT_HOURS:
      handleNumberInput(key, "Enter Hours:", INPUT_MINUTES);
      break;
    case INPUT_MINUTES:
      handleNumberInput(key, "Enter Minutes:", READY_TO_START);
      break;
    case CONFIRM_PRESET:
      handlePresetConfirmation(key);
      break;
    case READY_TO_START:
      handleReadyState(key);
      break;
    case RUNNING:
      handleRunningState();
      break;
    case TIME_UP:
      handleTimeUpState(key);
      break;
    default:
      break;
  }
}

// ==========================================
// LOGIC HANDLERS
// ==========================================

void checkPowerSwitch() {
  bool isPowerOn = (digitalRead(MAIN_SWITCH) == HIGH);
  
  if (!isPowerOn && currentState != POWER_OFF) {
    currentState = POWER_OFF;
    setDisplaysPower(false);
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER, LOW);
    lockServo.write(0); 
  } 
  else if (isPowerOn && currentState == POWER_OFF) {
    setDisplaysPower(true);
    digitalWrite(RED_LED, HIGH);
    lockServo.write(0);
    resetToInputHours();
  }
}

void handleNumberInput(char key, const char* prompt, SystemState nextState) {
  if (lcdNeedsUpdate) {
    printToOperator(prompt, inputBuffer);
    printToBooth("Karaoke Booth", "Status: Locked");
    lcdNeedsUpdate = false;
  }

  if (key) {
    if (key >= '0' && key <= '9') {
      if (inputBuffer == "0") inputBuffer = ""; 
      if (inputBuffer.length() < 2) {
        inputBuffer += key;
        lcdNeedsUpdate = true;
      }
    } 
    else if (key == '#') {
      inputBuffer = "0"; 
      lcdNeedsUpdate = true;
    } 
    else if (key == '*') {
      if (currentState == INPUT_HOURS) {
        targetHours = inputBuffer.toInt();
        inputBuffer = "0";
      } else {
        targetMinutes = inputBuffer.toInt();
        prepareBooth();
      }
      currentState = nextState;
      lcdNeedsUpdate = true;
    }
    else if (key >= 'A' && key <= 'D') {
      triggerPreset(key);
    }
  }
}

void triggerPreset(char key) {
  if (key == 'A') { presetH = 0; presetM = 10; }
  else if (key == 'B') { presetH = 0; presetM = 30; }
  else if (key == 'C') { presetH = 1; presetM = 0; }
  else if (key == 'D') { presetH = 2; presetM = 0; }
  
  currentState = CONFIRM_PRESET;
  lcdNeedsUpdate = true;
}

void handlePresetConfirmation(char key) {
  if (lcdNeedsUpdate) {
    String presetTime = String(presetH) + "h " + String(presetM) + "m";
    printToOperator("Preset: " + presetTime, "*=Yes, #=Cancel");
    printToBooth("Karaoke Booth", "Status: Locked");
    lcdNeedsUpdate = false;
  }

  if (key == '*') {
    targetHours = presetH;
    targetMinutes = presetM;
    prepareBooth();
    currentState = READY_TO_START;
    lcdNeedsUpdate = true;
  } else if (key == '#') {
    resetToInputHours();
  }
}

void prepareBooth() {
  lockServo.write(90); 
  digitalWrite(RED_LED, HIGH); 
  digitalWrite(GREEN_LED, LOW);
  remainingSeconds = (targetHours * 3600) + (targetMinutes * 60);
}

void handleReadyState(char key) {
  if (lcdNeedsUpdate) {
    printToOperator("Timer Ready", "Waiting on Booth");
    printToBooth("Booth Unlocked", "Press button ->");
    lcdNeedsUpdate = false;
  }

  if (digitalRead(BOOTH_BTN) == LOW) {
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, HIGH); 
    currentState = RUNNING;
    lastTickMillis = millis();
    lcdNeedsUpdate = true;
  }
  
  if (key == '#') {
    lockServo.write(0);
    resetToInputHours();
  }
}

void handleRunningState() {
  if (lcdNeedsUpdate) {
    printToOperator("Time Left:", "Starting...");
    printToBooth("Time Left:", "Starting...");
    lcdNeedsUpdate = false;
  }

  if (millis() - lastTickMillis >= 1000) {
    lastTickMillis += 1000;
    if (remainingSeconds > 0) {
      remainingSeconds--;
    }
    
    int h = remainingSeconds / 3600;
    int m = (remainingSeconds % 3600) / 60;
    int s = remainingSeconds % 60;
    
    char timeStr[9];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", h, m, s);
    
    // Dynamically update both displays
    printToOperator("Time Left:", String(timeStr));
    printToBooth("Time Left:", String(timeStr));
  }

  if (remainingSeconds == 0) {
    triggerTimeUp();
  }
}

void handleTimeUpState(char key) {
  if (lcdNeedsUpdate) {
    printToOperator("Time has ended!", "Press # to clear");
    printToBooth("Time has ended!", "Please exit.");
    lcdNeedsUpdate = false;
  }

  unsigned long currentMillis = millis();
  unsigned long cycleTime = currentMillis - buzzerCycleStart;
  
  if (cycleTime >= 10000) {
    buzzerCycleStart = currentMillis; 
    cycleTime = 0;
  }
  
  if (cycleTime < 5000) {
    if ((cycleTime / 500) % 2 == 0) {
      digitalWrite(BUZZER, HIGH);
    } else {
      digitalWrite(BUZZER, LOW);
    }
  } else {
    digitalWrite(BUZZER, LOW); 
  }

  if (key == '#') {
    digitalWrite(BUZZER, LOW);
    resetToInputHours();
  }
}

// ==========================================
// I2C BUS MULTIPLEXING & LCD HELPERS
// ==========================================
void switchToOperator() {
  Wire.end();
  // Forcefully disconnect Booth pins from I2C
  pinMode(SDA_BOOTH, INPUT);
  pinMode(SCL_BOOTH, INPUT);
  // Attach Operator pins
  Wire.begin(SDA_OP, SCL_OP);
  delay(10); // Quick stabilization 
}

void switchToBooth() {
  Wire.end();
  // Forcefully disconnect Operator pins from I2C
  pinMode(SDA_OP, INPUT);
  pinMode(SCL_OP, INPUT);
  // Attach Booth pins
  Wire.begin(SDA_BOOTH, SCL_BOOTH);
  delay(10); 
}

void printToOperator(String line1, String line2) {
  switchToOperator();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

void printToBooth(String line1, String line2) {
  switchToBooth();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(line1);
  lcd.setCursor(0, 1); lcd.print(line2);
}

void setDisplaysPower(bool powerOn) {
  // Update Operator Screen
  switchToOperator();
  if (powerOn) { lcd.backlight(); } 
  else { lcd.clear(); lcd.noBacklight(); }

  // Update Booth Screen
  switchToBooth();
  if (powerOn) { lcd.backlight(); } 
  else { lcd.clear(); lcd.noBacklight(); }
}

// ==========================================
// HELPER FUNCTIONS & INTERRUPTS
// ==========================================

void resetToInputHours() {
  currentState = INPUT_HOURS;
  inputBuffer = "0";
  digitalWrite(RED_LED, HIGH);
  digitalWrite(GREEN_LED, LOW);
  lcdNeedsUpdate = true;
}

void triggerTimeUp() {
  currentState = TIME_UP;
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);
  lockServo.write(0); 
  buzzerCycleStart = millis(); 
  lcdNeedsUpdate = true;
}

void keypadEvent(KeypadEvent key) {
  switch (keypad.getState()) {
    case HOLD:
      if (key == '#' && currentState == RUNNING) {
        remainingSeconds = 0; 
        triggerTimeUp();
      }
      break;
    default:
      break;
  }
}
