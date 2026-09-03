# Breathe Block

A small desk object that sits quietly, notices when your breathing has shifted,
and offers to breathe with you for a minute. Waveshare
ESP32-S3-Touch-AMOLED-1.43 (466 × 466 round AMOLED) with a Hi-Link HLK-LD6002
radar. No camera, no microphone, and no medical claims. Nothing is shown on
screen unasked — the one exception is a tap, which checks in with your heart
rate and breath rate for a few seconds before fading back to quiet.

**[DESIGN.md](DESIGN.md) is the interface concept and state flow.** Read that
first; this file is how to run it.

At rest the display is true black with a single dim ember. When the radar sees
a sustained shift relative to your own seated baseline, the ember visibly
grows and brightens, then a glow blooms at the rim and says *your breathing
has shifted*, then *breathe with me* — and waits. A tap begins it: five
four-second inhales and six-second exhales before dissolving back to black.
Left unanswered for 24 seconds, the invitation withdraws quietly instead of
starting on its own. Nothing on screen ever steps or cuts; a regression test
asserts it.

## Seeing it, in order of how little you need

### 1. In a browser, right now

Open `visual-preview.html`. It runs the same light field and the same state
machine as the firmware, with buttons for every state and the three palettes.
`make -C tools check-preview` fails if that port ever drifts from the firmware.

### 2. On a laptop, from the firmware's own code

```sh
make -C tools preview && mkdir -p out && tools/preview out
python3 tools/contact_sheet.py out          # needs pillow
```

`tools/preview` renders `BreathField`/`BreathScene` — the real firmware
sources — to PNG contact sheets of every moment in the flow.

To go further and run the *whole device UI*, including LVGL, Montserrat and the
dirty-rectangle logic:

```sh
git clone --depth 1 -b v9.2.2 https://github.com/lvgl/lvgl
make -C tools host_ui LVGL_DIR=../lvgl     # builds LVGL once, ~2 min
mkdir -p out && tools/host_ui out && python3 tools/contact_sheet.py out
```

The host framebuffer is only ever written by the flush callback, so anything
wrong with invalidation shows up immediately as smearing.

### 3. On the board, with no radar

1. Arduino IDE 2, **esp32 by Espressif 3.3.0+**, **lvgl 9.2.2**.
2. Open `BreatheBlock/BreatheBlock.ino`, select
   **Waveshare ESP32-S3-Touch-AMOLED-1.43**, upload over USB-C.

`AppConfig.h` ships in `DemoMode::Tour`, which loops the whole state flow with
a simulated radar. `DemoMode::Manual` steps one scene per BOOT press instead.
Either way a serial console is on 115200:

```
n  a noticed session          s  sleep       w  wake
m  a session you started      b  hold the simulated body in activation
c  simulate a tap             t  toggle the looping tour
x  dismiss                    p  next palette    f  frame timings    ?  help
r  return to quiet
```

`c` (no touch hardware needed) checks in with heart rate and breath rate; a
second `c` starts a 3-2-1 into the breathing exercise, exactly as a tap on the
glass would.

`f` reports how long the light field takes to composite and how many pixels the
average flush touches — worth checking after any change to the geometry.

The board's FT3168 touch controller is already wired to fixed pins (see
`board_config.h`), so no extra wiring is needed to try a real tap — but its
register map (`TouchSensor.cpp`) is ported from the common FocalTech
FT5x06-family layout and hasn't been checked against the FT3168's own
datasheet, so expect it to need a round of on-device tuning.

## Connecting the radar

Set `kUseSimulatedRadar = false` and `kDemoMode = DemoMode::Off` in
`AppConfig.h`, then wire the UART connector:

| Radar test board | Waveshare display |
|---|---|
| TX0 | RXD / GPIO 44 |
| RX0 | TXD / GPIO 43 |
| GND | GND |

Leave the connector's 3V3 wire disconnected and power both boards separately by
USB-C. Use the printed TX/RX/GND labels, not the wire colours, and cross TX to
RX. The firmware listens at the documented 1,382,400 baud and falls back to
115,200 once if no valid packets arrive.

Place the radar 40–100 cm from the centre of your chest and sit still for a
minute or two. The detector builds a 90-second seated baseline, compares heart
and breathing rates against *your own* baseline, requires a sustained change for
75 seconds, and then stays quiet for ten minutes. Those thresholds are
deliberately conservative and are worth tuning against a known-good reference
during real desk use. It describes relative activation; it does not diagnose
anything.

## Tuning the object

Everything you are likely to want to change is in `BreatheBlock/AppConfig.h`:

* **Voice** — the five strings, together in one block.
* **Palette** — `kIvory` (warm centre, sage halo), `kMoonlight`, `kEmber`.
* **Pacing** — inhale, exhale, cycle count.
* **`kShowSessionProgress`** — the hairline arc that traces the rim during
  guidance. Set it false for the purest version of the object.

Geometry (how large the contour grows, how soft its edges are, how bright the
resting ember is) lives in `SceneConfig` in `BreathScene.h`.

## Layout

| | |
|---|---|
| `BreathField.{h,cpp}` | the light field and its RGB565 renderer — no LVGL, no Arduino |
| `BreathScene.{h,cpp}` | the state machine and all of the easing — likewise |
| `BreathingUI.{h,cpp}` | LVGL: text, the progress hairline, dirty rectangles |
| `DemoDirector.{h,cpp}` | the tour, the button, the serial console |
| `TouchSensor.{h,cpp}` | polls the FT3168 over I²C and turns presses into a tap |
| `RadarSensor`, `LD6002`, `StressEngine` | radar, its packet parser, and the noticing logic |
| `amoled.*`, `low_level_amoled.*`, `board_config.h` | Waveshare display driver (MIT, see `THIRD_PARTY_NOTICES.md`) |

The first two have no dependencies on LVGL or Arduino, which is what lets the
interface be unit-tested and rendered to images on a laptop.

## Tests

```sh
make -C tests check
```

Beyond the radar-parser and detector tests, these assert the interface's own
design rules: that the scanline renderer's integer radius matches an honest
square root across the whole display, that nothing is drawn outside the bounding
box the UI invalidates, that light adds to LVGL's output and saturates instead
of wrapping, that no frame changes abruptly and no state change is visible as a
step, that a word is only ever swapped while invisible, that the guidance words
fade out after the third cycle, and that a session you turn down gets no
closing remark.

## A note on colour order

`lv_conf.h` sets `LV_COLOR_16_SWAP`, which makes LVGL byte-swap its render
buffer on the way into the flush callback. `BreathingUI` mirrors LVGL's own
`#if` so the light field is packed the same way. If you ever change that
setting, the glow follows automatically.
