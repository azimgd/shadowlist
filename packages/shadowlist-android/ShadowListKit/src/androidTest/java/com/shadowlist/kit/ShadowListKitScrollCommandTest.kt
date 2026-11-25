package com.shadowlist.kit

import android.content.Intent
import android.view.ViewTreeObserver
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListKitScrollCommandTest {
  private class Rows :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing {
    override fun numberOfItems(listView: ShadowListKitListView): Int = 1000
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = "row-$index"
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 60 + index % 4 * 17
  }

  /*
   * Run the real animation in an attached window. Its completion requests the landing layout;
   * sample on the following pre-draw, after that layout, rather than timing a sleep.
   */
  private fun assertCentered(horizontal: Boolean) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    val landed = CountDownLatch(1)
    val error = AtomicReference<Double>()
    try {
      instrumentation.runOnMainSync {
        val list = ShadowListKitListView(activity)
        list.horizontal = horizontal
        list.setPadding(40, 40, 40, 40)
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = Rows()
        list.delegate = object : ShadowListKitListView.Delegate {
          override fun didEndScrollingAnimation(listView: ShadowListKitListView) {
            list.viewTreeObserver.addOnPreDrawListener(object : ViewTreeObserver.OnPreDrawListener {
              override fun onPreDraw(): Boolean {
                list.viewTreeObserver.removeOnPreDrawListener(this)
                val row = list.rectForItem(300)!!
                val window = if (horizontal) {
                  list.width - list.paddingLeft - list.paddingRight
                } else {
                  list.height - list.paddingTop - list.paddingBottom
                }
                val center = if (horizontal) row.centerX() else row.centerY()
                error.set(center - (list.exactOffset + window / 2.0))
                landed.countDown()
                return true
              }
            })
          }
        }
        activity.setContentView(
          list,
          FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
        )
        list.reloadData()
        list.addOnLayoutChangeListener(object : android.view.View.OnLayoutChangeListener {
          override fun onLayoutChange(
            view: android.view.View,
            left: Int,
            top: Int,
            right: Int,
            bottom: Int,
            oldLeft: Int,
            oldTop: Int,
            oldRight: Int,
            oldBottom: Int
          ) {
            list.removeOnLayoutChangeListener(this)
            list.scrollToItem(300, 0.5, animated = true)
          }
        })
      }
      assertTrue("animated command must land in its actual window", landed.await(10, TimeUnit.SECONDS))
      assertEquals("row center must equal the padded viewport center", 0.0, error.get(), 1.0)
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test fun animated_vertical_centering_uses_the_padded_viewport() = assertCentered(false)

  @Test fun animated_horizontal_centering_uses_the_padded_viewport() = assertCentered(true)
}
