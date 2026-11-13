@testable import ShadowListUtils
import ShadowListUtilsCore
import UIKit
import XCTest

@MainActor
final class ProgressControlsTests: XCTestCase {
  private var format: UIGraphicsImageRendererFormat {
    let format = UIGraphicsImageRendererFormat(); format.scale = 1; format.preferredRange = .standard
    return format
  }
  private func pixel(_ view: UIView, at point: CGPoint) -> Data? {
    UIGraphicsImageRenderer(size: CGSize(width: 1, height: 1), format: format).image { context in
      context.cgContext.translateBy(x: -point.x, y: -point.y); view.draw(view.bounds)
    }.pngData()
  }
  private func pixel(_ color: UIColor) -> Data? {
    UIGraphicsImageRenderer(size: CGSize(width: 1, height: 1), format: format).image { _ in
      color.setFill(); UIRectFill(CGRect(x: 0, y: 0, width: 1, height: 1))
    }.pngData()
  }

  func testBothHelpersReplayActualRNProgressVectors() throws {
    let bar = ShadowListUtilsProgressBar(fraction: 0, accessibilityLabel: "Trip preparation")
    for entry in try Vectors.cases("progress.json") {
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      let value = (input["fraction"] as? NSNumber)?.doubleValue ?? .nan
      XCTAssertEqual(ShadowListUtilsCore.clampFraction(value), try XCTUnwrap(output["fraction"] as? Double))
      let percent = try XCTUnwrap(output["percent"] as? Int)
      XCTAssertEqual(ShadowListUtilsCore.progressPercent(value), percent)
      bar.fraction = value; XCTAssertEqual(bar.accessibilityValue, "\(percent)%")
    }
    XCTAssertEqual(ShadowListUtilsCore.clampFraction(.infinity), 0)
    XCTAssertEqual(ShadowListUtilsCore.clampFraction(-.infinity), 0)
  }

  func testBarPaintDefaultsReadOnlyAccessibilityAndNativeLeadingDirection() {
    let bar = ShadowListUtilsProgressBar(fraction: 0.5, accessibilityLabel: "Trip preparation", theme: .light)
    bar.frame = CGRect(x: 0, y: 0, width: 200, height: 6)
    bar.semanticContentAttribute = .forceLeftToRight
    XCTAssertEqual(bar.intrinsicContentSize.height, 6)
    XCTAssertEqual(pixel(bar, at: CGPoint(x: 25, y: 3)), pixel(bar.theme.colors.accent))
    XCTAssertEqual(pixel(bar, at: CGPoint(x: 150, y: 3)), pixel(bar.theme.colors.fill))
    bar.semanticContentAttribute = .forceRightToLeft
    XCTAssertEqual(pixel(bar, at: CGPoint(x: 25, y: 3)), pixel(bar.theme.colors.fill))
    XCTAssertEqual(pixel(bar, at: CGPoint(x: 150, y: 3)), pixel(bar.theme.colors.accent))
    XCTAssertTrue(bar.isAccessibilityElement); XCTAssertTrue(bar.accessibilityTraits.contains(.updatesFrequently))
    XCTAssertFalse(bar.accessibilityTraits.contains(.adjustable)); XCTAssertEqual(bar.accessibilityLabel, "Trip preparation")
    bar.height = 8; XCTAssertEqual(bar.intrinsicContentSize.height, 8)
    bar.fraction = .nan; XCTAssertTrue(bar.fraction.isNaN); XCTAssertEqual(bar.accessibilityValue, "0%")
  }

  func testRingIsDecorativeAndFillsClockwiseEvenInRTL() {
    let ring = ShadowListUtilsProgressRing(fraction: 0.25, theme: .light)
    ring.frame = CGRect(origin: .zero, size: ring.intrinsicContentSize)
    ring.semanticContentAttribute = .forceRightToLeft
    XCTAssertEqual(ring.size, 52); XCTAssertEqual(ring.strokeWidth, 7)
    XCTAssertFalse(ring.isAccessibilityElement); XCTAssertTrue(ring.accessibilityElementsHidden)
    XCTAssertEqual(pixel(ring, at: CGPoint(x: 26, y: 3)), pixel(ring.theme.colors.accent))
    XCTAssertEqual(pixel(ring, at: CGPoint(x: 49, y: 26)), pixel(ring.theme.colors.accent))
    XCTAssertEqual(pixel(ring, at: CGPoint(x: 26, y: 49)), pixel(ring.theme.colors.accentSoft))
    XCTAssertEqual(pixel(ring, at: CGPoint(x: 26, y: 26)), pixel(.clear))
    ring.fraction = .infinity
    XCTAssertEqual(pixel(ring, at: CGPoint(x: 26, y: 3)), pixel(ring.theme.colors.accentSoft))
  }

  func testAttachedValueUpdatesQueueNoImplicitLayerAnimation() throws {
    let window = UIWindow(frame: CGRect(x: 0, y: 0, width: 240, height: 120))
    let bar = ShadowListUtilsProgressBar(fraction: 0.25, accessibilityLabel: "Trip preparation")
    let ring = ShadowListUtilsProgressRing(fraction: 0.25)
    bar.frame = CGRect(x: 0, y: 0, width: 200, height: 6)
    ring.frame = CGRect(x: 0, y: 20, width: 52, height: 52)
    window.addSubview(bar); window.addSubview(ring); window.isHidden = false
    defer { window.isHidden = true }
    XCTAssertNotNil(bar.window); XCTAssertNotNil(ring.window)
    bar.layer.displayIfNeeded(); ring.layer.displayIfNeeded()
    let values = try Vectors.cases("progress.json").map { entry -> (Double, Int) in
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let output = try XCTUnwrap(entry["output"] as? [String: Any])
      return ((input["fraction"] as? NSNumber)?.doubleValue ?? .nan, try XCTUnwrap(output["percent"] as? Int))
    }
    CATransaction.begin(); CATransaction.setDisableActions(false); CATransaction.setAnimationDuration(2)
    for (fraction, percent) in values {
      bar.fraction = fraction; bar.layer.displayIfNeeded()
      XCTAssertEqual(bar.accessibilityValue, "\(percent)%")
    }
    bar.fraction = 0.75; ring.fraction = 0.75
    bar.layer.displayIfNeeded(); ring.layer.displayIfNeeded()
    CATransaction.commit()
    XCTAssertTrue((bar.layer.animationKeys() ?? []).isEmpty)
    XCTAssertTrue((ring.layer.animationKeys() ?? []).isEmpty)
    XCTAssertEqual(bar.accessibilityValue, "75%")
  }
}
