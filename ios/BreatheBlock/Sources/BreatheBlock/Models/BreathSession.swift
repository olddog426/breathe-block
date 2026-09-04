import Foundation
import SwiftData

/// One completed (or dismissed) guided session, logged locally on-device —
/// nothing leaves the phone in this build. This is deliberately the
/// smallest useful record for phase 1: no vitals yet, since nothing is
/// feeding them (no BLE, no HealthKit — see ios/SETUP.md for what's next).
/// Columns are additive later; this shape is safe to grow.
@Model
final class BreathSession {
    var startedAt: Date
    var endedAt: Date
    /// True if the session ran to completion; false if dismissed partway.
    var completed: Bool

    init(startedAt: Date, endedAt: Date, completed: Bool) {
        self.startedAt = startedAt
        self.endedAt = endedAt
        self.completed = completed
    }

    var duration: TimeInterval {
        endedAt.timeIntervalSince(startedAt)
    }
}
