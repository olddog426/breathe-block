import SwiftUI

/// Breathing style actually changes the pacing (pushed to BreathEngine
/// immediately, via the same controller BreathingView already ticks).
/// Objective is a stored preference that also picks a matching style, but
/// doesn't yet change anything beyond that — see InsightsView's note for
/// why: there's nothing here yet for it to personalize.
struct SettingsView: View {
    @ObservedObject var controller: BreathingController
    @AppStorage(PreferenceKeys.breathingStyle) private var styleRaw = BreathingStyle.calm.rawValue
    @AppStorage(PreferenceKeys.objective) private var objectiveRaw = BreathingObjective.calm.rawValue

    var body: some View {
        List {
            Section("Breathing style") {
                ForEach(BreathingStyle.allCases) { style in
                    optionRow(name: style.name,
                              detail: style.detail,
                              selected: styleRaw == style.rawValue) {
                        styleRaw = style.rawValue
                        controller.applyBreathingStyle(style)
                    }
                }
            }
            Section("Objective") {
                ForEach(BreathingObjective.allCases) { objective in
                    optionRow(name: objective.name,
                              detail: objective.detail,
                              selected: objectiveRaw == objective.rawValue) {
                        objectiveRaw = objective.rawValue
                        styleRaw = objective.matchingStyle.rawValue
                        controller.applyBreathingStyle(objective.matchingStyle)
                    }
                }
            }
        }
        .navigationTitle("Settings")
        .navigationBarTitleDisplayMode(.inline)
    }

    private func optionRow(name: String, detail: String, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            HStack {
                VStack(alignment: .leading, spacing: 2) {
                    Text(name)
                        .font(.subheadline)
                    Text(detail)
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }
                Spacer()
                if selected {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.footnote)
                        .foregroundStyle(Color.accentColor)
                }
            }
        }
        .tint(.primary)
    }
}

#Preview {
    NavigationStack {
        SettingsView(controller: BreathingController())
    }
}
