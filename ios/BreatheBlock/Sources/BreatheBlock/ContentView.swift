import SwiftUI
import SwiftData

struct ContentView: View {
    @Environment(\.modelContext) private var modelContext
    @StateObject private var controller = BreathingController()

    var body: some View {
        NavigationStack {
            VStack(spacing: 28) {
                Spacer(minLength: 0)

                BreathingView(controller: controller)
                    .padding(.horizontal, 40)

                Text(controller.stateName)
                    .font(.system(.footnote, design: .monospaced))
                    .foregroundStyle(.secondary)
                    .textCase(.uppercase)
                    .tracking(1.5)

                Button(action: primaryAction) {
                    Text(primaryLabel)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 4)
                }
                .buttonStyle(.bordered)
                .tint(.primary)
                .padding(.horizontal, 40)

                Spacer(minLength: 0)
            }
            .padding(.vertical, 24)
            .navigationTitle("Breathe Block")
            .navigationBarTitleDisplayMode(.inline)
            .toolbar {
                ToolbarItem(placement: .topBarTrailing) {
                    NavigationLink("History") {
                        HistoryView()
                    }
                }
            }
        }
        .onAppear {
            controller.onSessionFinished = { startedAt, completed in
                logSession(startedAt: startedAt, completed: completed)
            }
        }
        .preferredColorScheme(.dark)
    }

    /// "breathe with me" opens an invitation; while it's held open the same
    /// button answers it ("begin"), same as a tap on the device's glass
    /// (DESIGN.md §4) — an invitation the app never let you answer would be
    /// a dead end, not a real button. Anything already guiding or releasing
    /// dismisses.
    private var primaryLabel: String {
        switch controller.stateName {
        case "resting": return "breathe with me"
        case "inviting": return "begin"
        default: return "dismiss"
        }
    }

    private func primaryAction() {
        switch controller.stateName {
        case "resting":
            controller.start()
        case "inviting":
            controller.beginInvitedSession()
        default:
            controller.dismiss()
        }
    }

    private func logSession(startedAt: Date, completed: Bool) {
        let session = BreathSession(startedAt: startedAt, endedAt: Date(), completed: completed)
        modelContext.insert(session)
    }
}

#Preview {
    ContentView()
        .modelContainer(for: BreathSession.self, inMemory: true)
}
