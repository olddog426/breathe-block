# Breathe Block — iOS companion app, phase 1

A standalone SwiftUI app: the same breathing exercise as the device, a local
session log, and a 14-day history chart. No Bluetooth, no calendar, no
HealthKit yet — those are phases 2 and 3 (see the bottom of this file).
Everything here lives only on the phone.

The interesting part isn't UI, it's that the ember on screen is rendered by
the *exact same* `BreathField.cpp`/`BreathScene.cpp` the device and
`visual-preview.html` use — referenced from `../BreatheBlock/`, not
reimplemented a third time — through a small Objective-C++ bridge
(`Sources/BreatheBlock/Engine/BreathEngine.h`/`.mm`). If the app's ember ever
looks different from the device's, that's a bug in the bridge, not a design
drift, because there is only one copy of the actual field/scene math.

All of the Swift/Obj-C++ source is already written, under
`ios/BreatheBlock/Sources/BreatheBlock/`. What's missing is the Xcode
project shell itself — this file walks through creating one *without*
colliding with those files, since Xcode's own "New Project" template
generates files with the same names this project already uses.

This has not been compiled or run yet — there's no Xcode/Swift toolchain in
the environment these files were written in. Expect a round of real
debugging together the first time you build, the same way the firmware's
Arduino bring-up went.

## The quick way (recommended)

A `project.yml` at `ios/BreatheBlock/project.yml` describes the whole Xcode
project — bridging header, C++ dialect, every file — so a tool called
[XcodeGen](https://github.com/yonaskolb/XcodeGen) can build the `.xcodeproj`
for you instead of clicking through "New Project" and adding files by hand.

1. Get the code onto your Mac — the simplest way is GitHub's web UI:
   open the repo, pick the `claude/ios-companion-app` branch from the branch
   dropdown, **Code → Download ZIP**, then unzip it.
2. Install XcodeGen once (Terminal): `brew install xcodegen`
   (no Homebrew? [brew.sh](https://brew.sh) has a one-line installer.)
3. Generate the project (Terminal):
   ```sh
   cd breathe-block-ios-companion-app/ios/BreatheBlock
   xcodegen generate
   ```
   (adjust the folder name to whatever the unzip actually produced.)
4. Double-click the new `BreatheBlock.xcodeproj` — it opens already wired up.
5. Click the project name in the navigator → the **BreatheBlock** target →
   **Signing & Capabilities** → set **Team** to your own Apple ID (add it
   first under Xcode → Settings → Accounts if it's not listed).
6. Plug in your iPhone, pick it from the device menu next to the Run button
   (top toolbar), and click **Run**.
7. First launch will refuse to open — on the phone, **Settings → General →
   VPN & Device Management**, tap your Apple ID under Developer App, **Trust**.

This wasn't testable without a Mac, so treat step 4 as the first real
checkpoint: if `BreatheBlock.xcodeproj` won't open or `xcodegen generate`
errors, tell me exactly what it says and we'll fix `project.yml` together —
that's a much smaller fix than debugging the manual path below.

If you'd rather not install anything, or XcodeGen gives you trouble, the
manual walkthrough below builds the identical project by hand.

## The manual way

## 1. Create the project shell

1. Xcode → File → New → Project → **iOS → App**.
2. Product Name: `BreatheBlock`. Interface: **SwiftUI**. Language: **Swift**.
   **Storage: None** (not SwiftData/Core Data here — we're adding our own
   `@Model` file by hand, and letting Xcode template one too would collide).
3. Save it at `ios/` in this repo — i.e. so the project ends up at
   `ios/BreatheBlock/BreatheBlock.xcodeproj`, alongside the `Sources/`
   folder that's already there. Xcode will offer to create a git repo; leave
   that unchecked, this repo already exists.

Xcode will have generated `ios/BreatheBlock/BreatheBlock/ContentView.swift`,
`BreatheBlockApp.swift`, and `Assets.xcassets`, in a `BreatheBlock/` folder
that sits next to (not inside) the `Sources/` folder already in this repo.

## 2. Delete Xcode's generated Swift files

In the Project Navigator, delete these two (Move to Trash, not just
"Remove Reference" — we don't want the template versions at all):

- `BreatheBlock/ContentView.swift`
- `BreatheBlock/BreatheBlockApp.swift`

Leave `Assets.xcassets`, `Preview Content`, and the project's `Info` /
build-settings files alone.

## 3. Add the app's own source

Right-click the `BreatheBlock` group in the Project Navigator → **Add Files
to "BreatheBlock"...**, select the `Sources/BreatheBlock` folder (the one
already sitting at `ios/BreatheBlock/Sources/BreatheBlock/`), and:

- **Uncheck** "Copy items if needed" — these files should stay where they
  are, referenced in place, not duplicated into the Xcode project folder.
- Choose **"Create groups"** (not "Create folder references").
- Make sure the `BreatheBlock` target's checkbox is ticked.

This adds `BreatheBlockApp.swift`, `ContentView.swift`, `Models/`
(`BreathSession.swift`, `BreathingPreferences.swift`), `Views/`
(`BreathingController.swift`, `BreathingView.swift`, `JournalView.swift`,
`InsightsView.swift`, `SettingsView.swift`), `Engine/`, and the bridging
header, replacing the ones you deleted in step 2.

## 4. Add the shared C++ source (referenced, not copied)

Same dialog, a second time: **Add Files to "BreatheBlock"...**, navigate up
to the repo's `BreatheBlock/` folder (the firmware source, at the repo
root — sibling of `ios/`), and select these four files:

- `BreathField.h`
- `BreathField.cpp`
- `BreathScene.h`
- `BreathScene.cpp`

Again **uncheck "Copy items if needed"** — this is the whole point: the app
builds against the device's actual source, so a firmware change and an app
change can never quietly drift apart. Tick the `BreatheBlock` target.

If you'd rather see them nested under the `Engine` group instead of at the
project's top level, drag them there in the navigator afterward — a purely
cosmetic move, it doesn't change where the files live on disk.

## 5. Set the bridging header

Select the **BreatheBlock** project → **BreatheBlock** target → **Build
Settings** → search for `Objective-C Bridging Header`, and set it to:

```
Sources/BreatheBlock/BreatheBlock-Bridging-Header.h
```

(Relative to the project file. If Xcode already listed a different default
path here from the template, replace it — don't add a second one.)

## 6. Set the C++ dialect

Same Build Settings search box: **C++ Language Dialect** → **GNU++17**
(`C++ and Objective-C Interoperability` can stay at its default, "C++/
Objective-C++"). `BreathField.cpp`/`BreathScene.cpp` are plain C++17 with no
platform dependency, but the project-wide default dialect on a fresh SwiftUI
template is sometimes older than that.

## 7. Build and run

Pick an iPhone simulator (any recent one — the round display is drawn to fit
whatever frame SwiftUI gives it, it doesn't assume 466×466 pixels on
screen), and Run.

Expect the first build to surface real errors — the Obj-C→Swift naming was
pinned by hand with `NS_SWIFT_NAME` rather than verified by a compiler (see
the comment at the top of `Engine/BreathEngine.h`), and the include paths in
`Engine/BreathEngine.mm` assume the exact nesting depth described above. If
`BreathField.h`/`BreathScene.h` aren't found, it's almost always steps 3–4
having been added in a different way than described (e.g. "Copy items"
left checked, so the files moved and the relative `../../../../../` in
`BreathEngine.mm` no longer lands on the repo's `BreatheBlock/` folder).

## What phase 1 does and doesn't do

The interface is deliberately down to one thing: the blob. No title, no
button, no state label — a tap on the blob itself is the only control,
ticked every display frame through the same `BreathScene`/`BreathField` the
device runs, so it's the same ember, same pacing, same easing.

- **Tap the blob**: resting opens an invitation, a held invitation begins
  guiding, anything else (guiding, releasing) dismisses — the same
  three-way gesture a tap on the device's glass drives (`DESIGN.md` §4).
  A session started this way never claims to have noticed anything, since
  there's no radar here to notice anything with yet.
- **The status LED**, top right, is a passive indicator, not a control — it
  previews where a live signal will eventually come from (paired device /
  Oura / Apple Health) but always reads ambient today, since none of those
  are wired up. See `BreathDataSource`'s doc comment: it's the seam a later
  phase sets, not a feature this one has.
- Every finished or dismissed session is logged locally (SwiftData) with
  start time, end time, and whether it completed — deliberately the
  smallest useful record, matching the device's own restraint. Nothing
  leaves the phone.
- The hamburger menu (top left) opens three screens: **Journal** (a 14-day
  bar chart of minutes breathed and a plain session list), **Insights**
  (a few honest stats computed from your own logged sessions — nothing
  inferred or fabricated; see the note in `InsightsView`), and **Settings**
  (a breathing style, which actually changes the engine's pacing, and an
  objective, which is a stored preference with nothing yet to personalize).

Not in phase 1, by design, per the phased plan:

- **Bluetooth sync with the device.** The ESP32-S3 this runs on supports
  BLE, but the firmware doesn't advertise a service yet — that's new
  firmware work (a GATT service exposing session events / vitals) plus a
  CoreBluetooth central in the app. Until then the device and the app are
  two independent ways to do the same breathing exercise, not synced.
- **Calendar correlation.** Planned via EventKit (on-device, no OAuth)
  once there's more than a handful of logged sessions to correlate against.
- **Oura / Withings / Apple Health.** Planned via HealthKit as the single
  integration point, since Oura and Withings both sync into Apple Health
  already rather than needing their own separate API integrations.
