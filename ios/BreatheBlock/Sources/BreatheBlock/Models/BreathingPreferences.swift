import Foundation

/// Where the blob's live signal comes from. Always `.ambient` today — there
/// is no BLE pairing or HealthKit read anywhere in this build yet (that's
/// the phase 2/3 work in ios/SETUP.md). This exists now as the seam those
/// phases plug into, not as a feature: nothing currently sets it to
/// anything but `.ambient`, and the app must never claim a source it
/// doesn't actually have — see DESIGN.md's "no numbers unasked" principle.
enum BreathDataSource {
    case ambient
    case device
    case oura
    case appleHealth
}

/// UserDefaults keys shared between SettingsView (which writes them via
/// @AppStorage) and BreathingController (which reads the persisted style
/// once at launch) — kept in one place so the two can't drift apart.
enum PreferenceKeys {
    static let breathingStyle = "breathingStyle"
    static let objective = "objective"
}

/// Pacing only — inhale/exhale length and round count, the same fields
/// BreathEngine.setBreathingStyle(inhaleMs:exhaleMs:cycles:) takes. Every
/// style is still the engine's plain two-phase inhale/exhale cycle; there's
/// no held-breath phase to offer a box-breathing style honestly yet.
enum BreathingStyle: String, CaseIterable, Identifiable {
    case calm, slow, brisk

    var id: String { rawValue }

    var name: String {
        switch self {
        case .calm: return "Calm"
        case .slow: return "Slow"
        case .brisk: return "Brisk"
        }
    }

    var detail: String {
        switch self {
        case .calm: return "4s in · 6s out · 5 rounds"
        case .slow: return "5s in · 8s out · 4 rounds"
        case .brisk: return "3s in · 4s out · 7 rounds"
        }
    }

    var inhaleMs: UInt32 {
        switch self {
        case .calm: return 4000
        case .slow: return 5000
        case .brisk: return 3000
        }
    }

    var exhaleMs: UInt32 {
        switch self {
        case .calm: return 6000
        case .slow: return 8000
        case .brisk: return 4000
        }
    }

    var cycles: UInt8 {
        switch self {
        case .calm: return 5
        case .slow: return 4
        case .brisk: return 7
        }
    }
}

/// A stored preference, not yet something the app acts on beyond picking a
/// matching BreathingStyle — there's no calendar or health data yet for it
/// to personalize (see InsightsView's note). Kept simple on purpose rather
/// than wired to behavior it can't honestly deliver yet.
enum BreathingObjective: String, CaseIterable, Identifiable {
    case calm, focus, sleep

    var id: String { rawValue }

    var name: String {
        switch self {
        case .calm: return "Calm"
        case .focus: return "Focus"
        case .sleep: return "Sleep"
        }
    }

    var detail: String {
        switch self {
        case .calm: return "a general, day-to-day reset"
        case .focus: return "a short reset between tasks"
        case .sleep: return "winding down before bed"
        }
    }

    var matchingStyle: BreathingStyle {
        switch self {
        case .calm: return .calm
        case .focus: return .brisk
        case .sleep: return .slow
        }
    }
}
