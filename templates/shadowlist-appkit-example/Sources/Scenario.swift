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
 */
enum Scenario {
  static func start(_ screen: NSView & ListScreen) {
    print("[SLWINDOW] \(screen.window?.windowNumber ?? -1)")
    fflush(stdout)
    if let path = Settings.shotPath {
      after(1.5) { shoot(screen, to: path) }
      return
    }
    guard let name = Settings.scenario else { return }
    if name == "driven", let driven = screen as? DrivenScreen {
      after(1.5) { checkDriven(driven) }
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
