import Foundation
import os

// A separate module with SwiftPM, and part of this one with CocoaPods.
#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * The words for how long ago something was: now, minutes, hours and days. A moment a week or
 * more back is written as the month and day in the reader's locale.
 */
public struct ShadowListUtilsRelativeTimeLabels {
  public var now: String = "now"
  public var minutes: (Int) -> String = { "\($0)m" }
  public var hours: (Int) -> String = { "\($0)h" }
  public var days: (Int) -> String = { "\($0)d" }

  public init() {}

  /*
   * How long ago dateMs was at nowMs, both in milliseconds since 1970. The core picks the
   * bucket. A date that is not finite gives nothing. Thread safe.
   */
  public func format(dateMs: Double, nowMs: Double) -> String {
    let time = ShadowListUtilsCore.relativeTime(forDate: dateMs, now: nowMs)
    switch time.unit {
    case .now: return now
    case .minutes: return minutes(time.count)
    case .hours: return hours(time.count)
    case .days: return days(time.count)
    case .date: return Self.monthAndDay(dateMs)
    @unknown default: return Self.monthAndDay(dateMs)
    }
  }

  /*
   * The current time in milliseconds since 1970, the unit the item types keep their dates in.
   */
  static var nowMs: Double { Date().timeIntervalSince1970 * 1000 }

  /*
   * The formatter of the month and day, made once per locale.
   */
  private static let dateFormatters = OSAllocatedUnfairLock(uncheckedState: [Locale: DateFormatter]())

  private static func monthAndDay(_ dateMs: Double) -> String {
    guard ShadowListUtilsCore.relativeTimeHasDate(dateMs) else { return "" }
    let locale = Locale.current
    let date = Date(timeIntervalSince1970: dateMs / 1000)
    return dateFormatters.withLockUnchecked { formatters in
      if let formatter = formatters[locale] {
        return formatter.string(from: date)
      }
      let formatter = DateFormatter()
      formatter.locale = locale
      formatter.setLocalizedDateFormatFromTemplate("MMMd")
      formatters[locale] = formatter
      return formatter.string(from: date)
    }
  }
}
