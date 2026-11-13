package com.shadowlist.utils

import android.content.Context
import android.os.SystemClock
import android.view.Menu
import android.view.MotionEvent
import android.view.View
import android.view.View.MeasureSpec
import android.widget.PopupMenu
import androidx.test.platform.app.InstrumentationRegistry
import com.shadowlist.kit.ShadowListKitSwipeAction
import com.shadowlist.kit.ShadowListKitTextLayout
import org.json.JSONArray
import org.json.JSONObject
import org.json.JSONTokener
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue

/*
 * The size every layout test lays a row or a list out at, a phone's.
 */
internal const val TEST_WIDTH_DP = 390f
internal const val TEST_HEIGHT_DP = 800f

internal val testContext: Context get() = InstrumentationRegistry.getInstrumentation().targetContext

internal val testTheme: ShadowListUtilsTheme get() = ShadowListUtilsTheme.LIGHT

/*
 * Runs a test body on the UI thread, where the lists live, and fails the test with what it threw.
 */
internal fun onMain(body: () -> Unit) {
  var failure: Throwable? = null
  InstrumentationRegistry.getInstrumentation().runOnMainSync {
    try {
      body()
    } catch (thrown: Throwable) {
      failure = thrown
    }
  }
  failure?.let { throw it }
}

/*
 * Measures a list and lays it out at a phone's size, the way a screen would. The list is in no
 * window, which loads no image.
 */
internal fun host(list: View, widthPx: Int = TEST_WIDTH_DP.px, heightPx: Int = TEST_HEIGHT_DP.px) {
  list.measure(
    MeasureSpec.makeMeasureSpec(widthPx, MeasureSpec.EXACTLY),
    MeasureSpec.makeMeasureSpec(heightPx, MeasureSpec.EXACTLY))
  list.layout(0, 0, widthPx, heightPx)
}

/*
 * The keys the list shows, in order.
 */
internal val ShadowListUtilsList.keys: List<String> get() = rows.map { it.key }

/*
 * The cell of an item as a row view, or null when the item is not mounted.
 */
internal fun ShadowListUtilsList.rowView(index: Int): ShadowListUtilsRowView? =
  cellForItem(index) as? ShadowListUtilsRowView

/*
 * A tap at a point of an item's cell, by default its center, through the list's touch handling.
 */
internal fun ShadowListUtilsList.tap(index: Int, xInCell: Int? = null, yInCell: Int? = null) {
  val cell = requireNotNull(cellForItem(index)) { "item $index is not mounted" }
  val x = (cell.left - scrollX + (xInCell ?: (cell.width / 2))).toFloat()
  val y = (cell.top - scrollY + (yInCell ?: (cell.height / 2))).toFloat()
  val time = SystemClock.uptimeMillis()
  for (action in intArrayOf(MotionEvent.ACTION_DOWN, MotionEvent.ACTION_UP)) {
    val event = MotionEvent.obtain(time, time, action, x, y, 0)
    dispatchTouchEvent(event)
    event.recycle()
  }
}

/*
 * What the kit tells the list's own delegate when a held item is dropped, and the reload the
 * kit does right after.
 */
internal fun ShadowListUtilsList.drop(sourceIndex: Int, destinationIndex: Int) {
  adapter.moveItem(this, sourceIndex, destinationIndex)
  reloadData()
}

/*
 * What the kit asks the list's own delegate when an item is swiped or held.
 */
internal fun ShadowListUtilsList.leadingActions(index: Int): List<ShadowListKitSwipeAction> =
  adapter.leadingSwipeActionsForItem(this, index)?.actions.orEmpty()

internal fun ShadowListUtilsList.trailingActions(index: Int): List<ShadowListKitSwipeAction> =
  adapter.trailingSwipeActionsForItem(this, index)?.actions.orEmpty()

internal fun ShadowListUtilsList.menu(index: Int): Menu? {
  val menu = PopupMenu(context, this).menu
  return if (adapter.contextMenuForItem(this, index, menu)) menu else null
}

/*
 * Runs the action the way a press on its button does.
 */
internal fun ShadowListKitSwipeAction.perform() = handler(this) {}

/*
 * A text layout is as tall as its lines: line breaking differs per platform, the line height
 * does not.
 */
internal fun assertLines(layout: ShadowListKitTextLayout?, style: ShadowListUtilsTextStyle, maxLines: Int = Int.MAX_VALUE) {
  val text = requireNotNull(layout) { "no text layout" }
  assertTrue("${text.lineCount} lines, at most $maxLines expected", text.lineCount in 1..maxLines)
  assertEquals(text.lineCount * style.scaledLineHeightPx(), text.height)
}

/*
 * Runs a body with the library's text size override set, then clears it.
 */
internal fun <Result> withTextSize(scale: Float, body: () -> Result): Result {
  ShadowListUtilsPlatform.textScaleOverride = scale
  try {
    return body()
  } finally {
    ShadowListUtilsPlatform.textScaleOverride = null
  }
}

/*
 * Runs a body with the library's layout direction override set, then clears it.
 */
internal fun <Result> withRightToLeft(body: () -> Result): Result {
  ShadowListUtilsPlatform.isRightToLeftOverride = true
  try {
    return body()
  } finally {
    ShadowListUtilsPlatform.isRightToLeftOverride = null
  }
}

/*
 * A row that counts how often its view was filled, by key.
 */
internal class CountingRow(
  override val key: String,
  override val contentVersion: Int = 0,
  val height: Float = 44f,
) : ShadowListUtilsRow {
  override val reuseIdentifier: String get() = "CountingRow"

  override fun makeView(context: Context): ShadowListUtilsRowView = CountingRowView(context, reuseIdentifier)

  override fun layout(widthPx: Int): ShadowListUtilsRowLayout = ShadowListUtilsRowLayout(widthPx, height.px)
}

internal class CountingRowView(context: Context, reuseIdentifier: String) :
  ShadowListUtilsRowView(context, reuseIdentifier) {
  override fun fill(row: ShadowListUtilsRow) {
    fills[row.key] = (fills[row.key] ?: 0) + 1
  }

  companion object {
    val fills = HashMap<String, Int>()
  }
}

/*
 * The vector files the JS, C++ and native tests share, which the test APK carries as assets.
 */
internal object Vectors {
  /*
   * The parsed file: a JSONArray or a JSONObject.
   */
  fun read(name: String): Any {
    val assets = InstrumentationRegistry.getInstrumentation().context.assets
    val text = assets.open(name).use { it.readBytes().toString(Charsets.UTF_8) }
    return JSONTokener(text).nextValue()
  }

  fun cases(name: String): List<JSONObject> {
    val parsed = read(name) as? JSONArray ?: throw AssertionError("$name is a list of cases")
    return List(parsed.length()) { parsed.getJSONObject(it) }
  }
}

/*
 * Fixtures the template tests share.
 */
internal object Fixture {
  fun contact(index: Int) =
    ShadowListUtilsContactItem(key = "contact-$index", name = "Person $index", subtitle = "Seat $index")

  fun contacts(count: Int): List<ShadowListUtilsContactItem> = List(count, ::contact)
}
