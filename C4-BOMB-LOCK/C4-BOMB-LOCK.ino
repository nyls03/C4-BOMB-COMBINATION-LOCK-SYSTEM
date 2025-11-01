#include <Keypad.h>
#include <EEPROM.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Relay Configuration ---
const bool RELAY_ACTIVE_LOW = true;

// --- Keypad Setup ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {2,3,4,5};
byte colPins[COLS] = {6,7,8,9};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Relay and Buzzer Pins ---
const int relayGreen = 11;  // CH2 - Green (Disarmed)
const int relayRed   = 12;  // CH1 - Red (Countdown/Explosion)
const int buzzerPin  = 10;

// --- Password Setup ---
const int passwordLength = 4;
char password[passwordLength + 1];
char input[passwordLength + 1];
int inputIndex = 0;

// --- Countdown Setup ---
const unsigned long countdownTotalMs = 10000UL;
const unsigned long beepIntervalMs   = 1000UL;
unsigned long countdownStart = 0;
unsigned long lastBeepTime   = 0;
bool disarmed = false;
bool exploded = false;
bool inPasswordChange = false;

// --- Relay Control ---
void relayOn(int pin) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? LOW : HIGH);
}

void relayOff(int pin) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? HIGH : LOW);
}

void setup() {
  pinMode(relayGreen, OUTPUT);
  pinMode(relayRed, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  relayOff(relayGreen);
  relayOff(relayRed);

  lcd.init();
  lcd.backlight();
  Serial.begin(9600);

  loadPassword();

  lcd.clear();
  lcd.print("SYSTEM ARMED");
  delay(1200);

  lcd.clear();
  lcd.print("Time Left: 10s");
  lcd.setCursor(0, 1);
  lcd.print("Enter PIN:");

  countdownStart = millis();
  lastBeepTime = countdownStart - beepIntervalMs;
}

void loop() {
  handleKeypadInput();
  if (!inPasswordChange) handleCountdown();
}

void handleKeypadInput() {
  char key = keypad.getKey();
  if (!key) return;

  if (key == '*') {
    inputIndex = 0;
    lcd.setCursor(10, 1);
    lcd.print("     ");
    lcd.setCursor(10, 1);
    return;
  }

  if (key == 'A') {
    changePassword();
    return;
  }

  if (key >= '0' && key <= '9') {
    if (inputIndex < passwordLength) {
      input[inputIndex++] = key;
      lcd.setCursor(10, 1);
      for (int i = 0; i < inputIndex; i++) lcd.print('*');
      for (int i = inputIndex; i < passwordLength; i++) lcd.print(' ');
    }

    if (inputIndex == passwordLength) {
      input[inputIndex] = '\0';
      if (strcmp(input, password) == 0) {
        disarmSystem();
      } else {
        explodeNow();
      }
    }
  }
}

void handleCountdown() {
  if (exploded || disarmed) return;

  unsigned long now = millis();

  if ((now - lastBeepTime >= beepIntervalMs) && (now - countdownStart < countdownTotalMs)) {
    relayOn(relayRed);
    tone(buzzerPin, 1500);
    delay(150);
    noTone(buzzerPin);
    relayOff(relayRed);
    lastBeepTime = now;
  }

  long remainingMs = countdownTotalMs - (now - countdownStart);
  int remainingSec = (remainingMs > 0) ? ((remainingMs + 999) / 1000) : 0;

  // Update only the top line
  lcd.setCursor(0, 0);
  lcd.print("Time Left: ");
  lcd.print(remainingSec);
  lcd.print("s  ");

  // Reprint the bottom line (PIN entry)
  lcd.setCursor(0, 1);
  lcd.print("Enter PIN:");
  lcd.setCursor(10, 1);
  for (int i = 0; i < inputIndex; i++) lcd.print('*');
  for (int i = inputIndex; i < passwordLength; i++) lcd.print(' ');

  if ((now - countdownStart >= countdownTotalMs) && !disarmed) {
    explodeNow();
  }
}

void disarmSystem() {
  disarmed = true;
  lcd.clear();
  lcd.print("DISARMED!");
  relayOn(relayGreen);
  successTone();
  relayOff(relayRed);
  delay(2000);
  resetSystem();
}

void explodeNow() {
  exploded = true;
  lcd.clear();
  lcd.print("!!! EXPLODED !!!");
  relayOn(relayRed);
  relayOff(relayGreen);
  tone(buzzerPin, 400, 4000);
  delay(4000);
  noTone(buzzerPin);
  relayOff(relayRed);
  delay(2000);
  resetSystem();
}

void resetSystem() {
  exploded = false;
  disarmed = false;
  inputIndex = 0;
  relayOff(relayGreen);
  relayOff(relayRed);
  lcd.clear();
  lcd.print("SYSTEM RESET");
  delay(1500);
  lcd.clear();
  lcd.print("Time Left: 10s");
  lcd.setCursor(0, 1);
  lcd.print("Enter PIN:");
  countdownStart = millis();
  lastBeepTime = countdownStart - beepIntervalMs;
}

void successTone() {
  tone(buzzerPin, 2000, 300); delay(400);
  tone(buzzerPin, 2500, 300); delay(400);
  noTone(buzzerPin);
  delay(500);
}

void changePassword() {
  inPasswordChange = true;
  lcd.clear();
  lcd.print("Enter old PIN:");
  lcd.setCursor(0, 1);
  inputIndex = 0;

  while (inputIndex < passwordLength) {
    char oldKey = keypad.getKey();
    if (oldKey >= '0' && oldKey <= '9') {
      input[inputIndex++] = oldKey;
      lcd.print('*');
    }
  }
  input[inputIndex] = '\0';

  if (strcmp(input, password) == 0) {
    lcd.clear();
    lcd.print("New PIN:");
    lcd.setCursor(0, 1);
    inputIndex = 0;
    while (inputIndex < passwordLength) {
      char newKey = keypad.getKey();
      if (newKey >= '0' && newKey <= '9') {
        input[inputIndex++] = newKey;
        lcd.print('*');
      }
    }
    input[inputIndex] = '\0';
    savePassword(input);
    strcpy(password, input);
    lcd.clear();
    lcd.print("PIN UPDATED!");
    delay(1500);
  } else {
    lcd.clear();
    lcd.print("Wrong Old PIN!");
    delay(1500);
  }

  lcd.clear();
  lcd.print("Time Left: 10s");
  lcd.setCursor(0, 1);
  lcd.print("Enter PIN:");
  inPasswordChange = false;
  countdownStart = millis();
  lastBeepTime = countdownStart - beepIntervalMs;
  inputIndex = 0;
}

void savePassword(const char *newPass) {
  for (int i = 0; i < passwordLength; i++) EEPROM.write(i, newPass[i]);
}

void loadPassword() {
  bool valid = true;
  for (int i = 0; i < passwordLength; i++) {
    char c = EEPROM.read(i);
    if (c < '0' || c > '9') { valid = false; break; }
    password[i] = c;
  }
  if (!valid) strcpy(password, "1234");
  password[passwordLength] = '\0';
}
