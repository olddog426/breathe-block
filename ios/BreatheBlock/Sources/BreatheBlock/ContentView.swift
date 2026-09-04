import SwiftUI
import SwiftData

enum AppDestination: Hashable {
    case journal, insights, settings
}

struct ContentView: View {
    @Environment(\.modelContext) private var modelContext
    @StateObject private var controller = BreathingController()
    @State private var path = NavigationPath()
    @State private var showMenu = false

    var body: some View {
        NavigationStack(path: $path) {
            BreathingView(controller: controller)
                .padding(36)
                .frame(maxWidth: .infinity, maxHeight: .infinity)
                .toolbar {
                    ToolbarItem(placement: .topBarLeading) {
                        Button {
                            showMenu = true
                        } label: {
                            Image(systemName: "line.3.horizontal")
                                .font(.system(size: 14, weight: .medium))
                        }
                        .frame(width: 34, height: 34)
                        .background(.thinMaterial, in: Circle())
                    }
                    ToolbarItem(placement: .topBarTrailing) {
                        // A passive status LED, not a control — see
                        // BreathDataSource's doc comment. Always ambient
                        // today; the color is the one seam a later BLE/
                        // HealthKit phase changes.
                        Circle()
                            .fill(dataSourceColor)
                            .frame(width: 8, height: 8)
                    }
                }
                .toolbarBackground(.hidden, for: .navigationBar)
                .navigationDestination(for: AppDestination.self) { destination in
                    switch destination {
                    case .journal: JournalView()
                    case .insights: InsightsView()
                    case .settings: SettingsView(controller: controller)
                    }
                }
        }
        .onAppear {
            controller.onSessionFinished = { startedAt, completed in
                logSession(startedAt: startedAt, completed: completed)
            }
        }
        .sheet(isPresented: $showMenu) {
            MenuSheet { destination in
                showMenu = false
                path.append(destination)
            }
            .presentationDetents([.height(220)])
            .presentationDragIndicator(.visible)
        }
        .preferredColorScheme(.dark)
    }

    private var dataSourceColor: Color {
        switch controller.dataSource {
        case .ambient: return Color(white: 0.3)
        case .device: return .green
        case .oura: return .accentColor
        case .appleHealth: return .pink
        }
    }

    private func logSession(startedAt: Date, completed: Bool) {
        let session = BreathSession(startedAt: startedAt, endedAt: Date(), completed: completed)
        modelContext.insert(session)
    }
}

private struct MenuSheet: View {
    let onSelect: (AppDestination) -> Void

    var body: some View {
        VStack(spacing: 4) {
            row("Journal", .journal)
            row("Insights", .insights)
            row("Settings", .settings)
        }
        .padding(12)
    }

    private func row(_ title: String, _ destination: AppDestination) -> some View {
        Button {
            onSelect(destination)
        } label: {
            HStack(spacing: 12) {
                Circle().fill(Color.accentColor).frame(width: 6, height: 6)
                Text(title)
                Spacer()
            }
            .padding(.vertical, 12)
            .padding(.horizontal, 8)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .foregroundStyle(.primary)
    }
}

#Preview {
    ContentView()
        .modelContainer(for: BreathSession.self, inMemory: true)
}
