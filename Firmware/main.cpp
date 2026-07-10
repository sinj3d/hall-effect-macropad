/*
 * Hall-effect macropad firmware
 * Board: SparkFun Pro Micro (ATmega32U4, 5V/16MHz)
 *
 * Hardware, per Hardware/Macropad Schematic.pdf:
 *   - 4x SS49E linear Hall sensors (analog) on A3/A2/A1/A0 — WASD cluster
 *   - 2x A3144 Hall switches (open collector, active LOW) on D15/D14
 *   - Rotary encoder: A/B on D2/D3 (external interrupts), push switch on D4
 *
 * SS49E keys: the sensor idles near VCC/2 and swings up or down as the magnet
 * approaches, depending on which pole faces it. Each key is compared against a
 * baseline captured at power-up; it counts as pressed once the reading deviates
 * from that baseline by ACTUATION_DELTA and releases below RELEASE_DELTA, so
 * either magnet orientation works. Don't hold keys while plugging the board in
 * (send 'z' over serial to re-zero if you did).
 *
 * Actuation points are per key: serial command 'c' measures each key's full
 * travel and stores actuation/release thresholds in EEPROM ('d' clears them).
 */

#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// ---------- SS49E (analog) keys ----------

struct AnalogKey {
  uint16_t filtered;
  uint16_t baseline;
  bool pressed;
};

static AnalogKey analogKeys[NUM_ANALOG_KEYS];

static uint16_t readAveraged(uint8_t pin, uint8_t samples) {
  uint16_t sum = 0;
  for (uint8_t i = 0; i < samples; i++) sum += analogRead(pin);
  return sum / samples;
}

static void captureBaselines() {
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    analogKeys[i].baseline = readAveraged(ANALOG_KEY_PINS[i], 32);
    analogKeys[i].filtered = analogKeys[i].baseline;
  }
}

static uint16_t deltaFromBaseline(const AnalogKey &k) {
  return k.filtered > k.baseline ? k.filtered - k.baseline : k.baseline - k.filtered;
}

// ---------- Per-key actuation thresholds (EEPROM-backed) ----------

struct Calibration {
  uint8_t magic;
  uint16_t actuation[NUM_ANALOG_KEYS];
  uint16_t release[NUM_ANALOG_KEYS];
};

static const uint8_t CAL_MAGIC = 0xC4;
static Calibration cal;

static void loadDefaultCal() {
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    cal.actuation[i] = ACTUATION_DELTA;
    cal.release[i] = RELEASE_DELTA;
  }
}

static void loadCalibration() {
  Calibration stored;
  EEPROM.get(0, stored);
  if (stored.magic == CAL_MAGIC) cal = stored;
  else loadDefaultCal();
}

static void saveCalibration() {
  cal.magic = CAL_MAGIC;
  EEPROM.put(0, cal);  // update semantics: only changed bytes get written
}

static void scanAnalogKeys() {
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    AnalogKey &k = analogKeys[i];
    uint16_t raw = readAveraged(ANALOG_KEY_PINS[i], ADC_SAMPLES);
    k.filtered = (k.filtered * 3 + raw + 2) / 4;

    uint16_t delta = deltaFromBaseline(k);
    if (!k.pressed && delta >= cal.actuation[i]) {
      k.pressed = true;
      NKROKeyboard.press(ANALOG_KEY_MAP[i]);
    } else if (k.pressed && delta <= cal.release[i]) {
      k.pressed = false;
      NKROKeyboard.release(ANALOG_KEY_MAP[i]);
    }
  }
}

// Nudge idle baselines one count toward the current reading so slow drift
// (temperature, magnet creep) doesn't eat into the actuation margin.
static void trackBaselineDrift() {
  static uint32_t lastMs = 0;
  uint32_t now = millis();
  if (now - lastMs < BASELINE_TRACK_MS) return;
  lastMs = now;
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    AnalogKey &k = analogKeys[i];
    if (k.pressed || deltaFromBaseline(k) > cal.release[i]) continue;
    if (k.filtered > k.baseline) k.baseline++;
    else if (k.filtered < k.baseline) k.baseline--;
  }
}

// ---------- A3144 (digital) keys + encoder button ----------

struct DebouncedInput {
  bool state;
  bool lastReading;
  uint32_t lastEdgeMs;
};

static DebouncedInput digitalKeys[NUM_DIGITAL_KEYS];
static DebouncedInput encButton;

// Returns true when the debounced state just changed.
static bool debounce(DebouncedInput &d, bool reading, uint32_t now) {
  if (reading != d.lastReading) {
    d.lastReading = reading;
    d.lastEdgeMs = now;
  }
  if (reading != d.state && now - d.lastEdgeMs >= DEBOUNCE_MS) {
    d.state = reading;
    return true;
  }
  return false;
}

static void scanDigitalKeys() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < NUM_DIGITAL_KEYS; i++) {
    bool active = digitalRead(DIGITAL_KEY_PINS[i]) == LOW;  // sinks LOW near magnet
    if (debounce(digitalKeys[i], active, now)) {
      if (digitalKeys[i].state) NKROKeyboard.press(DIGITAL_KEY_MAP[i]);
      else NKROKeyboard.release(DIGITAL_KEY_MAP[i]);
    }
  }

  bool swActive = digitalRead(PIN_ENC_SW) == LOW;
  if (debounce(encButton, swActive, now) && encButton.state) {
    Consumer.write(ENC_PRESS_ACTION);
  }
}

// ---------- Rotary encoder (quadrature, interrupt-driven) ----------

static volatile int8_t encDetents = 0;
static uint8_t encPrevBits = 0;

static void encoderIsr() {
  static const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  static int8_t accum = 0;
  uint8_t bits = (uint8_t)((digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B));
  accum += QDEC[((encPrevBits << 2) | bits) & 0x0F];
  encPrevBits = bits;
  if (accum >= (int8_t)ENC_STEPS_PER_DETENT) {
    encDetents++;
    accum = 0;
  } else if (accum <= -(int8_t)ENC_STEPS_PER_DETENT) {
    encDetents--;
    accum = 0;
  }
}

static void pumpEncoder() {
  noInterrupts();
  int8_t d = encDetents;
  encDetents = 0;
  interrupts();
  if (d == 0) return;
#if ENCODER_REVERSED
  d = -d;
#endif
  while (d > 0) { Consumer.write(ENC_CW_ACTION); d--; }
  while (d < 0) { Consumer.write(ENC_CCW_ACTION); d++; }
}

// ---------- Serial tuning interface ----------

static bool streaming = false;

static void printStatus() {
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    const AnalogKey &k = analogKeys[i];
    Serial.print(F("A"));
    Serial.print(i);
    Serial.print(F(":val="));
    Serial.print(k.filtered);
    Serial.print(F(" base="));
    Serial.print(k.baseline);
    Serial.print(F(" d="));
    Serial.print(deltaFromBaseline(k));
    Serial.print(F("/"));
    Serial.print(cal.actuation[i]);
    Serial.print(k.pressed ? F("*  ") : F("   "));
  }
  Serial.print(F("| D0="));
  Serial.print(digitalKeys[0].state ? F("DOWN") : F("up"));
  Serial.print(F(" D1="));
  Serial.print(digitalKeys[1].state ? F("DOWN") : F("up"));
  Serial.print(F(" | enc A="));
  Serial.print(digitalRead(PIN_ENC_A));
  Serial.print(F(" B="));
  Serial.print(digitalRead(PIN_ENC_B));
  Serial.print(F(" SW="));
  Serial.println(digitalRead(PIN_ENC_SW));
}

// Guided per-key calibration: re-zero, record each key's peak travel while the
// user holds it down, then derive per-key actuation/release points from it.
// Blocks the scan loop on purpose; no key events are sent while it runs.
static void runCalibration() {
  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) analogKeys[i].pressed = false;
  NKROKeyboard.releaseAll();

  Serial.println(F("Calibration: release all keys, re-zeroing..."));
  delay(500);
  captureBaselines();

  Serial.println(F("Now press and HOLD each analog key fully, one at a time."));
  Serial.print(F("Capturing for "));
  Serial.print(CAL_WINDOW_MS / 1000);
  Serial.println(F(" s -- send any key to finish early."));

  uint16_t peak[NUM_ANALOG_KEYS] = {0};
  uint32_t start = millis();
  uint32_t lastEcho = start;
  while (millis() - start < CAL_WINDOW_MS && !Serial.available()) {
    for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
      uint16_t raw = readAveraged(ANALOG_KEY_PINS[i], ADC_SAMPLES);
      uint16_t b = analogKeys[i].baseline;
      uint16_t delta = raw > b ? raw - b : b - raw;
      if (delta > peak[i]) peak[i] = delta;
    }
    if (millis() - lastEcho >= 1000) {
      lastEcho = millis();
      Serial.print(F("  peak travel:"));
      for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
        Serial.print(' ');
        Serial.print(peak[i]);
      }
      Serial.println();
    }
  }
  while (Serial.available()) Serial.read();

  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) {
    Serial.print(F("A"));
    Serial.print(i);
    if (peak[i] < CAL_MIN_TRAVEL) {
      Serial.print(F(": travel "));
      Serial.print(peak[i]);
      Serial.println(F(" too small -- keeping previous thresholds"));
      continue;
    }
    cal.actuation[i] = (uint16_t)(((uint32_t)peak[i] * CAL_ACTUATION_PCT) / 100);
    cal.release[i] = (uint16_t)(((uint32_t)peak[i] * CAL_RELEASE_PCT) / 100);
    Serial.print(F(": travel="));
    Serial.print(peak[i]);
    Serial.print(F(" act="));
    Serial.print(cal.actuation[i]);
    Serial.print(F(" rel="));
    Serial.println(cal.release[i]);
  }
  saveCalibration();
  Serial.println(F("Saved to EEPROM."));

  noInterrupts();
  encDetents = 0;  // discard rotation accumulated while blocked here
  interrupts();
}

static void printHelp() {
  Serial.println(F("Hall-effect macropad -- serial commands:"));
  Serial.println(F("  v  print one status line (values, baselines, delta/actuation, pin states)"));
  Serial.println(F("  p  toggle 10 Hz status streaming (for threshold tuning)"));
  Serial.println(F("  z  re-capture analog baselines (release all keys first)"));
  Serial.println(F("  c  calibrate per-key actuation points (guided, saved to EEPROM)"));
  Serial.println(F("  d  clear saved calibration, back to config.h defaults"));
  Serial.println(F("  h  this help"));
}

static void handleSerial() {
  while (Serial.available()) {
    switch (Serial.read()) {
      case 'v':
        printStatus();
        break;
      case 'p':
        streaming = !streaming;
        break;
      case 'z':
        captureBaselines();
        Serial.println(F("Baselines re-zeroed."));
        break;
      case 'c':
        runCalibration();
        break;
      case 'd':
        EEPROM.write(0, 0xFF);  // invalidate the stored block
        loadDefaultCal();
        Serial.println(F("Calibration cleared; using config.h defaults."));
        break;
      case 'h':
      case '?':
        printHelp();
        break;
      default:
        break;
    }
  }
  if (streaming) {
    static uint32_t lastMs = 0;
    if (millis() - lastMs >= 100) {
      lastMs = millis();
      printStatus();
    }
  }
}

// ---------- Arduino entry points ----------

void setup() {
  Serial.begin(SERIAL_BAUD);

  for (uint8_t i = 0; i < NUM_ANALOG_KEYS; i++) pinMode(ANALOG_KEY_PINS[i], INPUT);
  for (uint8_t i = 0; i < NUM_DIGITAL_KEYS; i++) pinMode(DIGITAL_KEY_PINS[i], INPUT_PULLUP);
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  delay(50);  // let the sensors power up before zeroing
  captureBaselines();
  loadCalibration();

  encPrevBits = (uint8_t)((digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B));
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), encoderIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), encoderIsr, CHANGE);

  NKROKeyboard.begin();
  Consumer.begin();
}

void loop() {
  scanAnalogKeys();
  scanDigitalKeys();
  trackBaselineDrift();
  pumpEncoder();
  handleSerial();
}
