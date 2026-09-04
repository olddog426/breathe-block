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
                    Text(controller.sessionActive ? "dismiss" : "breathe with me")
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
            controller.onSessionFinished = { startedAt in
                logSession(startedAt: startedAt, completed: true)
            }
        }
        .preferredColorScheme(.dark)
    }

    private func primaryAction() {
        if controller.sessionActive {
            if let startedAt = controller.dismiss() {
                logSession(startedAt: startedAt, completed: false)
            }
        } else {
            controller.start()
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
