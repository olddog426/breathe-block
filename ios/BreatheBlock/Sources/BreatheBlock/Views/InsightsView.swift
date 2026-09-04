import SwiftUI
import SwiftData

/// Honest and local-only: everything below comes from the sessions already
/// logged on this phone, nothing is inferred or fabricated. Real pattern-
/// finding (what tends to precede a shift) needs the calendar/health work
/// in ios/SETUP.md's later phases — until then this stays a plain summary
/// of your own record, not a claim about what's causing anything.
struct InsightsView: View {
    @Query private var sessions: [BreathSession]

    private var thisWeekCount: Int {
        let calendar = Calendar.current
        guard let weekAgo = calendar.date(byAdding: .day, value: -7, to: Date()) else { return 0 }
        return sessions.filter { $0.startedAt >= weekAgo }.count
    }

    private var averageDuration: TimeInterval {
        guard !sessions.isEmpty else { return 0 }
        return sessions.reduce(0) { $0 + $1.duration } / Double(sessions.count)
    }

    /// Longest run of consecutive days, ending today, with at least one session.
    private var streak: Int {
        let calendar = Calendar.current
        let days = Set(sessions.map { calendar.startOfDay(for: $0.startedAt) })
        var count = 0
        var day = calendar.startOfDay(for: Date())
        while days.contains(day) {
            count += 1
            guard let previous = calendar.date(byAdding: .day, value: -1, to: day) else { break }
            day = previous
        }
        return count
    }

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                HStack(spacing: 8) {
                    StatTile(value: "\(thisWeekCount)", label: "this week")
                    StatTile(value: averageDuration > 0 ? durationLabel(averageDuration) : "—", label: "avg. length")
                    StatTile(value: "\(streak)", label: "day streak")
                }

                Text("Insights here are just your own logged sessions for now. Once a paired device, calendar, or health data are connected, this is where the patterns behind your stress — not just a record of it — will start to show up.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
                    .padding(14)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .background(.quaternary, in: RoundedRectangle(cornerRadius: 12))
            }
            .padding(16)
        }
        .navigationTitle("Insights")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func durationLabel(_ duration: TimeInterval) -> String {
        let totalSeconds = max(0, Int(duration.rounded()))
        return String(format: "%d:%02d", totalSeconds / 60, totalSeconds % 60)
    }
}

private struct StatTile: View {
    let value: String
    let label: String

    var body: some View {
        VStack(spacing: 4) {
            Text(value)
                .font(.system(.title2, design: .monospaced))
                .fontWeight(.medium)
            Text(label)
                .font(.caption2)
                .foregroundStyle(.secondary)
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 12)
        .background(.quaternary, in: RoundedRectangle(cornerRadius: 12))
    }
}

#Preview {
    NavigationStack {
        InsightsView()
    }
    .modelContainer(for: BreathSession.self, inMemory: true)
}
