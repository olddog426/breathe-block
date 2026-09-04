import SwiftUI
import SwiftData

@main
struct BreatheBlockApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .modelContainer(for: BreathSession.self)
    }
}
