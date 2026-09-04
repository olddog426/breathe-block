import SwiftUI

/// The round light field, ticked once per display frame via TimelineView.
/// Deliberately just the glow for this first pass — no word overlay yet
/// (see ios/SETUP.md phase notes); the engine's `stateName`/`progress`
/// are exposed on the controller for a surrounding view to show as text
/// in the meantime.
struct BreathingView: View {
    @ObservedObject var controller: BreathingController

    var body: some View {
        TimelineView(.animation) { _ in
            Canvas { context, size in
                guard let image = controller.tick() else { return }
                let side = min(size.width, size.height)
                let origin = CGPoint(x: (size.width - side) / 2,
                                     y: (size.height - side) / 2)
                context.draw(Image(decorative: image, scale: 1),
                            in: CGRect(origin: origin,
                                      size: CGSize(width: side, height: side)))
            }
        }
        .background(Color.black)
        .aspectRatio(1, contentMode: .fit)
        .clipShape(Circle())
        .overlay(Circle().stroke(Color(white: 0.16), lineWidth: 8))
    }
}
