#pragma once

#include "FuzzHelpers.hpp"
#include "KitHostHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/host/ListDriver.hpp>
#include <shadowlist-core/host/ListUpdate.hpp>
#include <shadowlist-core/host/ScrollTarget.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

/*
 * What the kit fuzz tests do to a KitHost and check after: data changes through every call an
 * app can make them with, scroll frames, user scrolls and scroll commands.
 */
namespace slt {

using namespace azimgd::shadowlist;

/*
 * The seeds a kit fuzz test runs by default.
 */
inline constexpr std::uint32_t KIT_FUZZ_SEEDS = 120;

/*
 * The rows overlapping the viewport, low to high.
 */
inline std::vector<ScreenRow> visibleRows(const KitHost& host) {
  std::vector<ScreenRow> rows;
  host.forEachRowOnScreen([&](std::size_t index, double start) {
    rows.push_back({host.driver.getKeyAt(index), start - host.offset});
    return true;
  });
  return rows;
}

inline double screenOf(const KitHost& host, const std::string& key) {
  std::size_t index = host.driver.indexOfKey(key);
  if (index == UNDEFINED_INDEX) {
    return std::nan("");
  }
  return host.driver.getLeadingAt(index) - host.offset;
}

inline bool atEnd(const KitHost& host) {
  return std::fabs(host.offset - host.maxOffset()) <= FUZZ_TOLERANCE;
}

/*
 * Whether a hold of the row at screen was clamped to an edge of the scroll range.
 */
inline bool isHoldClamped(const KitHost& host, const std::string& key, double screen) {
  double wanted = host.driver.getLeadingAt(host.driver.indexOfKey(key)) - screen;
  return isHoldClamped(wanted, host.offset, host.maxOffset());
}

/*
 * Bring the list to rest: no gesture, no correction waiting, the offset inside the range, every
 * row in its place, and one more frame changes nothing. After a change of the rows the driver's
 * keys are the data source's rows, and every key is found at its row.
 */
inline void rest(KitHost& host) {
  host.phase = ScrollPhase::Idle;
  host.tracking = false;
  host.needsFrame = true;
  host.layoutPass();
  if (host.settle() < 0) {
    host.failWith("correction never settled at rest");
  }
  if (host.offset < -FUZZ_TOLERANCE || host.offset > host.maxOffset() + FUZZ_TOLERANCE) {
    host.failWith(
      "rests outside the range: off=" + std::to_string(host.offset) + " max=" + std::to_string(host.maxOffset()));
  }
  std::string geometry = host.checkGeometry(true);
  if (!geometry.empty()) {
    host.failWith("geometry at rest: " + geometry);
  }
  if (host.structureVersion != host.checkedStructure) {
    host.checkedStructure = host.structureVersion;
    const std::vector<std::string>& keys = host.driver.getKeys();
    if (host.sectionModel.empty() ? keys != host.items : keys != host.readRowKeys()) {
      host.failWith("the driver's keys are not the data source's rows");
    }
    for (std::size_t index = 0; index < keys.size(); ++index) {
      if (host.driver.indexOfKey(keys[index]) != index) {
        host.failWith("key " + keys[index] + " is not found at its row " + std::to_string(index));
      }
    }
  }
  double before = host.offset;
  host.needsFrame = true;
  host.layoutPass();
  if (host.settleScheduled || std::fabs(host.offset - before) > 0.01) {
    host.failWith(
      "a frame at rest moved the list from " + std::to_string(before) + " to " + std::to_string(host.offset));
  }
}

/*
 * Open a fresh list the way an app does: the data is read before the view is in a window, and
 * the first layout runs with its bounds.
 */
inline void open(KitHost& host, std::size_t count) {
  host.items = host.makeKeys(count);
  for (KitSection& section : host.sectionModel) {
    section.itemCount = 0;
  }
  if (!host.sectionModel.empty()) {
    for (std::size_t item = 0; item < count; ++item) {
      ++host.sectionModel[item * host.sectionModel.size() / count].itemCount;
    }
  }
  host.note(
    "open n=" + std::to_string(count) + (host.settings.inverted ? " inverted" : "") +
    (host.settings.horizontal ? " horizontal" : "") + " cols=" + std::to_string(host.getColumns()) +
    " sections=" + std::to_string(host.sectionModel.size()));
  host.reloadData();
  host.inWindow = true;
  rest(host);
}

/*
 * A change of the data source's items the way UIKit names it: deleted, reloaded and move sources
 * in the items before, inserted and move destinations in the items after.
 */
struct Edit {
  std::string what;
  std::vector<std::string> next;
  BatchUpdate update;

  /*
   * Rows the change moved or resized. The reader's row is not expected to hold when it is one.
   */
  std::unordered_set<std::string> unstable;
  bool appendOnly = false;
};

inline Edit insertEdit(KitHost& host, std::size_t at, std::size_t count) {
  Edit edit;
  edit.next = host.items;
  at = std::min(at, edit.next.size());
  edit.appendOnly = at == edit.next.size();
  std::vector<std::string> made = host.makeKeys(count);
  edit.next.insert(edit.next.begin() + static_cast<std::ptrdiff_t>(at), made.begin(), made.end());
  for (std::size_t index = 0; index < count; ++index) {
    edit.update.inserted.push_back(at + index);
  }
  edit.what = "insert " + std::to_string(count) + " at " + std::to_string(at);
  return edit;
}

inline Edit deleteEdit(KitHost& host, std::size_t at, std::size_t count) {
  Edit edit;
  edit.next = host.items;
  at = std::min(at, edit.next.size() - 1);
  count = std::min(count, edit.next.size() - at);
  edit.next.erase(
    edit.next.begin() + static_cast<std::ptrdiff_t>(at), edit.next.begin() + static_cast<std::ptrdiff_t>(at + count));
  for (std::size_t index = 0; index < count; ++index) {
    edit.update.deleted.push_back(at + index);
  }
  edit.what = "delete " + std::to_string(count) + " at " + std::to_string(at);
  return edit;
}

/*
 * One row taken out at from and put back at to in the items after, like moveItem.
 */
inline Edit moveEdit(KitHost& host, std::size_t from, std::size_t to) {
  Edit edit;
  edit.next = host.items;
  std::string key = edit.next[from];
  edit.next.erase(edit.next.begin() + static_cast<std::ptrdiff_t>(from));
  to = std::min(to, edit.next.size());
  edit.next.insert(edit.next.begin() + static_cast<std::ptrdiff_t>(to), key);
  edit.update.moved.push_back({from, to});
  edit.unstable.insert(key);
  edit.what = "move " + std::to_string(from) + " to " + std::to_string(to);
  return edit;
}

/*
 * The row at drop leaves along with a prepend, like a pull to refresh.
 */
inline Edit refreshEdit(KitHost& host, std::size_t drop, std::size_t count) {
  Edit edit;
  edit.next = host.items;
  edit.next.erase(edit.next.begin() + static_cast<std::ptrdiff_t>(drop));
  std::vector<std::string> made = host.makeKeys(count);
  edit.next.insert(edit.next.begin(), made.begin(), made.end());
  edit.update.deleted.push_back(drop);
  for (std::size_t index = 0; index < count; ++index) {
    edit.update.inserted.push_back(index);
  }
  edit.what = "refresh dropping " + std::to_string(drop) + " for " + std::to_string(count);
  return edit;
}

/*
 * Rows deleted and inserted at scattered places in one change.
 */
inline Edit scatterEdit(KitHost& host, std::size_t deletes, std::size_t inserts) {
  Edit edit;
  std::size_t size = host.items.size();
  std::unordered_set<std::size_t> gone;
  for (std::size_t pick = 0; pick < deletes && size > 0; ++pick) {
    gone.insert(pickSize(host.rng, 0, size - 1));
  }
  for (std::size_t index = 0; index < size; ++index) {
    if (gone.count(index) > 0) {
      edit.update.deleted.push_back(index);
    } else {
      edit.next.push_back(host.items[index]);
    }
  }
  std::unordered_set<std::size_t> come;
  for (std::size_t pick = 0; pick < inserts; ++pick) {
    come.insert(pickSize(host.rng, 0, edit.next.size() + inserts - 1));
  }
  std::vector<std::size_t> places(come.begin(), come.end());
  std::sort(places.begin(), places.end());
  // Ascending places in the items after: each new row lands where its index says, or at the end.
  for (std::size_t place : places) {
    std::size_t index = std::min(place, edit.next.size());
    edit.next.insert(edit.next.begin() + static_cast<std::ptrdiff_t>(index), host.makeKeys(1).front());
    edit.update.inserted.push_back(index);
  }
  edit.what = "scatter " + std::to_string(edit.update.deleted.size()) + " out and " +
              std::to_string(edit.update.inserted.size()) + " in";
  return edit;
}

/*
 * Rows whose content changed under the same key and measure differently now.
 */
inline Edit resizeEdit(KitHost& host, std::size_t at, std::size_t count) {
  Edit edit;
  edit.next = host.items;
  at = std::min(at, edit.next.size() - 1);
  count = std::min(count, edit.next.size() - at);
  for (std::size_t index = at; index < at + count; ++index) {
    const std::string& key = edit.next[index];
    host.trueSizes[key] = pickRowSize(host.rng);
    ++host.versions[key];
    edit.update.reloaded.push_back(index);
    edit.unstable.insert(key);
  }
  edit.what = "resize " + std::to_string(count) + " at " + std::to_string(at);
  return edit;
}

/*
 * The calls an app can hand a change to the kit with.
 */
enum class EditRoute { OwnCall, Batch, Diff, Reload };

inline constexpr const char* EDIT_ROUTE_NAMES[] = {"own call", "batch", "diff", "reload"};

/*
 * Hand an edit to the kit through one of its routes, and count the route as a kind. A single
 * insert, delete or reload has its own call. Everything goes through a batch and through a diff.
 * A row that changed under its key is not seen by reloadData.
 */
inline void applyEdit(KitHost& host, const Edit& edit) {
  const BatchUpdate& update = edit.update;
  int parts = (update.deleted.empty() ? 0 : 1) + (update.inserted.empty() ? 0 : 1) + (update.reloaded.empty() ? 0 : 1) +
              (update.moved.empty() ? 0 : 1);
  bool single = parts == 1 && update.moved.empty();
  std::vector<EditRoute> routes = {EditRoute::Batch, EditRoute::Diff};
  if (single) {
    routes.push_back(EditRoute::OwnCall);
  }
  if (update.reloaded.empty()) {
    routes.push_back(EditRoute::Reload);
  }
  EditRoute route = routes[pickSize(host.rng, 0, routes.size() - 1)];
  std::string through = std::string("through ") + EDIT_ROUTE_NAMES[static_cast<std::size_t>(route)];
  host.items = edit.next;
  // An emptied list starts over.
  if (edit.next.empty()) {
    host.leftBottom = false;
  }
  host.episode.reach(through);
  host.note(
    edit.what + " " + through + " n=" + std::to_string(edit.next.size()) + " off=" + std::to_string(host.offset));
  switch (route) {
    case EditRoute::OwnCall:
      if (!update.inserted.empty()) {
        host.insertItems(update.inserted);
      } else if (!update.deleted.empty()) {
        host.deleteItems(update.deleted);
      } else {
        host.reloadItems(update.reloaded);
      }
      break;
    case EditRoute::Batch:
      host.performBatchUpdates(update);
      break;
    case EditRoute::Diff:
      host.applyChanges();
      break;
    case EditRoute::Reload:
      host.reloadData();
      break;
  }
  if (host.driver.getKeyCount() != (host.sectionModel.empty() ? host.items.size() : host.sections.getRowCount())) {
    host.failWith(edit.what + ": the driver's keys are not as many as the data source's rows");
  }
}

/*
 * Apply a data change while the list rests and check that the first row on screen stays put
 * once the layout it runs in is done. Rows the change deleted fall through to the next one.
 * A list with columns holds one track. There one of the rows on screen has to stay.
 */
inline void changeAndHold(KitHost& host, const Edit& edit) {
  std::vector<ScreenRow> before = visibleRows(host);
  bool wasAtBottom = host.settings.inverted && host.offset >= host.maxOffset() - INVERTED_FOLLOW_BAND;
  bool followsToBottom = wasAtBottom && !host.leftBottom && host.settings.followAppends && edit.appendOnly;

  applyEdit(host, edit);
  int frames = host.settle();
  if (frames < 0) {
    host.failWith(edit.what + ": correction never settled");
  }
  rest(host);

  if (followsToBottom) {
    if (!atEnd(host)) {
      host.failWith(
        edit.what + ": followAppends did not reach the bottom, off=" + std::to_string(host.offset) +
        " max=" + std::to_string(host.maxOffset()));
    }
    return;
  }
  /*
   * An inverted list resting on its newest row holds the bottom edge instead of the row.
   */
  if (wasAtBottom && atEnd(host)) {
    return;
  }
  std::string moved;
  for (const ScreenRow& row : before) {
    double after = screenOf(host, row.key);
    // A row the list never holds the content by leaves that to the next one.
    if (std::isnan(after) || host.nonAnchorKeys.count(row.key) > 0) {
      continue;
    }
    if (edit.unstable.count(row.key) > 0) {
      return;
    }
    if (std::fabs(after - row.screen) <= FUZZ_TOLERANCE) {
      return;
    }
    if (isHoldClamped(host, row.key, row.screen)) {
      return;
    }
    double wanted = host.driver.getLeadingAt(host.driver.indexOfKey(row.key)) - row.screen;
    if (moved.empty()) {
      moved = "row " + row.key + " moved from " + std::to_string(row.screen) + " to " + std::to_string(after) +
              " (wanted offset " + std::to_string(wanted) + ", host " + std::to_string(host.offset) + ", max " +
              std::to_string(host.maxOffset()) + ", frames " + std::to_string(frames) + ")";
    }
    if (host.getColumns() == 1) {
      break;
    }
  }
  if (!moved.empty()) {
    host.failWith(edit.what + ": " + moved);
  }
}

/*
 * The scroll view moves the list by delta under a finger or its momentum and the frame is laid
 * out. A row on screen before and after the frame is where the scroll left it: the pass corrects
 * before the frame is drawn. With past set the finger holds the list beyond an edge.
 */
inline void scrollFrame(KitHost& host, double delta, ScrollPhase phase, bool past = false) {
  double previous = host.offset;
  double next = host.offset + delta;
  if (!past) {
    next = std::min(std::max(next, 0.0), host.maxOffset());
  }
  /*
   * Only a row the reader saw last frame whose leading edge is on screen in this one can be seen
   * to jump. A list with columns holds one track. A frame that travels further than the overscan,
   * or follows one that left the core with rows to measure, shows rows nobody measured yet, and
   * the rows after one of those move when it is.
   */
  ScreenRow held;
  bool measuredAhead = std::fabs(next - previous) <= host.windowAlong * host.settings.overscan && !host.band.isEmpty();
  for (const ScreenRow& row : visibleRows(host)) {
    double screen = row.screen - (next - previous);
    if (screen >= 0.0 && screen < host.windowAlong && host.getColumns() == 1 && measuredAhead) {
      held = {row.key, screen};
      break;
    }
  }
  double endBefore = host.maxOffset();
  host.offset = next;
  host.phase = phase;
  host.tracking = phase == ScrollPhase::Dragging;
  host.layoutPass();
  if (host.settings.inverted && host.contentAlong > host.windowAlong) {
    if (host.offset < host.maxOffset() - INVERTED_FOLLOW_BAND) {
      host.leftBottom = true;
    } else if (next - previous >= OFFSET_MOVED_THRESHOLD && next >= endBefore - FUZZ_TOLERANCE && atEnd(host)) {
      host.leftBottom = false;
    }
  }
  if (held.key.empty() || past) {
    return;
  }
  bool atEdge = host.offset <= FUZZ_TOLERANCE || atEnd(host) || next <= 0.0 || next >= host.maxOffset();
  double after = screenOf(host, held.key);
  if (!atEdge && std::fabs(after - held.screen) > FUZZ_TOLERANCE) {
    host.failWith(
      "row " + held.key + " jumped from " + std::to_string(held.screen) + " to " + std::to_string(after) +
      " in a scroll frame");
  }
}

/*
 * A finger lands, which drops the landing of an animated command on its way.
 */
inline void beginDrag(KitHost& host) {
  host.driver.cancelLanding();
  host.phase = ScrollPhase::Dragging;
  host.tracking = true;
}

/*
 * A user scroll to a resting offset, reported as a drag and then an idle frame.
 */
inline void scrollTo(KitHost& host, double target) {
  target = std::min(std::max(target, 0.0), host.maxOffset());
  host.note("scroll to " + std::to_string(target));
  beginDrag(host);
  double travel = target - host.offset;
  for (int step = 0; step < 3; ++step) {
    scrollFrame(host, travel / 3.0, ScrollPhase::Dragging);
  }
  // Rows measured on the way moved the end.
  target = std::min(std::max(target, 0.0), host.maxOffset());
  scrollFrame(host, target - host.offset, ScrollPhase::Settling);
  rest(host);
}

/*
 * Where a scroll to a row rests, see landingOffset.
 */
inline double landingOf(const KitHost& host, std::size_t row, double viewPosition) {
  return landingOffset(
    host.driver.getLeadingAt(row), host.driver.getExtentAt(row), viewPosition, host.windowAlong, host.maxOffset());
}

/*
 * Run a command the way the kit does: stop any motion, send it to the driver and lay out now.
 * Animated, the scroll view first eases to the driver's estimate with no finger down, then the
 * landing sends the command.
 */
template <typename Send>
inline void runCommand(KitHost& host, bool animated, double estimate, const ScrollLanding& landing, const Send& send) {
  host.phase = ScrollPhase::Idle;
  host.tracking = false;
  if (!animated) {
    host.driver.cancelLanding();
    send();
    host.needsFrame = true;
    host.layoutPass();
  } else {
    host.driver.setLanding(landing);
    double from = host.offset;
    for (int frame = 1; frame <= 4; ++frame) {
      host.offset = from + (estimate - from) * frame / 4.0;
      host.layoutPass();
    }
    if (!host.driver.land()) {
      host.failWith("the landing of an animated command was gone when its animation ended");
    }
    host.needsFrame = true;
    host.layoutPass();
  }
  if (host.settle() < 0) {
    host.failWith("command never settled");
  }
  rest(host);
  if (host.driver.land()) {
    host.failWith("a landing was sent twice");
  }
}

inline void scrollToItem(KitHost& host, std::size_t item, double viewPosition, bool animated) {
  std::size_t row = host.sections.rowForItem(item);
  std::string key = host.driver.getKeyAt(row);
  host.note(
    std::string(animated ? "animated " : "") + "scrollToItem " + std::to_string(item) + " pos " +
    std::to_string(viewPosition));
  double estimate = host.driver.animatedTargetOffset(row, viewPosition, host.windowAlong, host.maxOffset());
  runCommand(host, animated, estimate, {ScrollLanding::Target::Row, row, viewPosition}, [&] {
    host.driver.scrollToRow(row, viewPosition);
  });
  std::size_t landed = host.driver.indexOfKey(key);
  double wanted = landingOf(host, landed, viewPosition);
  if (std::fabs(host.offset - wanted) > FUZZ_TOLERANCE) {
    host.failWith(
      "scrollToItem landed at " + std::to_string(host.offset) + " wanted " + std::to_string(wanted) + " (row at " +
      std::to_string(host.driver.getLeadingAt(landed)) + ", size " + std::to_string(host.driver.getExtentAt(landed)) +
      ")");
  }
}

inline void scrollToEnd(KitHost& host, bool animated) {
  host.note(std::string(animated ? "animated " : "") + "scrollToEnd");
  double from = host.offset;
  runCommand(
    host, animated, host.maxOffset(), {ScrollLanding::Target::End, 0, 0.0}, [&] { host.driver.scrollToEnd(); });
  if (!atEnd(host)) {
    host.failWith("scrollToEnd landed at " + std::to_string(host.offset) + " max " + std::to_string(host.maxOffset()));
  }
  // A command that moved the list onto its newest row has the reader follow it again.
  if (host.offset - from >= OFFSET_MOVED_THRESHOLD) {
    host.leftBottom = false;
  }
}

inline void scrollToStart(KitHost& host, bool animated) {
  host.note(std::string(animated ? "animated " : "") + "scrollToStart");
  runCommand(host, animated, 0.0, {ScrollLanding::Target::Start, 0, 0.0}, [&] { host.driver.scrollToStart(); });
  if (host.offset > FUZZ_TOLERANCE) {
    host.failWith("scrollToStart landed at " + std::to_string(host.offset));
  }
}

}
