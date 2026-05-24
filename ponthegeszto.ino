/*******************************************************************************
 * PROJEKT:   Profi Akkumulátor Ponthegesztő Vezérlés (Spot Welder v1.4.1)
 * FEJLESZTŐ: Rodnas Oref (Dual-Pulse & STABILITY FIX)
 * * VÁLTOZÁSOK (v1.4.1):
 * - I2C kommunikáció túlterhelés elleni védelme (FPS limiter beépítése).
 * - OLED boot késleltetés a "fekete képernyő" probléma elkerülésére.
 * - Volatile int -> volatile byte csere a megszakítások atomi olvasásához.
 * - Draw flag törlési logikájának javítása interrupt race-condition ellen.
 *******************************************************************************/

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <EEPROM.h>

// --- PIN KIOSZTÁS ---
#define WELD_OUT_PIN     3   
#define VOLTAGE_PIN      A1  
#define SENSE_PIN        A0  

#define ENCODER_SW       4   
#define ENCODER_CLK      5
#define ENCODER_DT       6
#define BTN_BACK         8   
#define BTN_CONFIRM      9   

// --- EEPROM CÍMEK ---
#define EEPROM_ADDR_P1   0
#define EEPROM_ADDR_DLY  1
#define EEPROM_ADDR_P2   2

// --- KIJELZŐ INICIALIZÁLÁS ---
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- GLOBÁLIS VÁLTOZÓK (ATOMIC BYTE TÍPUSRA CSERÉLVE) ---
volatile byte pulse1TimeMs = 3;   
volatile byte pulseDelayMs = 15;  
volatile byte pulse2TimeMs = 15;  
volatile byte selectedSetting = 0; 

bool autoWeldEnabled = true; 
volatile bool updateDisplay = true;
unsigned long lastDrawTime = 0;
const unsigned long drawInterval = 40; // Max ~25 FPS képfrissítés (I2C védelem)

// Feszültségmérés konstansai
const float VOLTAGE_CALIBRATION = 0.01529; 
const float MIN_BATTERY_VOLTAGE = 11.0;    
float currentBatteryVoltage = 0.0;
bool isBatteryOk = false; 

unsigned long lastVoltageMeasureTime = 0;
const unsigned long voltageMeasureInterval = 300; 

// Auto-Weld Érzékelési küszöbök
const int TOUCH_THRESHOLD_LOW  = 200;  
const int TOUCH_THRESHOLD_HIGH = 600;

enum WeldState { IDLE, CONTACT_DETECTED, WELDING, COOLDOWN, ERROR_STATE };
WeldState systemState = IDLE;

// Időzítési konstansok (ms)
const unsigned long autoWeldDelay = 800;   
const unsigned long cooldownDelay = 1500;
const unsigned long safetyTimeout  = 3000;  
const unsigned long debounceDelay = 50;    

unsigned long contactStartTime = 0;
unsigned long cooldownStartTime = 0;
unsigned long lastDebounceTime = 0;
volatile unsigned long lastEncoderMoveTime = 0;
volatile bool eepromSavePending = false;

bool lastBackState = HIGH;
bool lastConfirmState = HIGH;
bool lastEncSwState = HIGH;
volatile bool lastClkState = HIGH; 

void triggerWeld();
void preciseDelayMs(int ms);
void drawUI();
void measureVoltage();

void setup() {
  // STABILITÁS: Várakozás az OLED kondenzátorainak feltöltődésére
  delay(250); 
  
  pinMode(WELD_OUT_PIN, OUTPUT);
  digitalWrite(WELD_OUT_PIN, LOW);

  pinMode(SENSE_PIN, INPUT);
  pinMode(VOLTAGE_PIN, INPUT);
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
  pinMode(BTN_CONFIRM, INPUT_PULLUP);

  lastClkState = digitalRead(ENCODER_CLK);
  lastEncSwState = digitalRead(ENCODER_SW);

  // Értékek beolvasása (byte-ként kezelve)
  byte savedP1 = EEPROM.read(EEPROM_ADDR_P1);
  byte savedDly = EEPROM.read(EEPROM_ADDR_DLY);
  byte savedP2 = EEPROM.read(EEPROM_ADDR_P2);
  
  if (savedP1 >= 1 && savedP1 <= 99) pulse1TimeMs = savedP1;
  if (savedDly >= 1 && savedDly <= 99) pulseDelayMs = savedDly;
  if (savedP2 >= 1 && savedP2 <= 99) pulse2TimeMs = savedP2;

  u8g2.begin();
  u8g2.setContrast(255);
  measureVoltage();

  PCICR |= (1 << PCIE2);
  PCMSK2 |= (1 << PCINT21); 
  PCMSK2 |= (1 << PCINT22); 
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. FESZÜLTSÉGMÉRÉS
  if (currentMillis - lastVoltageMeasureTime >= voltageMeasureInterval) {
    measureVoltage();
    lastVoltageMeasureTime = currentMillis;
    updateDisplay = true; 
  }

  // 2. EEPROM MENTÉS
  if (eepromSavePending && (currentMillis - lastEncoderMoveTime >= 3000)) {
    EEPROM.update(EEPROM_ADDR_P1, pulse1TimeMs);
    EEPROM.update(EEPROM_ADDR_DLY, pulseDelayMs);
    EEPROM.update(EEPROM_ADDR_P2, pulse2TimeMs);
    eepromSavePending = false;
  }

  // 3. GOMBOK
  bool readingBack = digitalRead(BTN_BACK);
  bool readingConfirm = digitalRead(BTN_CONFIRM);
  bool readingEncSw = digitalRead(ENCODER_SW);

  if ((currentMillis - lastDebounceTime) > debounceDelay) {
    if (readingBack == LOW && lastBackState == HIGH) {
      autoWeldEnabled = !autoWeldEnabled;
      updateDisplay = true;
      lastDebounceTime = currentMillis;
    }
    
    if (readingConfirm == LOW && lastConfirmState == HIGH) {
      if (!autoWeldEnabled && systemState == IDLE && isBatteryOk) {
        triggerWeld();
        cooldownStartTime = currentMillis;
        systemState = COOLDOWN;
        updateDisplay = true;
      }
      lastDebounceTime = currentMillis;
    }
    
    if (readingEncSw == LOW && lastEncSwState == HIGH) {
      selectedSetting++;
      if (selectedSetting > 2) selectedSetting = 0;
      updateDisplay = true;
      lastDebounceTime = currentMillis;
    }
  }
  
  lastBackState = readingBack;
  lastConfirmState = readingConfirm;
  lastEncSwState = readingEncSw;

  // 4. ÁLLAPOTGÉP
  int sensorValue = analogRead(SENSE_PIN);

  switch (systemState) {
    case IDLE:
      if (isBatteryOk && autoWeldEnabled && (sensorValue < TOUCH_THRESHOLD_LOW)) {
        contactStartTime = currentMillis;
        systemState = CONTACT_DETECTED;
        updateDisplay = true;
      }
      break;

    case CONTACT_DETECTED:
      if (sensorValue > TOUCH_THRESHOLD_HIGH) {
        systemState = IDLE;
        updateDisplay = true;
      } 
      else if (currentMillis - contactStartTime >= autoWeldDelay) {
        if (isBatteryOk) {
          triggerWeld();
          cooldownStartTime = currentMillis;
          systemState = COOLDOWN;
        } else {
          systemState = IDLE;
        }
        updateDisplay = true;
      }
      else if (currentMillis - contactStartTime >= safetyTimeout) {
        systemState = ERROR_STATE;
        updateDisplay = true;
      }
      break;

    case COOLDOWN:
      // Folyamatos animációhoz a flag beállítása, de az FPS korlát megfogja
      updateDisplay = true; 
      if (currentMillis - cooldownStartTime >= cooldownDelay) {
        if (sensorValue > TOUCH_THRESHOLD_HIGH) {
          systemState = IDLE;
          updateDisplay = true;
        }
      }
      break;

    case ERROR_STATE:
      if (sensorValue > TOUCH_THRESHOLD_HIGH) {
        systemState = IDLE;
        updateDisplay = true;
      }
      break;
  }

  // 5. KIJELZŐ FRISSÍTÉS - FPS KORLÁTOZÁSSAL (Stabilitás javítás)
  if (updateDisplay && (currentMillis - lastDrawTime >= drawInterval)) {
    lastDrawTime = currentMillis;
    updateDisplay = false; // Flag törlése a ciklus ELŐTT
    
    u8g2.firstPage();
    do {
      drawUI();
    } while ( u8g2.nextPage() );
  }
}

// --- FÜGGVÉNYEK ---

void measureVoltage() {
  long sum = 0;
  for(int i=0; i<8; i++) {
    sum += analogRead(VOLTAGE_PIN);
    delayMicroseconds(50);
  }
  float calculatedVoltage = (sum / 8.0) * VOLTAGE_CALIBRATION;
  if (calculatedVoltage < 1.5) {
    currentBatteryVoltage = 0.0;
    isBatteryOk = false;
  } else {
    currentBatteryVoltage = calculatedVoltage;
    isBatteryOk = (currentBatteryVoltage >= MIN_BATTERY_VOLTAGE);
  }
}

void preciseDelayMs(int ms) {
  for(int i = 0; i < ms; i++) {
    delayMicroseconds(1000);
  }
}

void triggerWeld() {
  noInterrupts(); 
  
  if (pulse1TimeMs > 0) {
    digitalWrite(WELD_OUT_PIN, HIGH);
    preciseDelayMs(pulse1TimeMs);
    digitalWrite(WELD_OUT_PIN, LOW);
  }

  if (pulseDelayMs > 0) {
    preciseDelayMs(pulseDelayMs);
  }

  if (pulse2TimeMs > 0) {
    digitalWrite(WELD_OUT_PIN, HIGH);
    preciseDelayMs(pulse2TimeMs);
    digitalWrite(WELD_OUT_PIN, LOW);
  }
  
  interrupts(); 
}

void drawUI() {
  u8g2.setFont(u8g2_font_6x10_tr);
  
  u8g2.setCursor(5, 12);
  u8g2.print(F("SPOT WELDER"));
  
  u8g2.setCursor(95, 12);
  u8g2.print(currentBatteryVoltage, 1);
  u8g2.print(F("V"));
  u8g2.drawHLine(0, 16, 128);

  u8g2.setFont(u8g2_font_6x12_tr);
  
  u8g2.setCursor(5, 29);
  if (selectedSetting == 0) u8g2.print(F(">")); else u8g2.print(F(" "));
  u8g2.print(F(" P1 : ")); u8g2.print(pulse1TimeMs); u8g2.print(F(" ms"));
  
  u8g2.setCursor(5, 41);
  if (selectedSetting == 1) u8g2.print(F(">")); else u8g2.print(F(" "));
  u8g2.print(F(" DLY: ")); u8g2.print(pulseDelayMs); u8g2.print(F(" ms"));
  
  u8g2.setCursor(5, 53);
  if (selectedSetting == 2) u8g2.print(F(">")); else u8g2.print(F(" "));
  u8g2.print(F(" P2 : ")); u8g2.print(pulse2TimeMs); u8g2.print(F(" ms"));

  u8g2.drawVLine(75, 20, 32);
  
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setCursor(84, 30);
  u8g2.print(F("MODE:"));
  
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.setCursor(84, 46);
  if (autoWeldEnabled) u8g2.print(F("AUTO"));
  else u8g2.print(F("MANUAL"));

  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(5, 62);
  
  if (!isBatteryOk) {
    if (currentBatteryVoltage < 1.5) u8g2.print(F("ERROR: NO BATTERY"));
    else u8g2.print(F("ERROR: BATTERY LOW!"));
  } 
  else if (systemState == COOLDOWN) {
    unsigned long elapsed = millis() - cooldownStartTime;
    int barWidth = map(elapsed, 0, cooldownDelay, 0, 50);
    if (barWidth > 50) barWidth = 50;
    if (barWidth < 0) barWidth = 0; // Alulcsordulás védelem
    
    u8g2.print(F("HOLD:"));
    u8g2.drawFrame(35, 56, 54, 7);
    u8g2.drawBox(37, 58, barWidth, 3);        
  } 
  else {
    switch (systemState) {
      case IDLE:
        u8g2.print(F("READY / READY TO WELD"));
        break;
      case CONTACT_DETECTED:
        u8g2.print(F("CONTACT! HOLD STILL..."));
        break;
      case ERROR_STATE:
        u8g2.print(F("ERROR: BAD CONTACT!"));
        break;
    }
  }
}

ISR(PCINT2_vect) {
  bool currentClkState = digitalRead(ENCODER_CLK);
  
  if (currentClkState != lastClkState && currentClkState == LOW) {
    bool dirCw = (digitalRead(ENCODER_DT) != currentClkState);
    
    if (selectedSetting == 0) {       
      if (dirCw && pulse1TimeMs < 99) pulse1TimeMs++;
      else if (!dirCw && pulse1TimeMs > 0) pulse1TimeMs--;
    } 
    else if (selectedSetting == 1) {   
      if (dirCw && pulseDelayMs < 99) pulseDelayMs++;
      else if (!dirCw && pulseDelayMs > 0) pulseDelayMs--;
    } 
    else if (selectedSetting == 2) {   
      if (dirCw && pulse2TimeMs < 99) pulse2TimeMs++;
      else if (!dirCw && pulse2TimeMs > 0) pulse2TimeMs--;
    }
    
    updateDisplay = true; 
    lastEncoderMoveTime = millis();
    eepromSavePending = true;
  }
  lastClkState = currentClkState;
}