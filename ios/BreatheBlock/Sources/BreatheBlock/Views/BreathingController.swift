import SwiftUI
import Combine

/// Drives the shared BreathEngine once per display frame and hands the
/// rendered image to BreathingView. State that other SwiftUI views read
/// (stateName, progress) is deliberately only ever written from inside
/// `tick()`, and only via a deferred dispatch — mutating @Published
/// properties synchronously from a view's own render pass is what causes
/// SwiftUI's "modifying state during view update" warnings, so the write
/// is pushed to the next run loop turn instead, same trick real-time
/// SwiftUI Canvas apps commonly use.
@MainActor
final class BreathingController: ObservableObject {
    @Published private(set) var stateName: String = "awakening"
    @Published private(set) var progress: Double = 0

    /// Fires once, whenever a session finishes releasing and the object is
    /// back at rest — BreathEngine reports this the same way whether the
    /// session ran to completion or was cut short by `dismiss()`; the
    /// `completed` flag is what tells those apart (tracked here, not by the
    /// engine — see `dismiss()`). The natural moment to log it; see
    /// ContentView.
    var onSessionFinished: ((Date, Bool) -> Void)?

    private let engine = BreathEngine()
    private static let fieldSize = 466
    private var pixelBuffer = [UInt8](repeating: 0, count: fieldSize * fieldSize * 4)
    private let colorSpace = CGColorSpaceCreateDeviceRGB()

    private var sessionStartedAt: Date?
    private var sessionWasDismissed = false

    func start() {
        sessionStartedAt = Date()
        sessionWasDismissed = false
        engine.startSession(nowMs: Self.nowMs())
    }

    /// Answers a held invitation and begins guiding. Only meaningful while
    /// `stateName == "inviting"` — see the doc comment on
    /// `BreathEngine.tap(nowMs:)` for why callers must gate on that rather
    /// than call it unconditionally.
    func beginInvitedSession() {
        engine.tap(nowMs: Self.nowMs())
    }

    /// Cuts the current session short. Logging happens later, in `tick()`,
    /// once the engine reports the releasing animation has actually
    /// finished — not here — since a dismiss still takes ~2s to release.
    func dismiss() {
        sessionWasDismissed = true
        engine.dismiss(nowMs: Self.nowMs())
    }

    var sessionActive: Bool { engine.sessionActive }

    /// Call once per frame from BreathingView. Returns the frame's image,
    /// or nil if the buffer couldn't be wrapped (should not happen).
    func tick() -> CGImage? {
        let now = Self.nowMs()
        engine.update(nowMs: now, presence: true)

        let newState = engine.stateName
        let newProgress = Double(engine.progress)
        if newState != stateName || abs(newProgress - progress) > 0.002 {
            DispatchQueue.main.async { [weak self] in
                self?.stateName = newState
                self?.progress = newProgress
            }
        }

        if engine.consumeSessionFinished(), let startedAt = sessionStartedAt {
            let completed = !sessionWasDismissed
            sessionStartedAt = nil
            let callback = onSessionFinished
            DispatchQueue.main.async { callback?(startedAt, completed) }
        }

        pixelBuffer.withUnsafeMutableBufferPointer { buffer in
            guard let base = buffer.baseAddress else { return }
            engine.renderRGBA(into: base,
                              width: Self.fieldSize,
                              height: Self.fieldSize,
                              palette: BreathPaletteIvory)
        }
        return makeImage()
    }

    private func makeImage() -> CGImage? {
        let bitmapInfo = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipLast.rawValue)
        guard let provider = CGDataProvider(data: Data(pixelBuffer) as CFData) else { return nil }
        return CGImage(width: Self.fieldSize,
                       height: Self.fieldSize,
                       bitsPerComponent: 8,
                       bitsPerPixel: 32,
                       bytesPerRow: Self.fieldSize * 4,
                       space: colorSpace,
                       bitmapInfo: bitmapInfo,
                       provider: provider,
                       decode: nil,
                       shouldInterpolate: false,
                       intent: .defaultIntent)
    }

    /// A monotonic millisecond clock, same convention the firmware's
    /// millis() follows — truncated (not clamped) to 32 bits, which is
    /// safe because every elapsed-time comparison in BreathScene already
    /// uses wraparound-safe unsigned subtraction, exactly as it has to on
    /// the device, where millis() itself wraps every ~49 days.
    private static func nowMs() -> UInt32 {
        UInt32(truncatingIfNeeded: Int64(CACurrentMediaTime() * 1000))
    }
}
