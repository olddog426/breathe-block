import SwiftUI
import SwiftData
import Charts

/// A day's worth of sessions, rolled up for the chart. Kept separate from
/// BreathSession itself so the chart doesn't care how the rollup is
/// computed — daily totals now, something richer once there's vitals or
/// calendar data to correlate against.
private struct DayTotal: Identifiable {
    let day: Date
    let minutes: Double
    var id: Date { day }
}

struct JournalView: View {
    @Query(sort: \BreathSession.startedAt, order: .reverse)
    private var sessions: [BreathSession]

    private var dailyTotals: [DayTotal] {
        let calendar = Calendar.current
        let grouped = Dictionary(grouping: sessions) { session in
            calendar.startOfDay(for: session.startedAt)
        }
        let today = calendar.startOfDay(for: Date())
        // Always show the last 14 days, even the empty ones, so a quiet
        // stretch reads as quiet rather than as a gap in the axis.
        return (0..<14).compactMap { offset -> DayTotal? in
            guard let day = calendar.date(byAdding: .day, value: -offset, to: today) else {
                return nil
            }
            let minutes = (grouped[day] ?? []).reduce(0.0) { $0 + $1.duration / 60.0 }
            return DayTotal(day: day, minutes: minutes)
        }.reversed()
    }

    var body: some View {
        List {
            Section {
                Chart(dailyTotals) { total in
                    BarMark(
                        x: .value("Day", total.day, unit: .day),
                        y: .value("Minutes", total.minutes)
                    )
                    .foregroundStyle(Color.accentColor)
                }
                .frame(height: 160)
                .listRowInsets(EdgeInsets(top: 12, leading: 4, bottom: 12, trailing: 4))
            } header: {
                Text("Minutes breathed, last 14 days")
            }

            Section {
                if sessions.isEmpty {
                    Text("No sessions yet.")
                        .foregroundStyle(.secondary)
                } else {
                    ForEach(sessions) { session in
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(session.startedAt, style: .date)
                                Text(session.startedAt, style: .time)
                                    .font(.caption)
                                    .foregroundStyle(.secondary)
                            }
                            Spacer()
                            Text(durationLabel(session.duration))
                                .font(.system(.body, design: .monospaced))
                                .foregroundStyle(.secondary)
                            if !session.completed {
                                Image(systemName: "xmark.circle")
                                    .foregroundStyle(.tertiary)
                                    .help("Dismissed before finishing")
                            }
                        }
                    }
                }
            } header: {
                Text("Sessions")
            }
        }
        .navigationTitle("Journal")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func durationLabel(_ duration: TimeInterval) -> String {
        let totalSeconds = max(0, Int(duration.rounded()))
        return String(format: "%d:%02d", totalSeconds / 60, totalSeconds % 60)
    }
}

#Preview {
    NavigationStack {
        JournalView()
    }
    .modelContainer(for: BreathSession.self, inMemory: true)
}
