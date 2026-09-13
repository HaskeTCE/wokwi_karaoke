#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Keypad.h>
#include <ESP32Servo.h>

// ==========================================
// PIN DEFINITIONS
// ==========================================
// Shared LCD Data Pins
const int RS = 22, D4 = 5, D5 = 18, D6 = 19, D7 = 21;

// Independent LCD Enable Pins
const int EN_OP = 23;
const int EN_BOOTH = 0;

LiquidCrystal lcdOp(RS, EN_OP, D4, D5, D6, D7);
LiquidCrystal lcdBooth(RS, EN_BOOTH, D4, D5, D6, D7);

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

// Buzzer variables
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
void updateBoothIdleDisplay(const char* line1, const char* line2);

// ==========================================
// SETUP
// ==========================================
void setup() {
  Serial.begin(115200);

  // Initialize both LCDs
  lcdOp.begin(16, 2);
  lcdBooth.begin(16, 2);
  
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
    return; // Halt logic processing when switch is off
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
    lcdOp.clear();
    lcdBooth.clear();
    lcdOp.noDisplay();
    lcdBooth.noDisplay();
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    digitalWrite(BUZZER, LOW);
    lockServo.write(0); 
  } 
  else if (isPowerOn && currentState == POWER_OFF) {
    lcdOp.display();
    lcdBooth.display();
    digitalWrite(RED_LED, HIGH);
    lockServo.write(0);
    resetToInputHours();
  }
}

void handleNumberInput(char key, const char* prompt, SystemState nextState) {
  if (lcdNeedsUpdate) {
    // Update Operator LCD
    lcdOp.clear();
    lcdOp.setCursor(0, 0);
    lcdOp.print(prompt);
    lcdOp.setCursor(0, 1);
    lcdOp.print(inputBuffer);

    // Update Booth LCD (User just sees it's locked)
    updateBoothIdleDisplay("Karaoke Booth", "Status: Locked");
    
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
    // Operator Display
    lcdOp.clear();
    lcdOp.setCursor(0, 0);
    lcdOp.print(String("Preset: ") + presetH + "h " + presetM + "m");
    lcdOp.setCursor(0, 1);
    lcdOp.print("*=Yes, #=Cancel");

    // Booth Display
    updateBoothIdleDisplay("Karaoke Booth", "Status: Locked");

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
    // Operator sees prompt
    lcdOp.clear();
    lcdOp.setCursor(0, 0);
    lcdOp.print("Timer Ready");
    lcdOp.setCursor(0, 1);
    lcdOp.print("Waiting on Booth");

    // Booth sees unlock status and instruction
    lcdBooth.clear();
    lcdBooth.setCursor(0, 0);
    lcdBooth.print("Booth Unlocked");
    lcdBooth.setCursor(0, 1);
    lcdBooth.print("Press button ->");

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
    lcdOp.clear();
    lcdBooth.clear();
    
    lcdOp.setCursor(0, 0);
    lcdOp.print("Time Left:");
    lcdBooth.setCursor(0, 0);
    lcdBooth.print("Time Left:");
    
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
    
    lcdOp.setCursor(0, 1);
    lcdOp.print(timeStr);
    
    lcdBooth.setCursor(0, 1);
    lcdBooth.print(timeStr);
  }

  if (remainingSeconds == 0) {
    triggerTimeUp();
  }
}

void handleTimeUpState(char key) {
  if (lcdNeedsUpdate) {
    lcdOp.clear();
    lcdOp.setCursor(0, 0);
    lcdOp.print("Time has ended!");
    lcdOp.setCursor(0, 1);
    lcdOp.print("Press # to clear");

    lcdBooth.clear();
    lcdBooth.setCursor(0, 0);
    lcdBooth.print("Time has ended!");
    lcdBooth.setCursor(0, 1);
    lcdBooth.print("Please exit.");

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
// HELPER FUNCTIONS & INTERRUPTS
// ==========================================

void updateBoothIdleDisplay(const char* line1, const char* line2) {
  lcdBooth.clear();
  lcdBooth.setCursor(0, 0);
  lcdBooth.print(line1);
  lcdBooth.setCursor(0, 1);
  lcdBooth.print(line2);
}

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