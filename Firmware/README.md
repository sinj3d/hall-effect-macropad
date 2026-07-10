# Macropad Firmware

Firmware for the 6-key + rotary-encoder Hall-effect macropad, running on a SparkFun Pro Micro (ATmega32U4). Built with PlatformIO on the Arduino framework, using [HID-Project](https://github.com/NicoHood/HID) for NKRO keyboard and media-key (consumer control) output.

## Hardware

| Signal | Pro Micro pin | MCU port | Notes |
|---|---|---|---|
| SS49E sensor 0–3 (WASD) | A3, A2, A1, A0 | PF4–PF7 | Linear analog output, idles near VCC/2 |
| A3144 switch 0 | 15 | PB1 | Open collector, LOW when magnet present |
| A3144 switch 1 | 14 | PB3 | Open collector, LOW when magnet present |
| Encoder A | 2 | PD1 / INT1 | Interrupt-driven quadrature |
| Encoder B | 3 | PD0 / INT0 | Interrupt-driven quadrature |
| Encoder push switch | 4 | PD4 | Active LOW |

The SS49Es must be on A0–A3: PB1/PB3 have no ADC, so those two positions can only host the digital A3144s.

### How analog sensing works

The SS49E idles near VCC/2 and its output swings **up or down** as the magnet approaches, depending on which pole faces the sensor. The firmware captures a per-key baseline at power-up and treats a key as pressed when the reading deviates from that baseline by `ACTUATION_DELTA` counts (released when it falls back below `RELEASE_DELTA`). This works with either magnet orientation and needs no per-key polarity configuration.

Consequence: **don't hold keys down while plugging the board in.** If you did, send `z` over serial to re-capture baselines. Slow drift (temperature, magnet creep) is compensated automatically while keys are idle.

## Build & flash

```bash
pip install platformio        # once
pio run                       # compile (from the repo root)
pio run -t upload             # compile + flash, port auto-detected
pio device monitor            # serial console, 115200 baud
```

If `pio` isn't recognized after installing (pip's Scripts directory not on PATH), use `python -m platformio` in place of `pio` — e.g. `python -m platformio run -t upload`.

If the upload times out, double-tap the reset button on the Pro Micro (this enters the Caterina bootloader for ~8 seconds) and run the upload command again.

## Serial commands (115200 baud)

| Key | Action |
|---|---|
| `v` | Print one status line: filtered value, baseline, and delta/actuation per analog key, digital key states, raw encoder pin levels |
| `p` | Toggle 10 Hz streaming of that status line (use while tuning) |
| `z` | Re-capture analog baselines (release all keys first) |
| `c` | Guided per-key actuation calibration (results saved to EEPROM) |
| `d` | Clear saved calibration, back to the `config.h` defaults |
| `h` | Help |

## Calibrating actuation points (per key)

Each analog key has its own actuation and release point, calibrated in place:

1. Run `pio device monitor` and send `c`.
2. Keep all keys released while it re-zeros, then press and **hold** each WASD key fully, one at a time. Peak travel per key is echoed once a second.
3. After 15 s (or send any character to finish early), each key's actuation point is set to 65% of its measured travel and its release point to 35%, the results are printed, and everything is saved to EEPROM — calibration survives power cycles and re-flashing.

Keys that moved less than `CAL_MIN_TRAVEL` (30 counts) during the window keep their previous thresholds, so you can re-calibrate a single key without touching the others. The 65/35 split is set by `CAL_ACTUATION_PCT` / `CAL_RELEASE_PCT` in `config.h`; the gap between them is the hysteresis that debounces the analog keys — keep it wide to avoid chatter.

Until calibration is run (or after `d` clears it), all keys use the `ACTUATION_DELTA` / `RELEASE_DELTA` defaults from `config.h` (60/35 counts); a close-range neodymium magnet typically swings 150–300 counts at full press.

## Verifying the physical layout

The schematic doesn't say which physical key sits on which pin, so verify once:

- **Key order**: stream with `p`, press each key, and note which channel (`A0`–`A3`, `D0`/`D1`) responds. Reorder `ANALOG_KEY_MAP` / `DIGITAL_KEY_MAP` in `config.h` to match.
- **Encoder pins**: the firmware assumes A=D2, B=D3, switch=D4. Watching the `enc A= B= SW=` fields while rotating slowly and pressing shows which pins actually toggle; adjust `PIN_ENC_*` in `config.h` if they're crossed.
- **Encoder direction**: if volume goes the wrong way, set `ENCODER_REVERSED` to `1`.

## Changing the keymap

Edit `config.h`:

```cpp
static const KeyboardKeycode ANALOG_KEY_MAP[NUM_ANALOG_KEYS] = {KEY_W, KEY_A, KEY_S, KEY_D};
static const KeyboardKeycode DIGITAL_KEY_MAP[NUM_DIGITAL_KEYS] = {KEY_LEFT_SHIFT, KEY_SPACE};

static const ConsumerKeycode ENC_CW_ACTION = MEDIA_VOLUME_UP;
static const ConsumerKeycode ENC_CCW_ACTION = MEDIA_VOLUME_DOWN;
static const ConsumerKeycode ENC_PRESS_ACTION = MEDIA_PLAY_PAUSE;
```

Key names are HID-Project's `KeyboardKeycode` (`KEY_A`–`KEY_Z`, `KEY_F1`–`KEY_F24`, `KEY_LEFT_CTRL`, `KEY_ESC`, ...) and `ConsumerKeycode` (`MEDIA_VOLUME_UP`, `MEDIA_VOL_MUTE`, `MEDIA_NEXT`, `CONSUMER_BROWSER_HOME`, ...). Full lists are in HID-Project's `ImprovedKeylayouts.h` and `ConsumerAPI.h`.

## Troubleshooting

- **An A3144 key never triggers**: the A3144 is unipolar — it only responds to the magnet's **south pole**. Flip the magnet in that switch stem.
- **An analog key is stuck down or dead**: its baseline is probably wrong (key was held at power-up, or the magnet moved). Send `z` with all keys released. If deltas barely move on press, the magnet may be too far from the sensor at the bottom of travel.
- **Encoder skips or double-steps**: try `ENC_STEPS_PER_DETENT = 2` (some encoders are half-step per detent).
- **Upload fails**: double-tap reset, then upload within ~8 seconds. Check `pio device list` shows the board.
- **Two keys are dead**: they're likely the PB1/PB3 positions with SS49Es installed there by mistake — those pins can't do analog. Swap the sensors so the A3144s sit on PB1/PB3.
