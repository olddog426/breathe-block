# Breathe Block firmware

A quiet first prototype for the Waveshare ESP32-S3-Touch-AMOLED-1.43 and
Hi-Link HLK-LD6002 radar kit.

The interface deliberately shows no health data during normal use. Its quiet
ambient form follows the radar's live respiratory-phase signal. When a guided
session begins, the form expands for four seconds, releases for six seconds,
repeats five times, then fades to black. The main message stays in a protected
quiet zone at the center while the contour moves around it; phase changes fade
gently rather than switching abruptly. The radar logic describes sustained relative
activation; it does not diagnose stress or provide medical monitoring.

The prompt sequence is deliberately unhurried:

1. **your breathing has shifted** fades in, then out;
2. a short empty pause;
3. **breathe with me** fades in, then out;
4. a second empty pause;
5. the guided inhale/exhale instructions begin.

A slow, low-contrast ring pulses around the outer edge during the first four
steps. It is intended for peripheral vision and stops before breath guidance.
Every opacity and size change uses an eased envelope. The contour carries its
current size into the next stage and blends onto the new motion over 900 ms, so
the experience remains one continuous flow with no visual reset between screens.

## Try the visual first

1. Install Arduino IDE 2.
2. In Boards Manager, install **esp32 by Espressif Systems 3.3.0 or newer**.
3. In Library Manager, install **lvgl 9.2.2**.
4. Open `BreatheBlock/BreatheBlock.ino`.
5. Select **Waveshare ESP32-S3-Touch-AMOLED-1.43** as the board.
6. Connect the Waveshare board by USB-C and press Upload.

The current configuration uses simulated readings and automatically starts
the visual after boot. Press the onboard **BOOT** button to start or stop a
session manually.

Open `visual-preview.html` to compare the interaction before uploading. It can
replay the gentle alert and switch between simulated live-breath following and
the guided 4-in/6-out rhythm.

If the exact Waveshare board name is not visible, update the Espressif board
package before using a generic ESP32-S3 profile. The board requires 16 MB
flash and PSRAM enabled.

## Connect the radar later

After confirming the visual works, open `BreatheBlock/AppConfig.h` and change:

```cpp
constexpr bool kUseSimulatedRadar = false;
constexpr bool kAutoStartVisualDemo = false;
```

Use the small UART connector on the Waveshare board:

| Radar test board | Waveshare display |
|---|---|
| TX0 | RXD / GPIO 44 |
| RX0 | TXD / GPIO 43 |
| GND | GND |

Leave the connector's 3V3 wire disconnected. Power the radar test board and
display separately by USB-C. **Do not rely on wire colors**; use the printed
TX/RX/GND labels and cross TX to RX.

The firmware first listens at the manufacturer's documented 1,382,400 baud
and automatically tries 115,200 once if no valid packets arrive.

## First live-data test

Place the radar 40–100 cm from the center of your chest, aimed directly at
you. Sit still for at least a minute. Open Arduino's Serial Monitor at 115200
baud to see the live readings and baseline while keeping the normal screen
calm and number-free.

The firmware parses the radar's `0x0A13` respiratory-phase stream for the live
ambient motion. If phase data is temporarily unavailable, it quietly falls
back to a small ambient pulse rather than pretending to follow a breath.

The initial detector:

- builds a 90-second seated baseline;
- compares heart and breathing rates with your own baseline;
- requires a sustained change for 75 seconds;
- suppresses prompts for ten minutes after a session;
- never labels the result as anxiety or medical stress.

These thresholds are intentionally conservative and should be tuned only
after comparing the radar with your Withings during real desk use.
