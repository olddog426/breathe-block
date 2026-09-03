# Breathe Block — interface concept

> The screen is not a readout. It is a small body of light that lives on the desk,
> and occasionally asks you to breathe with it.

## 1. Position

Breathe Block is an **ambient object**, not a health tracker. The interface is
judged by how it feels in peripheral vision for 23 hours a day, not by how it
looks in a screenshot. So the design starts from restraint:

* At rest the display is **true black** with a single dim ember. On AMOLED,
  black pixels are off — the device disappears into the desk and draws almost
  nothing.
* There are **no numbers, no graphs, no faces, no icons, no percentages**.
  Nothing on screen can be read as a measurement of you.
* Language is descriptive and reversible, never diagnostic: *your breathing has
  shifted*, never *stress detected*.
* Nothing ever cuts. Every appearance, disappearance and state change is an
  eased cross-fade, so the object never looks like software repainting itself.

## 2. Visual vocabulary

Three elements. Nothing else exists on screen.

| Element | What it is | When it appears |
|---|---|---|
| **The ember** | A soft radial bloom at the centre. The object's presence. | Always, at 1.5–6 % brightness |
| **The contour** | A luminous ring whose radius *is* the breath, filled with a faint veil so it reads as a bubble of light rather than an outline | Guidance only |
| **The horizon** | A dim ring hugging the display edge that blooms inward | The moment of noticing only |

Plus **text**: lowercase, letter-spaced, centred, always cross-faded, never
more than three words at a time.

### Why a ring and not a filled disc

A filled disc is the obvious choice (and everyone else's). A ring keeps the
centre of the screen dark, which does three things at once: it leaves a
protected quiet zone for the word, it makes the light read as volumetric rather
than as a UI shape, and it halves the lit-pixel count on an OLED that we would
like to keep dark.

### Light, not shapes

The whole background is rendered as a **continuous radial light field** —
a sum of soft, compactly-supported bell profiles evaluated per pixel, colour-
graded and ordered-dithered into RGB565. There are no hard edges anywhere,
because there are no widget borders anywhere. This is the single most important
decision in the design: hard-edged LVGL circles read as *an Arduino demo*; a
dithered gradient with a warm core and a cool halo reads as *a lamp*.

Colour is a function of intensity: bright light tends to a warm ivory, faint
light to a pale sage-teal, so the glow appears to scatter the way real light
does. During guidance the whole field is nudged a few percent warmer on the
inhale and cooler on the exhale — subliminal, but it makes the rhythm feel
embodied.

### Protecting the words

Wherever a word is shown, a **text well** attenuates the light field inside a
small central radius. The contour appears to dip behind the word rather than
wash it out. This is why guidance text stays legible even when the ring is at
its smallest.

## 3. Motion

* All easing is `smoothstep`; velocity is zero at both ends of every move.
* **The turn of the breath is still.** The inhale reaches full extension at 88 %
  of its four seconds and the exhale bottoms out at 92 % of its six. Those
  ~0.5 s of stillness at each turn are what make the pacing feel human instead
  of metronomic.
* Every field parameter is exponentially smoothed toward its target
  (geometry τ≈110 ms, brightness τ≈260 ms). A state change can therefore never
  produce a jump — the machine *cannot* snap, by construction rather than by
  care. A regression test asserts this over a whole session.
* Nothing is ever completely still: even at rest the ember breathes at about
  3.7 cycles per minute — deliberately slower than a person, so it reads as the
  object idling, not as the object tracking you.
* The ember drifts a few pixels over minutes (burn-in care, and it makes the
  object feel alive). Drift eases to zero during a session so the ring stays
  concentric with the words.

## 4. State flow

```
        power on
           │
      ┌────▼─────┐
      │ AWAKENING│  2.6 s  spark → outward wave → ember. Happens once.
      └────┬─────┘
           │
      ┌────▼─────┐  no presence 3 min   ┌──────────┐
      │ RESTING  ├─────────────────────▶│ SLEEPING │  ember at 1.5 %
      │  ember   │◀─────────────────────┤          │
      └────┬─────┘      presence        └──────────┘
           │
           │ sustained relative shift (StressEngine)   ── or manual start ──┐
           │                                                                │
      ┌────▼─────┐                                                          │
      │ NOTICING │  5.2 s  horizon blooms at the rim and contracts inward,  │
      │          │         carrying "your breathing has shifted" to centre  │
      └────┬─────┘                                                          │
           │                                                                │
      ┌────▼─────┐                                                          │
      │ INVITING │  3.4 s  horizon hands its radius to the contour,         │◀┘
      │          │         "breathe with me", contour settles to the floor
      └────┬─────┘
           │
      ┌────▼─────┐
      │ GUIDING  │  5 × (4 s in / 6 s out).  Words for the first two cycles,
      │          │  half-lit on the third, then light alone.
      └────┬─────┘  A hairline arc traces the rim: how long is left, no numbers.
           │
      ┌────▼─────┐
      │ RELEASING│  4.6 s  the contour expands and dissolves outward while the
      │          │         ember re-forms.  "settled", then black.
      └────┬─────┘
           │
        RESTING
```

Two details that matter more than they look:

* **A manually started session skips NOTICING.** If nothing was noticed, the
  device does not claim it noticed something — it goes straight to
  *breathe with me*.
* **Any session can be dismissed** (BOOT button, or a touch once the touch
  controller is wired). Dismissal goes to a short 2 s RELEASING with no closing
  word. The device does not comment on being turned down.

### Presence, without saying so

While resting, if the radar is delivering a respiratory-phase signal, the
ember's size follows *your* breath at about ±16 %. No words, no numbers, no
indication that anything is being measured. Most people will never consciously
notice it; it is the difference between an object that is on and an object that
is present.

## 5. Verbal design

| Moment | Words |
|---|---|
| Noticing | `your breathing has shifted` |
| Inviting | `breathe with me` |
| Guiding (cycles 1–2, half on 3) | `inhale` / `exhale` |
| Releasing | `settled` |
| Dismissed | *(nothing)* |

Everything is lowercase and letter-spaced. All five strings live in one block of
`AppConfig.h` so the product voice can be retuned without touching the interface.

## 6. Rendering architecture

The interesting constraint is that a 466×466 round AMOLED on a QSPI bus costs
roughly 13–23 ms to repaint in full, which is most of a 30 fps frame. So:

* **The light field is composited into LVGL's flush buffer**, not into an
  offscreen canvas. LVGL renders only the text and the progress arc onto a true
  black screen; the flush callback adds the glow on top. This removes a 434 KB
  canvas and all of its PSRAM traffic.
* Because the field is a pure function of `(x, y, snapshot)`, *any* area LVGL
  chooses to repaint comes out correct. There is no possibility of a stale
  background.
* The field is radially symmetric, so a frame is a **1-D lookup table of 341
  radii × 16 ordered-dither phases** (≈11 KB, internal SRAM), and the per-pixel
  work is one table read and one store, with the radius maintained by integer
  addition — no square roots, no multiplies, no PSRAM reads in the inner loop.
* Each row is clipped to the span where the glow actually exists, so cost is
  proportional to the lit disc, not to the flushed rectangle.
* The UI invalidates `union(previous glow box, current glow box)` each frame, so
  a sleeping device repaints an 80×80 square and a session repaints ~440×440.
  When the field is unchanged to within a quantisation step, nothing is
  invalidated at all.

`BreathField` (renderer) and `BreathScene` (state machine and easing) are pure
C++ with no LVGL and no Arduino dependency, which is what lets both of them be
unit-tested and rendered to PNG on a laptop.

## 7. Testing without the radar

Three independent levels, because the hardware and the radar arrive at
different times:

1. **`tools/preview`** — builds with `g++` on any machine and renders the real
   field code to PNG contact sheets. Design review with no hardware at all.
2. **`visual-preview.html`** — the same state machine and the same field maths
   ported to canvas, with buttons for every state. Design review in a browser.
3. **Demo mode on the device** — `DemoMode::Tour` walks the entire state flow on
   a loop with a simulated radar; `DemoMode::Manual` steps scenes from the BOOT
   button; a single-character serial console jumps to any state, forces a
   simulated activation, and prints frame timings.
