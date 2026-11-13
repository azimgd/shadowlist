package com.shadowlist.utils

import android.text.format.DateFormat
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/*
 * The words for how long ago something was: now, minutes, hours and days. A moment a week or
 * more back is written as the month and day in the reader's locale.
 */
data class ShadowListUtilsRelativeTimeLabels(
  val now: String = "now",
  val minutes: (Int) -> String = { "${it}m" },
  val hours: (Int) -> String = { "${it}h" },
  val days: (Int) -> String = { "${it}d" },
) {
  /*
   * How long ago dateMs was at nowMs, both in milliseconds since 1970. The core picks the
   * bucket. A date that is not finite gives nothing. Thread safe.
   */
  fun format(dateMs: Double, nowMs: Double): String {
    val time = ShadowListUtilsRelativeTime(ShadowListUtilsCore.relativeTime(dateMs, nowMs))
    return when (time.unit) {
      ShadowListUtilsCore.RELATIVE_TIME_NOW -> now
      ShadowListUtilsCore.RELATIVE_TIME_MINUTES -> minutes(time.count)
      ShadowListUtilsCore.RELATIVE_TIME_HOURS -> hours(time.count)
      ShadowListUtilsCore.RELATIVE_TIME_DAYS -> days(time.count)
      else -> monthAndDay(dateMs)
    }
  }

  private companion object {
    /*
     * An abbreviated month and the day, in the order the locale writes them.
     */
    const val DATE_SKELETON = "MMMd"

    /*
     * The formatter of the month and day, made once per locale. A SimpleDateFormat is not
     * thread safe, which the lock is for.
     */
    private var formatterLocale: Locale? = null
    private var formatter: SimpleDateFormat? = null

    @Synchronized
    fun monthAndDay(dateMs: Double): String {
      if (!ShadowListUtilsCore.relativeTimeHasDate(dateMs)) return ""
      val locale = Locale.getDefault()
      val current = formatter?.takeIf { formatterLocale == locale }
        ?: SimpleDateFormat(DateFormat.getBestDateTimePattern(locale, DATE_SKELETON), locale).also {
          formatter = it
          formatterLocale = locale
        }
      return current.format(Date(dateMs.toLong()))
    }
  }
}
