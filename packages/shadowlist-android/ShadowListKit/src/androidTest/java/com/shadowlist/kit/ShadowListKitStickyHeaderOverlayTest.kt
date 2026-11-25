package com.shadowlist.kit

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.view.View
import android.widget.Button
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class ShadowListKitStickyHeaderOverlayTest {
  private open class Rows :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing {
    val keys = MutableList(20) { "item-$it" }
    var title = "Custom header"
    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 40
  }
  private class EmptySection :
    Rows(),
    ShadowListKitListView.Sections {
    init {
      keys.clear()
    }
    override fun numberOfSections(listView: ShadowListKitListView): Int = 1
    override fun numberOfItemsInSection(listView: ShadowListKitListView, section: Int): Int = 0
    override fun titleForHeaderInSection(listView: ShadowListKitListView, section: Int): String = "Empty section"
    override fun sizeForHeaderInSection(listView: ShadowListKitListView, section: Int, crossSize: Int): Int = 40
  }
  private class OverlayButton(context: Context) : Button(context) {
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
      setMeasuredDimension(
        if (MeasureSpec.getMode(widthMeasureSpec) ==
          MeasureSpec.EXACTLY
        ) {
          MeasureSpec.getSize(widthMeasureSpec)
        } else {
          60
        },
        if (MeasureSpec.getMode(heightMeasureSpec) ==
          MeasureSpec.EXACTLY
        ) {
          MeasureSpec.getSize(heightMeasureSpec)
        } else {
          60
        }
      )
    }
  }
  private val instrumentation = InstrumentationRegistry.getInstrumentation()
  private fun fixture(source: Rows = Rows(), body: (Activity, ShadowListKitListView, Rows) -> Unit) {
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    lateinit var list: ShadowListKitListView
    try {
      instrumentation.runOnMainSync {
        list = ShadowListKitListView(activity)
        list.registerCell("row") { ShadowListKitListCell(it, "row") }
        list.dataSource = source
        list.stickyIndices = intArrayOf(0, 3)
        val root = FrameLayout(activity)
        root.addView(list, FrameLayout.LayoutParams(200, 100))
        activity.setContentView(root)
        list.reloadData()
      }
      instrumentation.waitForIdleSync()
      instrumentation.runOnMainSync { body(activity, list, source) }
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test fun identity_survives_scroll_and_width_and_custom_extent_controls_push_off() = fixture { activity, list, _ ->
    val made = mutableListOf<OverlayButton>()
    val original = list.cellForItem(0)!!
    list.renderStickyHeaderOverlay = { key, index, section ->
      assertEquals("item-0", key)
      assertEquals(0, index)
      assertEquals(-1, section)
      OverlayButton(activity).also { made.add(it) }
    }
    list.layoutPass()
    val button = made[0]
    assertEquals(View.INVISIBLE, original.visibility)
    list.writeOffset(20)
    list.layoutPass()
    assertEquals(20, button.top)
    list.layoutParams = FrameLayout.LayoutParams(300, 100)
    list.measure(
      View.MeasureSpec.makeMeasureSpec(300, View.MeasureSpec.EXACTLY),
      View.MeasureSpec.makeMeasureSpec(100, View.MeasureSpec.EXACTLY)
    )
    list.layout(0, 0, 300, 100)
    list.layoutPass()
    assertEquals(1, made.size)
    assertSame(button, made[0])
    assertEquals(300, button.width)
    list.writeOffset(100)
    list.layoutPass()
    assertEquals(60, button.top)
    list.renderStickyHeaderOverlay = null
    list.layoutPass()
    assertNull(button.parent)
    assertEquals(View.VISIBLE, original.visibility)
  }

  @Test fun payload_active_replacement_and_detach_remove_the_old_view() = fixture { activity, list, source ->
    val made = mutableListOf<OverlayButton>()
    list.renderStickyHeaderOverlay = { key, index, _ ->
      assertEquals(source.keys[index], key)
      OverlayButton(activity).also {
        it.text = source.title + ":" + key
        made.add(it)
      }
    }
    list.layoutPass()
    val first = made[0]
    source.title = "Updated"
    list.reloadItems(intArrayOf(0), "title")
    list.layoutPass()
    assertNull(first.parent)
    assertEquals("Updated:item-0", made.last().text.toString())
    val payloadView = made.last()
    java.util.Collections.swap(source.keys, 0, 1)
    list.reloadData()
    list.layoutPass()
    assertNull(payloadView.parent)
    assertEquals("Updated:item-1", made.last().text.toString())
    val updated = made.last()
    list.writeOffset(120)
    list.layoutPass()
    assertNull(updated.parent)
    assertEquals("Updated:item-3", made.last().text.toString())
    val active = made.last()
    (list.parent as FrameLayout).removeView(list)
    assertNull(active.parent)
    assertNull(list.liveCore)
  }

  @Test fun interaction_and_accessibility_use_the_overlay_and_nil_restores_the_original() = fixture {
      activity,
      list,
      _
    ->
    val button = OverlayButton(activity)
    button.contentDescription = "Custom section action"
    var presses = 0
    button.setOnClickListener { presses++ }
    val original = list.cellForItem(0)!!
    list.renderStickyHeaderOverlay = { _, _, _ -> button }
    list.layoutPass()
    assertEquals(View.INVISIBLE, original.visibility)
    assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS, original.importantForAccessibility)
    val children = arrayListOf<View>()
    list.addChildrenForAccessibility(children)
    assertTrue(children.contains(button))
    assertFalse(children.contains(original))
    assertNull(list.cellAt(20f, 20f))
    button.performClick()
    assertEquals(1, presses)
    list.renderStickyHeaderOverlay = { _, _, _ -> null }
    list.layoutPass()
    assertEquals(View.VISIBLE, original.visibility)
    assertNull(button.parent)
  }

  @Test fun anonymous_empty_section_uses_nil_key_and_the_public_section_index() = fixture(EmptySection()) {
      activity,
      list,
      _
    ->
    list.stickyIndices = intArrayOf()
    list.stickySectionHeaders = true
    var calls = 0
    list.renderStickyHeaderOverlay = { key, index, section ->
      calls++
      assertNull(key)
      assertEquals(-1, index)
      assertEquals(0, section)
      OverlayButton(activity)
    }
    list.layoutPass()
    assertEquals(1, calls)
  }

  @Test fun horizontal_overlay_uses_the_existing_leading_axis_and_cross_size() = fixture { activity, list, _ ->
    list.horizontal = true
    list.layoutPass()
    val button = OverlayButton(activity)
    list.renderStickyHeaderOverlay = { _, _, _ -> button }
    list.layoutPass()
    assertEquals(60, button.width)
    assertEquals(100, button.height)
    list.writeOffset(100)
    list.layoutPass()
    assertEquals(60, button.left)
  }

  @Test fun factory_detachment_does_not_attach_its_returned_view_after_cleanup() = fixture { activity, list, _ ->
    val button = OverlayButton(activity)
    var calls = 0
    list.renderStickyHeaderOverlay = { _, _, _ ->
      calls++
      (list.parent as FrameLayout).removeView(list)
      button
    }
    list.layoutPass()
    assertEquals(1, calls)
    assertNull(button.parent)
    assertFalse(list.isAttachedToWindow)
    assertNull(list.liveCore)
  }

  @Test fun factory_can_clear_itself_without_landing_an_obsolete_view() = fixture { activity, list, _ ->
    val original = list.cellForItem(0)!!
    val button = OverlayButton(activity)
    list.renderStickyHeaderOverlay = { _, _, _ ->
      list.renderStickyHeaderOverlay = null
      button
    }
    list.layoutPass()
    assertNull(button.parent)
    assertEquals(View.VISIBLE, original.visibility)
  }
}
