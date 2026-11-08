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
 *   keys     clicks with modifiers, the arrows with and without Shift, Command arrows, Option
 *            page keys, Return, type select to an item not mounted, Escape, emphasis and the
 *            pointer's hover. Logs each check
 *   menu     a right click and a Control-click on items: the menu, clickedIndex, the menu target
 *            and its outline, and their end when the menu closes
 *   reorder  a press dragged short of the slop and past it, the drop spot as the mouse moves, a
 *            drop, the edge scroll on the display link with the row under the mouse, and Escape
 *   sections -SLRoute Sections: the section queries, self-sized rows, the header view, a
 *            scroll to a section, its header pinned and pushed by the next one, overscroll past
 *            the start, separators and a regroup that keeps the visible rows still
 *   animate  the feed with animatesChanges: delete a shown post and add one, log the animation
 *   a11y     what VoiceOver sees: the row count, a far row, scrolling it to visible, a page
 *            scroll through the scroll bar, the selected rows and a post's move and swipe actions
 *   swipe    trackpad swipes across posts, sent as wheel events: a slow swipe past half the
 *            buttons and a short one, a fling, the leading side, a click on an action, a click
 *            elsewhere, a scroll, Escape, a mouse wheel, a full swipe that deletes, the menu of a
 *            post without its own and the swipe actions accessibility runs
 *
 * -SLShot scrolls a little first, which shows the overlay scrollers in the picture. With keys,
 * menu, reorder, sections or swipe it shoots the end state instead: a selection with a hovered
 * item, an open menu's outline, a lifted post with its shadow over the gap it opened, a pinned
 * section header over the rows scrolled under it, or a post swiped open over its actions.
 */
enum Scenario {
  static func start(_ screen: NSView & ListScreen) {
    print("[SLWINDOW] \(screen.window?.windowNumber ?? -1)")
    fflush(stdout)
    if let path = Settings.shotPath, Settings.scenario == nil {
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
    if name == "keys", let feed = screen as? FeedScreen {
      after(1.5) { checkKeys(feed) }
      return
    }
    if name == "menu", let feed = screen as? FeedScreen {
      after(1.5) { checkMenu(feed) }
      return
    }
    if name == "reorder", let feed = screen as? FeedScreen {
      after(1.5) { checkReorder(feed) }
      return
    }
    if name == "sections", let sections = screen as? SectionsScreen {
      after(1.5) { checkSections(sections) }
      return
    }
    if name == "animate", let feed = screen as? FeedScreen {
      after(1.5) { checkAnimation(feed) }
      return
    }
    if name == "a11y", let feed = screen as? FeedScreen {
      after(1.5) { checkAccessibility(feed) }
      return
    }
    if name == "swipe", let feed = screen as? FeedScreen {
      after(1.5) { checkSwipe(feed) }
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
   * Mouse and key events on the feed, one step a frame, each check logged by name. The window
   * is key and the list its first responder, which emphasizes the selection.
   */
  private static func checkKeys(_ screen: FeedScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "keys", "engine": "shadowlist", "screen": "FeedScreen"]
    var checks: [String: Bool] = [:]
    let selection = { Array(list.selectedIndices) }
    func press(_ characters: String, _ code: UInt16, _ flags: NSEvent.ModifierFlags = []) {
      let function: NSEvent.ModifierFlags = characters.unicodeScalars.first.map { $0.value >= 0xF700 } == true ? [.function] : []
      list.shadowListKit_pressKey(characters, keyCode: code, modifierFlags: flags.union(function))
    }
    func visible(_ index: Int) -> Bool {
      guard let cell = list.cellForItem(at: index) else { return false }
      let frame = list.convert(cell.frame, from: cell.superview)
      return frame.minY >= -0.5 && frame.maxY <= list.bounds.height + 0.5
    }
    let down = "\u{F701}"
    let up = "\u{F700}"
    let pageDown = "\u{F72D}"
    let last = screen.keys.count - 1
    var steps: [() -> Void] = [
      {
        list.scrollToStart(animated: false)
        list.shadowListKit_clickItem(at: 2, modifierFlags: [], clickCount: 1)
        checks["clickSelects"] = selection() == [2]
        checks["firstResponder"] = list.window?.firstResponder === list
        checks["emphasized"] = list.cellForItem(at: 2)?.isEmphasized == true
      },
      {
        list.shadowListKit_clickItem(at: 4, modifierFlags: .command, clickCount: 1)
        list.shadowListKit_clickItem(at: 6, modifierFlags: .shift, clickCount: 1)
        checks["commandShiftClick"] = selection() == [2, 4, 5, 6]
      },
      {
        press(down, 125)
        press(down, 125)
        checks["arrows"] = selection() == [8]
        press(down, 125, .shift)
        press(down, 125, .shift)
        press(up, 126, .shift)
        checks["shiftArrows"] = selection() == [8, 9]
      },
      {
        press(down, 125, .command)
        checks["commandDown"] = selection() == [last] && visible(last)
        press(up, 126, .command)
        checks["commandUp"] = selection() == [0] && visible(0)
      },
      {
        press(pageDown, 121, .option)
        let page = selection().first ?? 0
        result["optionPageDownItem"] = page
        checks["optionPageDown"] = selection().count == 1 && page > 2 && visible(page)
      },
      {
        let lead = selection().first ?? -1
        press("\r", 36)
        checks["returnActivates"] = screen.activated == [lead]
      },
      {
        // The name of the item before the lead comes next a whole cycle of names later, past the
        // mounted cells.
        list.shadowListKit_clickItem(at: 1, modifierFlags: [], clickCount: 1)
        let name = screen.name(at: 0)
        let target = (2...last).first { screen.name(at: $0) == name } ?? -1
        result["typeSelectTarget"] = target
        result["typeSelectMountedBefore"] = list.cellForItem(at: target) != nil
        for character in name {
          press(String(character), 0)
        }
        checks["typeSelect"] = selection() == [target] && visible(target)
      },
      {
        press("\u{1B}", 53)
        checks["escapeDeselects"] = selection().isEmpty
        list.scrollToStart(animated: false)
        list.shadowListKit_clickItem(at: 3, modifierFlags: [], clickCount: 1)
        list.shadowListKit_clickItem(at: 5, modifierFlags: .shift, clickCount: 1)
        let cell = list.cellForItem(at: 1)
        let point = cell.map { list.convert(NSPoint(x: $0.frame.midX, y: $0.frame.midY), from: $0.superview) } ?? .zero
        list.shadowListKit_moveMouse(to: point)
        checks["hover"] = cell?.isHovered == true && list.visibleCells.filter { $0.isHovered }.count == 1
        checks["selectedEmphasized"] = selection().allSatisfy { list.cellForItem(at: $0)?.isEmphasized == true }
      },
    ]
    steps.append {
      result["checks"] = checks
      result["pass"] = checks.values.allSatisfy { $0 }
      finish(result, shoot: screen)
    }
    run(steps)
  }

  /*
   * Menus asked the way a right click and a Control-click ask for them, closed the way AppKit
   * ends their tracking.
   */
  private static func checkMenu(_ screen: FeedScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "menu", "engine": "shadowlist", "screen": "FeedScreen"]
    var checks: [String: Bool] = [:]
    var menu: NSMenu?
    let steps: [() -> Void] = [
      {
        list.scrollToStart(animated: false)
        list.shadowListKit_clickItem(at: 1, modifierFlags: [], clickCount: 1)
        menu = list.shadowListKit_menuForItem(at: 2, modifierFlags: [])
        checks["rightClickMenu"] = menu?.title == screen.keys[2]
        checks["clickedIndex"] = list.clickedIndex == 2
        checks["menuTarget"] = list.cellForItem(at: 2)?.isMenuTarget == true
        checks["outline"] = list.shadowListKit_menuOutline?.superlayer === list.cellForItem(at: 2)?.layer
        checks["selectionKept"] = Array(list.selectedIndices) == [1] && list.cellForItem(at: 1)?.isEmphasized == true
      },
      {
        if let menu {
          NotificationCenter.default.post(name: NSMenu.didEndTrackingNotification, object: menu)
        }
        // The chosen item's action runs after tracking ends and still reads the clicked item.
        checks["actionReadsClickedIndex"] = list.clickedIndex == 2
        checks["closedClearsOutline"] = list.shadowListKit_menuOutline == nil && list.cellForItem(at: 2)?.isMenuTarget == false
      },
      {
        checks["closedClearsClickedIndex"] = list.clickedIndex == NSNotFound
      },
      {
        // Left open for the picture.
        menu = list.shadowListKit_menuForItem(at: 0, modifierFlags: .control)
        checks["controlClickMenu"] = menu?.title == screen.keys[0] && list.clickedIndex == 0
      },
      {
        result["checks"] = checks
        result["pass"] = checks.values.allSatisfy { $0 }
        finish(result, shoot: screen)
      },
    ]
    run(steps)
  }

  /*
   * Drags with the mouse's events on the feed, one step a frame. The edge scroll runs on the
   * list's own display link for a few frames. The last drag stays lifted for the picture.
   */
  private static func checkReorder(_ screen: FeedScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "reorder", "engine": "shadowlist", "screen": "FeedScreen"]
    var checks: [String: Bool] = [:]
    var keys: [String] = []
    var start = CGPoint.zero
    var edge = CGPoint.zero
    var offset: CGFloat = 0
    func center(_ index: Int) -> CGPoint {
      guard let cell = list.cellForItem(at: index) else { return .zero }
      let frame = list.convert(cell.frame, from: cell.superview)
      return CGPoint(x: frame.midX, y: frame.midY)
    }
    func shownMidY(_ index: Int) -> CGFloat {
      guard let layer = list.cellForItem(at: index)?.layer else { return .nan }
      return layer.frame.midY - list.contentOffset.y
    }
    var steps: [() -> Void] = [
      {
        list.scrollToStart(animated: false)
        keys = screen.keys
        start = center(2)
        list.shadowListKit_mouseDown(at: start)
        list.shadowListKit_mouseDrag(to: CGPoint(x: start.x, y: start.y + 2))
        checks["belowSlopNoLift"] = list.shadowListKit_heldIndex == NSNotFound
        list.shadowListKit_mouseDrag(to: CGPoint(x: start.x, y: start.y + 4))
        checks["liftsPastSlop"] = list.shadowListKit_heldIndex == 2 && (list.cellForItem(at: 2)?.layer?.shadowOpacity ?? 0) > 0
      },
      {
        let target = center(5)
        list.shadowListKit_mouseDrag(to: CGPoint(x: target.x, y: target.y + 2))
        result["dropIndex"] = list.shadowListKit_dropIndex
        checks["dropSpotFollows"] = list.shadowListKit_dropIndex == 5
        checks["rowsMakeRoom"] = (3...5).allSatisfy { list.cellForItem(at: $0)?.layer.map { $0.transform.m42 < 0 } == true }
        checks["heldUnderMouse"] = abs(shownMidY(2) - (target.y + 2)) < 0.5
      },
      {
        list.shadowListKit_mouseUp(at: center(5))
        checks["dropMoves"] = screen.moves == [[2, 5]] && screen.keys[5] == keys[2]
      },
      {
        keys = screen.keys
        start = center(1)
        edge = CGPoint(x: start.x, y: list.bounds.height - 12)
        offset = list.contentOffset.y
        list.shadowListKit_mouseDown(at: start)
        list.shadowListKit_mouseDrag(to: edge)
      },
    ]
    steps += Array(repeating: {}, count: 20)
    steps += [
      {
        result["autoScrollTravel"] = Double(list.contentOffset.y - offset)
        checks["autoScroll"] = list.contentOffset.y - offset > 40
        checks["heldStaysUnderMouse"] = abs(shownMidY(1) - edge.y) < 0.5 && list.shadowListKit_heldIndex == 1
        list.shadowListKit_pressKey("\u{1B}", keyCode: 53, modifierFlags: [])
        checks["escapePutsBack"] = list.shadowListKit_heldIndex == NSNotFound
      },
      {
        list.shadowListKit_mouseUp(at: edge)
        checks["escapeMovesNothing"] = screen.keys == keys && screen.moves.count == 1
        list.scrollToStart(animated: false)
      },
      {
        // Left lifted for the picture.
        start = center(1)
        let target = center(3)
        list.shadowListKit_mouseDown(at: start)
        list.shadowListKit_mouseDrag(to: CGPoint(x: start.x + 6, y: target.y + 8))
        result["checks"] = checks
        result["pass"] = checks.values.allSatisfy { $0 }
        finish(result, shoot: screen)
      },
    ]
    run(steps)
  }

  /*
   * Run steps one frame apart, each laid out after.
   */
  private static func run(_ steps: [() -> Void]) {
    var index = 0
    Timer.scheduledTimer(withTimeInterval: 1.0 / 60.0, repeats: true) { timer in
      guard index < steps.count else {
        timer.invalidate()
        return
      }
      steps[index]()
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
   * The checks of the UIKit example's sections scenario, and the pinning a Mac list adds: the
   * push by the next header and a rubber band past the start. It ends with a header pinned over
   * rows for the picture.
   */
  private static func checkSections(_ screen: SectionsScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "sections", "engine": "shadowlist", "screen": "SectionsScreen"]
    func layout() {
      for _ in 0..<4 {
        list.needsLayout = true
        list.layoutSubtreeIfNeeded()
      }
    }
    func titleCells() -> [(title: String, cell: ShadowListKitListCell)] {
      list.subviews.first!.subviews.compactMap { view in
        guard let cell = view as? ShadowListKitListCell, !cell.isHidden, cell.index == NSNotFound,
          let title = cell.subviews.compactMap({ ($0 as? NSTextField)?.stringValue }).first
        else { return nil }
        return (title, cell)
      }
    }
    func pinned() -> (title: String, offset: Double)? {
      titleCells().first { $0.cell.layer?.zPosition == 1 }.map { ($0.title, Double(list.convert($0.cell.frame, from: $0.cell.superview).minY)) }
    }
    result["numberOfSections"] = list.numberOfSections
    let third = list.firstItemIndexInSection(3)
    result["sectionOfThirdsFirstItem"] = list.sectionForItem(at: third)
    // Self-sizing: a row with the long note is taller than one without.
    result["selfSizedShort"] = Double(list.rectForItem(at: 1).height)
    result["selfSizedTall"] = Double(list.rectForItem(at: 0).height)
    // The header view comes before the first section.
    result["firstHeaderStart"] = Double(list.rectForHeader(inSection: 0).minY)
    list.scrollToSection(5, animated: false)
    layout()
    result["headerOffsetAfterScrollToSection"] = Double(list.rectForHeader(inSection: 5).minY - list.contentOffset.y)
    // Scroll into the section: its header stays pinned at the top.
    list.contentOffset = CGPoint(x: 0, y: list.contentOffset.y + 120)
    layout()
    result["pinnedHeader"] = pinned()?.title ?? NSNull()
    result["pinnedHeaderOffset"] = pinned()?.offset ?? NSNull()
    // The next header pushes it up.
    list.contentOffset = CGPoint(x: 0, y: list.rectForHeader(inSection: 6).minY - 10)
    layout()
    result["pushedHeaderOffset"] = pinned()?.offset ?? NSNull()
    result["pushedHeaderExpected"] = Double(10 - list.rectForHeader(inSection: 5).height)
    // A rubber band past the start pins nothing and the first header goes with the content.
    list.scrollPhase = .dragging
    list.contentOffset = CGPoint(x: 0, y: -40)
    layout()
    result["pinnedPastStart"] = pinned() != nil
    result["firstHeaderPastStart"] = titleCells().first { $0.title == screen.sections[0].letter }
      .map { Double(list.convert($0.cell.frame, from: $0.cell.superview).minY) } ?? NSNull()
    list.contentOffset = CGPoint(x: 0, y: list.rectForHeader(inSection: 5).minY + 120)
    list.scrollPhase = .idle
    layout()
    result["separators"] = list.visibleCells.filter { cell in
      cell.layer?.sublayers?.contains { $0.zPosition == 1000 && !$0.isHidden } ?? false
    }.count
    // Adding a traveller regroups the sections. Visible rows stay put.
    let before = visibleRows(list)
    screen.addTraveller()
    layout()
    let (shift, _) = maxShift(from: before, to: visibleRows(list))
    result["maxShiftAfterRegroup"] = Double(shift)
    result["pinnedAfterRegroup"] = pinned()?.offset ?? NSNull()
    finish(result, shoot: screen)
  }

  /*
   * Delete the second shown post, which slides the next one up, and add a new post after that
   * one, all through reloadData. Logs the new post's opacity and a survivor's slide while the
   * animation runs, then whether it settled and how far the first shown post moved. The checks
   * and the result are the UIKit example's.
   */
  private static func checkAnimation(_ screen: FeedScreen) {
    let list = screen.list
    list.animatesChanges = true
    func screenY(_ index: Int, middle: Bool = false) -> CGFloat {
      guard let cell = list.cellForItem(at: index) else { return -1 }
      let frame = list.convert(cell.frame, from: cell.superview)
      return middle ? frame.midY : frame.minY
    }
    let first = list.visibleRange.location
    let topBefore = screenY(first)
    let survivorBefore = screenY(first + 2, middle: true)
    screen.replacePost(after: first)
    list.layoutSubtreeIfNeeded()
    after(0.06) {
      let inserted = list.cellForItem(at: first + 2)?.layer
      let survivor = list.cellForItem(at: first + 1)?.layer
      let slide = survivor.flatMap { layer in layer.presentation().map { $0.position.y - layer.position.y } } ?? 0
      let during: [String: Any] = [
        "insertedAlpha": Double(inserted?.presentation()?.opacity ?? -1),
        "survivorSlideY": Double(slide),
        "survivorScreenYBefore": Double(survivorBefore),
        "survivorScreenYAfter": Double(screenY(first + 1, middle: true)),
      ]
      after(0.7) {
        let settled = list.visibleCells.allSatisfy { $0.alphaValue == 1 && ($0.layer?.animationKeys() ?? []).isEmpty }
        let topAfter = screenY(first)
        finish(["scenario": "animate", "during": during, "settled": settled, "firstRowMoved": Double(topAfter - topBefore)])
      }
    }
  }

  /*
   * The UIKit example's checks of what VoiceOver sees, through the Mac's accessibility calls: the
   * row count, a far row that is not mounted, its index, a page scrolled through the scroll bar,
   * then scrolling the far row to visible, selecting it and its move and swipe actions.
   */
  private static func checkAccessibility(_ screen: FeedScreen) {
    let list = screen.list
    let rows = (list.accessibilityRows() ?? []).compactMap { $0 as? NSAccessibilityElement }
    let target = 500
    guard rows.count > target else {
      finish(["scenario": "a11y", "elementCount": rows.count])
      return
    }
    let element = rows[target]
    var result: [String: Any] = ["scenario": "a11y", "elementCount": rows.count]
    result["role"] = list.accessibilityRole()?.rawValue ?? ""
    result["rowCount"] = list.accessibilityRowCount()
    result["elementIsCell"] = element.accessibilityChildren()?.first is ShadowListKitListCell
    result["elementOnScreen"] = NSLocationInRange(target, list.visibleRange)
    result["indexOfElement"] = element.accessibilityIndex()
    let bar = list.shadowListKit_scrollerView.accessibilityChildren()?.first as? NSAccessibilityElement
    let before = list.contentOffset.y
    result["scrollHandled"] = bar?.accessibilityPerformIncrement() ?? false
    result["pageScrolledBy"] = Double(list.contentOffset.y - before)
    result["scrollBarValue"] = bar?.accessibilityValue() as? Double ?? -1
    _ = element.perform(NSSelectorFromString("accessibilityPerformAction:"), with: "AXScrollToVisible")
    result["scrolledToVisible"] = NSLocationInRange(target, list.visibleRange)
    result["elementIsCellAfterScroll"] = element.accessibilityChildren()?.first is ShadowListKitListCell
    element.setAccessibilitySelected(true)
    result["selectedRows"] = (list.accessibilitySelectedRows() ?? []).compactMap { ($0 as? NSAccessibilityElement)?.accessibilityIndex() }
    let actions = (element.accessibilityCustomActions() ?? []).map { $0.name }
    result["moveActions"] = actions.filter { $0.hasPrefix("Move ") }
    result["swipeActions"] = actions.filter { !$0.hasPrefix("Move ") }
    finish(result)
  }

  /*
   * Two finger swipes on the feed as the trackpad sends them, one event a frame, with the
   * settle running on the list's display link between steps. The last swipe stays open for the
   * picture.
   */
  private static func checkSwipe(_ screen: FeedScreen) {
    let list = screen.list
    var result: [String: Any] = ["scenario": "swipe", "engine": "shadowlist", "screen": "FeedScreen"]
    var checks: [String: Bool] = [:]
    func center(_ index: Int) -> CGPoint {
      guard let cell = list.cellForItem(at: index) else { return .zero }
      let frame = list.convert(cell.frame, from: cell.superview)
      return CGPoint(x: frame.midX, y: frame.midY)
    }
    func swipe(_ index: Int, _ deltas: [CGFloat]) {
      let point = center(index)
      list.shadowListKit_swipeWheelDelta(.zero, phase: .began, momentumPhase: [], at: point)
      for delta in deltas {
        list.shadowListKit_swipeWheelDelta(CGPoint(x: delta, y: 0), phase: .changed, momentumPhase: [], at: point)
      }
      list.shadowListKit_swipeWheelDelta(.zero, phase: .ended, momentumPhase: [], at: point)
    }
    func shown() -> [String] {
      (list.shadowListKit_swipeActionsView?.subviews ?? []).compactMap { $0.isHidden ? nil : ($0 as? NSButton)?.title }
    }
    func shownWidth() -> CGFloat {
      (list.shadowListKit_swipeActionsView?.subviews ?? []).filter { !$0.isHidden }.reduce(0) { $0 + $1.frame.width }
    }
    func click(_ point: CGPoint) {
      list.shadowListKit_mouseDown(at: point)
      list.shadowListKit_mouseUp(at: point)
    }
    let settle: [() -> Void] = Array(repeating: {}, count: 24)
    var keys: [String] = []
    var steps: [() -> Void] = [
      {
        list.scrollToStart(animated: false)
        list.shadowListKit_clickItem(at: 0, modifierFlags: [], clickCount: 1)
        swipe(1, Array(repeating: -2, count: 50))
        checks["followsTheFingers"] = list.shadowListKit_swipedIndex == 1 && abs(list.shadowListKit_swipeOffset + 100) < 1
      },
    ]
    steps += settle
    steps += [
      {
        result["openOffset"] = Double(list.shadowListKit_swipeOffset)
        checks["slowSwipeOpens"] = list.shadowListKit_swipedIndex == 1 && abs(list.shadowListKit_swipeOffset + shownWidth()) < 0.5
        checks["trailingButtons"] = shown() == ["Delete", "Flag"]
        // Flag sits inside Delete, at the outer edge.
        click(CGPoint(x: list.bounds.width + list.shadowListKit_swipeOffset + 10, y: center(1).y))
        checks["clickPerforms"] = screen.flagged == [screen.keys[1]]
      }
    ]
    steps += settle
    steps += [
      {
        checks["actionCloses"] = list.shadowListKit_swipedIndex == NSNotFound
        swipe(3, Array(repeating: -2, count: 10))
      }
    ]
    steps += settle
    steps += [
      {
        checks["shortSwipeCloses"] = list.shadowListKit_swipedIndex == NSNotFound
        swipe(3, [-14, -14, -14])
      }
    ]
    steps += settle
    steps += [
      {
        checks["flingOpens"] = list.shadowListKit_swipedIndex == 3 && abs(list.shadowListKit_swipeOffset + shownWidth()) < 0.5
        list.shadowListKit_clickItem(at: 5, modifierFlags: [], clickCount: 1)
        checks["clickElsewhereKeepsSelection"] = Array(list.selectedIndices) == [0]
      }
    ]
    steps += settle
    steps += [
      {
        checks["clickElsewhereCloses"] = list.shadowListKit_swipedIndex == NSNotFound
        swipe(2, Array(repeating: 2, count: 50))
      }
    ]
    steps += settle
    steps += [
      {
        checks["leadingOpens"] = list.shadowListKit_swipedIndex == 2 && list.shadowListKit_swipeOffset > 0 && shown() == ["Pin"]
        list.shadowListKit_pressKey("\u{1B}", keyCode: 53, modifierFlags: [])
      }
    ]
    steps += settle
    steps += [
      {
        checks["escapeCloses"] = list.shadowListKit_swipedIndex == NSNotFound && Array(list.selectedIndices) == [0]
        swipe(3, [-14, -14, -14])
      }
    ]
    steps += settle
    steps += [
      {
        list.shadowListKit_scrollWheelDelta(.zero, phase: .began, momentumPhase: [])
        list.shadowListKit_scrollWheelDelta(CGPoint(x: 0, y: -10), phase: .changed, momentumPhase: [])
        list.shadowListKit_scrollWheelDelta(.zero, phase: .ended, momentumPhase: [])
      }
    ]
    steps += settle
    steps += [
      {
        checks["scrollCloses"] = list.shadowListKit_swipedIndex == NSNotFound
        list.scrollToStart(animated: false)
        let point = center(3)
        for _ in 0..<6 {
          list.shadowListKit_swipeWheelDelta(CGPoint(x: -20, y: 0), phase: [], momentumPhase: [], at: point)
        }
        checks["mouseWheelNeverSwipes"] = list.shadowListKit_swipedIndex == NSNotFound
        keys = screen.keys
        list.shadowListKit_swipeItem(at: 4, distance: -list.bounds.width, velocity: 0)
        checks["fullSwipeDeletes"] = screen.keys.count == keys.count - 1 && !screen.keys.contains(keys[4])
      }
    ]
    steps += settle
    steps += [
      {
        checks["deletedClosed"] = list.shadowListKit_swipedIndex == NSNotFound
        screen.showsMenus = false
        let menu = list.shadowListKit_menuForItem(at: 3, modifierFlags: [])
        let titles = menu?.items.map { $0.isSeparatorItem ? "-" : $0.title } ?? []
        result["menuFallback"] = titles
        let red = menu?.items.first?.attributedTitle?.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor
        checks["menuFallback"] = titles == ["Delete", "Flag", "-", "Pin"] && red == .systemRed
        if let menu {
          NotificationCenter.default.post(name: NSMenu.didEndTrackingNotification, object: menu)
        }
        screen.showsMenus = true
        let row = list.accessibilityRows()?[3] as? NSAccessibilityElement
        let names = (row?.accessibilityCustomActions() ?? []).map(\.name)
        result["accessibilityActions"] = names
        checks["accessibilityActions"] = Array(names.prefix(3)) == ["Delete", "Flag", "Pin"]
        // Left open for the picture.
        swipe(1, Array(repeating: -2, count: 50))
      }
    ]
    steps += settle
    steps += [
      {
        result["checks"] = checks
        result["pass"] = checks.values.allSatisfy { $0 }
        finish(result, shoot: screen)
      }
    ]
    run(steps)
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
   * The window's content as its layers show it, at the backing scale. The presentation layers
   * show what animations hold, like a swiped post moved over its actions.
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
    (screen.layer?.presentation() ?? screen.layer)?.render(in: context.cgContext)
    try! rep.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: path))
    print("[SLSHOT] \(path)")
    fflush(stdout)
    exit(0)
  }

  /*
   * Log the result. With -SLShot the screen is shot after it.
   */
  private static func finish(_ result: [String: Any], shoot screen: NSView? = nil) {
    let data = try! JSONSerialization.data(withJSONObject: result, options: .sortedKeys)
    print("[SLSCENARIO] \(String(data: data, encoding: .utf8)!)")
    fflush(stdout)
    if let screen, let path = Settings.shotPath {
      after(0.2) { shoot(screen, to: path) }
      return
    }
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
