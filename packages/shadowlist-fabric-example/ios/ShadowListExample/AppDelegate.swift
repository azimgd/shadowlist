import UIKit
import React
import React_RCTAppDelegate
import ReactAppDependencyProvider

@main
class AppDelegate: UIResponder, UIApplicationDelegate {
  var window: UIWindow?

  var reactNativeDelegate: ReactNativeDelegate?
  var reactNativeFactory: RCTReactNativeFactory?

  func application(
    _ application: UIApplication,
    didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
  ) -> Bool {
    let delegate = ReactNativeDelegate()
    let factory = RCTReactNativeFactory(delegate: delegate)
    // Engine flags that make list commits cheaper, see ShadowListEngineFlags.h.
    ShadowListApplyEngineFlags()
    delegate.dependencyProvider = RCTAppDependencyProvider()

    reactNativeDelegate = delegate
    reactNativeFactory = factory

    window = UIWindow(frame: UIScreen.main.bounds)

    factory.startReactNative(
      withModuleName: "ShadowListExample",
      in: window,
      launchOptions: launchOptions
    )

    if let plan = UserDefaults.standard.string(forKey: "SLAutoFling") {
      AutoFlingDriver.shared.start(plan: plan)
    }
    if UserDefaults.standard.string(forKey: "SLJsFps") == "1" {
      UIFrameMonitor.shared.start()
      RCTSetLogThreshold(.info)
      let defaultLog = RCTGetLogFunction()
      RCTSetLogFunction { level, source, fileName, lineNumber, message in
        if let message, message.contains("[SLFPS]") {
          FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
        }
        defaultLog?(level, source, fileName, lineNumber, message)
      }
    }

    return true
  }
}

/*
 * BENCHMARK-ONLY. With -SLJsFps 1, logs one [SLUI] line per second of main thread frames:
 * frames, frames over 20 ms, the longest gap. Same numbers for any list library.
 */
final class UIFrameMonitor: NSObject {
  static let shared = UIFrameMonitor()
  private var link: CADisplayLink?
  private var last: CFTimeInterval = 0
  private var windowStart: CFTimeInterval = 0
  private var frames = 0
  private var slow = 0
  private var longest: CFTimeInterval = 0

  func start() {
    let link = CADisplayLink(target: self, selector: #selector(tick(_:)))
    link.preferredFrameRateRange = CAFrameRateRange(minimum: 60, maximum: 60, preferred: 60)
    link.add(to: .main, forMode: .common)
    self.link = link
  }

  @objc private func tick(_ link: CADisplayLink) {
    let now = link.timestamp
    if last != 0 {
      let gap = now - last
      frames += 1
      if gap > 0.020 { slow += 1 }
      longest = max(longest, gap)
    } else {
      windowStart = now
    }
    last = now
    if now - windowStart >= 1 {
      let line = String(
        format: "[SLUI] t=%.0f span=%.0f frames=%d slow=%d longest=%.1f",
        now * 1000, (now - windowStart) * 1000, frames, slow, longest * 1000)
      FileHandle.standardError.write((line + "\n").data(using: .utf8)!)
      windowStart = now
      frames = 0
      slow = 0
      longest = 0
    }
  }
}

class ReactNativeDelegate: RCTDefaultReactNativeFactoryDelegate {
  override func sourceURL(for bridge: RCTBridge) -> URL? {
    self.bundleURL()
  }

  override func bundleURL() -> URL? {
#if DEBUG
    RCTBundleURLProvider.sharedSettings().jsBundleURL(forBundleRoot: "index")
#else
    Bundle.main.url(forResource: "main", withExtension: "jsbundle")
#endif
  }
}

/*
 * BENCHMARK-ONLY. -SLAutoFling "delay,forward,back,velocity" flings the tallest vertical scroll
 * view in the window, like a finger would: each fling starts at velocity pt/s and slows with
 * UIKit's normal deceleration, set straight on contentOffset once per frame. It replaces
 * XCTest gestures, which snapshot the accessibility tree on the main thread for every gesture
 * and cost more the more views a list keeps mounted. Negative velocity flings toward the
 * start (a chat's history). Writes [SLBENCH] start and end lines to stderr.
 */
final class AutoFlingDriver: NSObject {
  static let shared = AutoFlingDriver()
  private var link: CADisplayLink?
  private var flings: [Double] = []
  private var velocity: Double = 0
  private var last: CFTimeInterval = 0
  private var pauseUntil: CFTimeInterval = 0
  private weak var scrollView: UIScrollView?

  func start(plan: String) {
    let parts = plan.split(separator: ",").compactMap { Double($0) }
    guard parts.count == 4 else { return }
    let speed = parts[3]
    flings = Array(repeating: speed, count: Int(parts[1])) + Array(repeating: -speed, count: Int(parts[2]))
    DispatchQueue.main.asyncAfter(deadline: .now() + parts[0]) { self.begin() }
  }

  private func begin() {
    guard let window = UIApplication.shared.connectedScenes
      .compactMap({ ($0 as? UIWindowScene)?.keyWindow }).first,
      let target = tallestScrollView(in: window)
    else {
      log("[SLBENCH] no scroll view")
      return
    }
    scrollView = target
    log("[SLBENCH] start")
    let link = CADisplayLink(target: self, selector: #selector(tick(_:)))
    link.preferredFrameRateRange = CAFrameRateRange(minimum: 60, maximum: 60, preferred: 60)
    link.add(to: .main, forMode: .common)
    self.link = link
  }

  private func tallestScrollView(in view: UIView) -> UIScrollView? {
    var best: UIScrollView?
    func visit(_ view: UIView) {
      if let scroll = view as? UIScrollView, scroll.contentSize.height > scroll.bounds.height,
        scroll.contentSize.height > (best?.contentSize.height ?? 0)
      {
        best = scroll
      }
      view.subviews.forEach(visit)
    }
    visit(view)
    return best
  }

  @objc private func tick(_ link: CADisplayLink) {
    let now = link.timestamp
    defer { last = now }
    guard let scrollView, last != 0 else { return }
    if velocity == 0 {
      if now < pauseUntil { return }
      if flings.isEmpty {
        link.invalidate()
        log("[SLBENCH] end")
        return
      }
      velocity = flings.removeFirst()
    }
    let dt = now - last
    // UIScrollView.DecelerationRate.normal is 0.998 per millisecond.
    velocity *= pow(0.998, dt * 1000)
    var offset = scrollView.contentOffset
    let minY = -scrollView.adjustedContentInset.top
    let maxY = max(minY, scrollView.contentSize.height - scrollView.bounds.height + scrollView.adjustedContentInset.bottom)
    offset.y = min(maxY, max(minY, offset.y + velocity * dt))
    scrollView.contentOffset = offset
    if abs(velocity) < 60 || offset.y == minY || offset.y == maxY {
      velocity = 0
      pauseUntil = now + 0.1
    }
  }

  private func log(_ line: String) {
    FileHandle.standardError.write((line + "\n").data(using: .utf8)!)
  }
}
