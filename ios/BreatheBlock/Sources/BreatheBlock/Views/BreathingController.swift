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

    /// Fires once, on the frame a guided session finishes on its own —
    /// never on a dismiss — with the session's start time, since
    /// `sessionStartedAt` is already cleared by the time this runs
    /// (deferred to the next run loop turn; see `tick()`). The natural
    /// moment to log it; see ContentView.
    var onSessionFinished: ((Date) -> Void)?

    private let engine = BreathEngine()
    private static let fieldSize = 466
    private var pixelBuffer = [UInt8](repeating: 0, count: fieldSize * fieldSize * 4)
    private let colorSpace = CGColorSpaceCreateDeviceRGB()

    private var sessionStartedAt: Date?

    func start() {
        sessionStartedAt = Date()
        engine.startSession(nowMs: Self.nowMs())
    }

    /// Returns the session's start time if one was active (so the caller
    /// can log a partial/dismissed entry), or nil if nothing was running.
    @discardableResult
    func dismiss() -> Date? {
        let startedAt = sessionStartedAt
        sessionStartedAt = nil
        engine.dismiss(nowMs: Self.nowMs())
        return startedAt
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

        if engine.consumeSessionFinished() {
            let startedAt = sessionStartedAt ?? Date()
            sessionStartedAt = nil
            let callback = onSessionFinished
            DispatchQueue.main.async { callback?(startedAt) }
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
