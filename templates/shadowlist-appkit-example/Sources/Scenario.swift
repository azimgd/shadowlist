import AppKit
import ShadowListKit

/*
 * Scripted correctness checks, started with -SLScenario <name>. Each logs one [SLSCENARIO] JSON
 * line and quits with -SLBenchExit 1. The window's number goes out first as [SLWINDOW], for
 * screencapture -l.
 *
 *   prepend  scroll into the list, add rows at the start, measure how far visible rows moved
 *   append   same with rows added at the end
 *   jump     scrollToItem to a far item, measure where it lands against the top of the viewport
 *   driven   -SLRoute Driven: a host drags both axes, rows are prepended mid drag, then it
 *            settles, rests and jumps. Logs the shift, the corrections, blank frames and the landing
 *   wheel    trackpad gestures with momentum, a bounce off each end, a mouse wheel, a gesture
 *            across the axis and animated commands, sent as wheel events. Logs where each landed
 *
 * -SLShot scrolls a little first, which shows the overlay scrollers in the picture.
 */
enum Scenario {
  static func start(_ screen: NSView & ListScreen) {
    print("[SLWINDOW] \(screen.window?.windowNumber ?? -1)")
    fflush(stdout)
    if let path = Settings.shotPath {
      after(1.5) {
        if let driven = screen as? DrivenScreen {
          driven.scroll(to: CGPoint(x: 160, y: 900))
        } else {
          screen.list.contentOffset = CGPoint(x: 0, y: 900)
        }
        after(0.2) { shoot(screen, to: path) }
      }
      return
    }
    guard let name = Settings.scenario else { return }
    if name == "driven", let driven = screen as? DrivenScreen {
      after(1.5) { checkDriven(driven) }
      return
    }
    if name == "wheel" {
      after(1.5) { checkWheel(screen.list) }
      return
    }
    after(1.5) {
      let list = screen.list
      // Rest in the middle of the loaded content, away from both ends.
      let middle = max(0, min(4000, (list.contentSize.height - list.bounds.height) / 2))
      if name != "jump" {
        list.contentOffset = CGPoint(x: 0, y: middle)
      }
      after(1.0) {
        let before = visibleRows(list)
        let target = screen.keys.count * 3 / 4
        let targetKey = screen.keys[target]
        switch name {
        case "prepend": screen.prependRows()
        case "append": screen.appendRows()
        case "jump": list.scrollToItem(at: target, viewPosition: 0, animated: false)
        default: break
        }
        after(1.0) {
          var result: [String: Any] = ["scenario": name, "engine": "shadowlist", "screen": String(describing: type(of: screen))]
          if name == "jump" {
            let rows = visibleRows(list)
            result["landedOffset"] = rows[targetKey].map { Double($0) } ?? NSNull()
            result["targetVisible"] = rows[targetKey] != nil
          } else {
            let (shift, kept) = maxShift(from: before, to: visibleRows(list))
            result["maxShift"] = Double(shift)
            result["visibleBefore"] = before.count
            result["stillVisible"] = kept
          }
          finish(result)
        }
      }
    }
  }

  /*
   * A drag down and to the right, one step a frame, with rows prepended halfway. Then momentum,
   * rest and a jump. Every frame checks the rows cover the viewport and the tiles follow the
   * sideways offset.
   */
  private static func checkDriven(_ screen: DrivenScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "driven", "engine": "shadowlist", "screen": "DrivenScreen"]
    var frame = 0
    var blankFrames = 0
    var tileMisses = 0
    var target = 0
    var targetKey = ""
    list.scrollPhase = .dragging
    Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { timer in
      frame += 1
      switch frame {
      case 1...60:
        screen.scroll(to: CGPoint(x: screen.columnOffset + 7, y: list.contentOffset.y + 45))
      case 61:
        list.scrollPhase = .settling
      case 62...90:
        let speed = CGFloat(91 - frame)
        screen.scroll(to: CGPoint(x: screen.columnOffset + speed / 6, y: list.contentOffset.y + speed))
      case 91:
        list.scrollPhase = .idle
      default:
        break
      }
      list.layoutSubtreeIfNeeded()
      if frame <= 91 {
        if !coversViewport(list) {
          blankFrames += 1
        }
        if !tilesFollow(screen) {
          tileMisses += 1
        }
      }
      if frame == 30 {
        let before = visibleRows(list)
        let corrections = screen.corrections
        screen.prependRows()
        list.layoutSubtreeIfNeeded()
        let (shift, kept) = maxShift(from: before, to: visibleRows(list))
        result["prependMaxShift"] = Double(shift)
        result["prependVisibleBefore"] = before.count
        result["prependStillVisible"] = kept
        result["prependCorrections"] = screen.corrections - corrections
        result["prependWhileDragging"] = list.scrollPhase == .dragging
      }
      if frame == 95 {
        target = screen.keys.count * 2 / 3
        targetKey = screen.keys[target]
        list.scrollToItem(at: target, viewPosition: 0, animated: false)
      }
      if frame == 110 {
        timer.invalidate()
        let rows = visibleRows(list)
        result["landedOffset"] = rows[targetKey].map { Double($0) } ?? NSNull()
        result["targetVisible"] = rows[targetKey] != nil
        result["blankFrames"] = blankFrames
        result["tileMisses"] = tileMisses
        result["corrections"] = screen.corrections
        result["columnOffset"] = Double(screen.columnOffset)
        finish(result)
      }
    }
  }

  /*
   * One step of a scripted wheel run: a wheel event, a check, or a frame of waiting.
   */
  private enum WheelStep {
    case event(CGPoint, NSEvent.Phase, NSEvent.Phase)
    case run(() -> Void)
    case wait
  }

  /*
   * A trackpad gesture with momentum along y: deltas the way NSEvent reports them, negative
   * moving toward the end.
   */
  private static func gesture(_ dragDelta: CGFloat, drags: Int, momentum: [CGFloat], across: CGFloat = 0) -> [WheelStep] {
    var steps: [WheelStep] = [.event(.zero, .began, [])]
    steps += (0..<drags).map { _ in .event(CGPoint(x: across, y: dragDelta), .changed, []) }
    steps.append(.event(.zero, .ended, []))
    for (index, delta) in momentum.enumerated() {
      steps.append(.event(CGPoint(x: 0, y: delta), [], index == 0 ? .began : .changed))
    }
    steps.append(.event(.zero, [], .ended))
    return steps
  }

  private static func waiting(_ seconds: Double) -> [WheelStep] {
    Array(repeating: .wait, count: Int(seconds * 60))
  }

  /*
   * Wheel events one frame apart on the feed, with the list's own display link running between
   * them. Every gesture's landing is checked against the deltas sent, and every bounce against
   * the edge it left.
   */
  private static func checkWheel(_ list: ShadowListKitListView) {
    var result: [String: Any] = ["scenario": "wheel", "engine": "shadowlist", "screen": "FeedScreen"]
    var start: CGFloat = 0
    var deepest: CGFloat = 0
    let decay: [CGFloat] = (0..<24).map { (-40 * pow(0.85, CGFloat($0))).rounded() }
    let travel = 12 * 25 + decay.reduce(0) { $0 - $1 }
    var steps: [WheelStep] = []

    steps.append(.run { start = list.contentOffset.y })
    steps += gesture(-25, drags: 12, momentum: decay)
    steps += waiting(0.3)
    steps.append(
      .run {
        result["gestureTravel"] = Double(list.contentOffset.y - start)
        result["gestureExpected"] = Double(travel)
        result["gestureIdle"] = list.scrollPhase == .idle
      })

    steps.append(.run { list.scrollToStart(animated: false) })
    steps += gesture(40, drags: 10, momentum: [])
    steps.append(.run { result["startPulledPast"] = Double(-list.contentOffset.y) })
    steps += waiting(0.6)
    steps.append(
      .run {
        result["startBounceReturned"] = abs(list.contentOffset.y) < 0.01
        // Momentum carries the list into its end from a little way up.
        list.scrollToEnd(animated: false)
        list.contentOffset = CGPoint(x: 0, y: list.contentOffset.y - 150)
        deepest = list.contentOffset.y
      })
    steps += gesture(-10, drags: 2, momentum: Array(repeating: -40, count: 20)).map { step in
      if case .event = step {
        return [step, .run { deepest = max(deepest, list.contentOffset.y) }]
      }
      return [step]
    }.flatMap { $0 }
    steps += waiting(0.6)
    steps.append(
      .run {
        let end = list.contentSize.height - list.bounds.height
        result["endBounceDepth"] = Double(deepest - end)
        result["endBounceLimit"] = Double(list.bounds.height * 0.12)
        result["endBounceReturned"] = abs(list.contentOffset.y - end) < 0.5
        list.scrollToItem(at: 40, viewPosition: 0, animated: false)
        start = list.contentOffset.y
        mouseWheel(list, lines: -3)
        result["mouseWheelTravel"] = Double(list.contentOffset.y - start)
        start = list.contentOffset.y
      })
    steps += gesture(-30, drags: 6, momentum: [-30, -20], across: -60)
    steps.append(.run { result["crossAxisTravel"] = Double(list.contentOffset.y - start) })
    steps += waiting(0.2)
    steps.append(.run { list.scrollToItem(at: 300, viewPosition: 0, animated: true) })
    steps += waiting(0.6)
    steps.append(
      .run {
        let rows = visibleRows(list)
        let screen = list.superview as? ListScreen
        result["animatedLanded"] = screen.flatMap { rows[$0.keys[300]] }.map { Double($0) } ?? NSNull()
      })

    var index = 0
    Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { timer in
      guard index < steps.count else {
        timer.invalidate()
        finish(result)
        return
      }
      switch steps[index] {
      case .event(let delta, let phase, let momentum):
        list.shadowListKit_scrollWheelDelta(delta, phase: phase, momentumPhase: momentum)
      case .run(let work):
        work()
      case .wait:
        break
      }
      index += 1
    }
  }

  /*
   * A mouse wheel's notches, the way a wheel without precise deltas sends them.
   */
  private static func mouseWheel(_ list: ShadowListKitListView, lines: Int32) {
    guard let event = CGEvent(scrollWheelEvent2Source: nil, units: .line, wheelCount: 1, wheel1: lines, wheel2: 0, wheel3: 0),
      let wheel = NSEvent(cgEvent: event)
    else { return }
    list.scrollWheel(with: wheel)
    list.layoutSubtreeIfNeeded()
  }

  /*
   * Whether the mounted rows cover the viewport from top to bottom without a gap.
   */
  private static func coversViewport(_ list: ShadowListKitListView) -> Bool {
    let frames = list.visibleCells.map { list.convert($0.frame, from: $0.superview) }.sorted { $0.minY < $1.minY }
    guard let first = frames.first, let last = frames.last, first.minY <= 0.5, last.maxY >= list.bounds.height - 0.5 else {
      return false
    }
    return zip(frames, frames.dropFirst()).allSatisfy { $1.minY - $0.maxY <= 0.5 }
  }

  /*
   * Whether each row's first tile sits where the sideways offset puts it.
   */
  private static func tilesFollow(_ screen: DrivenScreen) -> Bool {
    let width = GridRowCell.columnWidth
    let shift = screen.columnOffset
    let expected = GridRowCell.numberWidth + floor(shift / width) * width - shift + 6
    for case let cell as GridRowCell in screen.list.visibleCells {
      cell.layoutSubtreeIfNeeded()
      guard let x = cell.firstTileX, abs(x - expected) < 0.5 else { return false }
    }
    return true
  }

  /*
   * The window's content as its layers draw it, at the backing scale.
   */
  private static func shoot(_ screen: NSView, to path: String) {
    let scale = screen.window?.backingScaleFactor ?? 2
    let size = screen.bounds.size
    let rep = NSBitmapImageRep(
      bitmapDataPlanes: nil, pixelsWide: Int(size.width * scale), pixelsHigh: Int(size.height * scale), bitsPerSample: 8,
      samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    let context = NSGraphicsContext(bitmapImageRep: rep)!
    // Layers draw top down in a flipped view. The bitmap's origin is at its bottom.
    context.cgContext.translateBy(x: 0, y: size.height * scale)
    context.cgContext.scaleBy(x: scale, y: -scale)
    screen.layer?.render(in: context.cgContext)
    try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: path))
    print("[SLSHOT] \(path)")
    fflush(stdout)
    exit(0)
  }

  private static func finish(_ result: [String: Any]) {
    let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    if Settings.exitsAfterScenario {
      exit(0)
    }
  }

  private static func after(_ seconds: Double, _ work: @escaping () -> Void) {
    DispatchQueue.main.asyncAfter(deadline: .now() + seconds, execute: work)
  }

  private static func maxShift(from before: [String: CGFloat], to after: [String: CGFloat]) -> (CGFloat, Int) {
    var shift: CGFloat = 0
    var kept = 0
    for (key, y) in before {
      if let moved = after[key] {
        shift = max(shift, abs(moved - y))
        kept += 1
      }
    }
    return (shift, kept)
  }

  /*
   * The top of every row on screen in the list's coordinates, by key.
   */
  static func visibleRows(_ list: ShadowListKitListView) -> [String: CGFloat] {
    var rows: [String: CGFloat] = [:]
    for cell in list.visibleCells {
      let frame = list.convert(cell.frame, from: cell.superview)
      guard frame.maxY > 0, frame.minY < list.bounds.height else { continue }
      if let feed = cell as? FeedCell {
        rows[feed.key] = frame.minY
      } else if cell.index >= 0, let screen = list.superview as? ListScreen, cell.index < screen.keys.count {
        rows[screen.keys[cell.index]] = frame.minY
      }
    }
    return rows
  }
}
