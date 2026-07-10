#pragma once
/*
 * Macropad configuration — pin map, keymap, tuning.
 *
 * Pin assignments follow Hardware/Macropad Schematic.pdf. Normally only the
 * keymaps and the tuning section should need editing. Key names come from
 * HID-Project: KeyboardKeycode (KEY_W, KEY_F13, KEY_LEFT_SHIFT, ...) and
 * ConsumerKeycode (MEDIA_VOLUME_UP, MEDIA_PLAY_PAUSE, MEDIA_VOL_MUTE, ...).
 */

#include <HID-Project.h>

// ---- 4x SS49E linear Hall sensors (analog) — the WASD cluster ----
// PB1/PB3 have no ADC on the ATmega32U4, so the SS49Es must be the four
// sensors wired to A3..A0. Reorder ANALOG_KEY_MAP after checking which
// physical key moves which channel (serial command 'p').
#define NUM_ANALOG_KEYS 4
static const uint8_t ANALOG_KEY_PINS[NUM_ANALOG_KEYS] = {A3, A2, A1, A0};  // PF4 PF5 PF6 PF7
static const KeyboardKeycode ANALOG_KEY_MAP[NUM_ANALOG_KEYS] = {KEY_W, KEY_A, KEY_S, KEY_D};

// ---- 2x A3144 Hall switches (digital, open collector, active LOW) ----
#define NUM_DIGITAL_KEYS 2
static const uint8_t DIGITAL_KEY_PINS[NUM_DIGITAL_KEYS] = {15, 14};  // PB1, PB3
static const KeyboardKeycode DIGITAL_KEY_MAP[NUM_DIGITAL_KEYS] = {KEY_LEFT_SHIFT, KEY_SPACE};

// ---- Rotary encoder ----
static const uint8_t PIN_ENC_A = 2;   // PD1 / INT1
static const uint8_t PIN_ENC_B = 3;   // PD0 / INT0
static const uint8_t PIN_ENC_SW = 4;  // PD4
#define ENCODER_REVERSED 0            // set to 1 if rotation goes the wrong way
static const uint8_t ENC_STEPS_PER_DETENT = 4;  // 4 for most EC11s; 2 for half-step types

static const ConsumerKeycode ENC_CW_ACTION = MEDIA_VOLUME_UP;
static const ConsumerKeycode ENC_CCW_ACTION = MEDIA_VOLUME_DOWN;
static const ConsumerKeycode ENC_PRESS_ACTION = MEDIA_PLAY_PAUSE;

// ---- Tuning ----
// SS49E deltas are in ADC counts (0-1023 over 0-5 V, roughly 0.3 counts/gauss).
// These are the defaults used until per-key calibration is run (serial 'c');
// calibrated per-key thresholds live in EEPROM and override them.
static const uint16_t ACTUATION_DELTA = 60;
static const uint16_t RELEASE_DELTA = 35;

// ---- Per-key calibration (serial command 'c') ----
// Calibration measures each key's full-press delta and derives its actuation
// and release points from it. The gap between the two is hysteresis.
static const uint8_t CAL_ACTUATION_PCT = 65;  // actuation point, % of full travel
static const uint8_t CAL_RELEASE_PCT = 35;    // release point, % of full travel
static const uint16_t CAL_MIN_TRAVEL = 30;    // counts; keys moving less keep old values
static const uint16_t CAL_WINDOW_MS = 15000;  // capture window length

static const uint8_t ADC_SAMPLES = 4;            // averaged per reading
static const uint8_t DEBOUNCE_MS = 8;            // A3144 keys + encoder button
static const uint16_t BASELINE_TRACK_MS = 1000;  // idle drift compensation interval
static const uint32_t SERIAL_BAUD = 115200;
