package com.shadowlist.kit

import android.animation.ValueAnimator
import android.content.Intent
import android.view.View
import android.widget.FrameLayout
import androidx.test.platform.app.InstrumentationRegistry
import com.shadowlist.kit.ShadowListKitDefaultItemAnimator.Batch
import com.shadowlist.kit.ShadowListKitDefaultItemAnimator.Batch.Event
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

/*
 * A batch's completion runs once the animations the batch started ended, heard from the
 * animator rather than timed.
 */
class ShadowListKitBatchCompletionTest {
  @Test
  fun a_batch_waits_for_every_registered_animation_even_before_the_first_frame() {
    var completions = 0
    val batch = Batch { ++completions }
    val first = Any()
    val second = Any()
    batch.transition(Event.Started(first))
    batch.transition(Event.Started(second))
    batch.transition(Event.LayoutFinished)
    assertEquals(0, completions)
    batch.transition(Event.Ended(first))
    assertEquals(0, completions)
    batch.transition(Event.Ended(first))
    assertEquals("a duplicate end cannot finish another animation", 0, completions)
    batch.transition(Event.Ended(second))
    assertEquals(1, completions)
    batch.transition(Event.LayoutFinished)
    batch.transition(Event.Ended(second))
    assertEquals("completion is delivered once", 1, completions)
  }

  @Test
  fun animations_ending_during_layout_do_not_finish_a_batch_still_collecting() {
    var completions = 0
    val batch = Batch { ++completions }
    val first = Any()
    val second = Any()
    batch.transition(Event.Started(first))
    batch.transition(Event.Ended(first))
    assertEquals(0, completions)
    batch.transition(Event.Started(second))
    batch.transition(Event.LayoutFinished)
    assertEquals(0, completions)
    batch.transition(Event.Ended(second))
    assertEquals(1, completions)
  }

  @Test
  fun a_batch_with_no_remaining_animations_finishes_when_layout_finishes() {
    var completions = 0
    val empty = Batch { ++completions }
    empty.transition(Event.LayoutFinished)
    val ended = Batch { ++completions }
    val animation = Any()
    ended.transition(Event.Started(animation))
    ended.transition(Event.Ended(animation))
    ended.transition(Event.LayoutFinished)
    assertEquals(2, completions)
  }

  @Test
  fun a_discarded_batch_never_completes() {
    var completions = 0
    val batch = Batch { ++completions }
    val animation = Any()
    batch.transition(Event.Started(animation))
    batch.transition(Event.Discarded)
    batch.transition(Event.Ended(animation))
    batch.transition(Event.LayoutFinished)
    assertEquals(0, completions)
  }

  @Test
  fun overlapping_batches_do_not_wait_for_each_others_animations() {
    val completed = ArrayList<Int>()
    val first = Batch { completed.add(1) }
    val second = Batch { completed.add(2) }
    val firstAnimation = Any()
    val secondAnimation = Any()
    first.transition(Event.Started(firstAnimation))
    first.transition(Event.LayoutFinished)
    second.transition(Event.Started(secondAnimation))
    second.transition(Event.LayoutFinished)
    second.transition(Event.Ended(secondAnimation))
    assertEquals(listOf(2), completed)
    first.transition(Event.Ended(firstAnimation))
    assertEquals(listOf(2, 1), completed)
  }

  @Test
  fun cancelling_and_replacing_an_animation_in_the_same_turn_completes_each_batch_once() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    val completed = ArrayList<Int>()
    try {
      instrumentation.runOnMainSync {
        val list = ShadowListKitListView(activity)
        activity.setContentView(list)
        val animator = ShadowListKitDefaultItemAnimator()
        val cell = ShadowListKitListCell(activity, "row")
        val first = animator.beginBatch(list) { completed.add(1) }
        animator.animateInsert(list, cell)
        animator.endBatch(first)
        val second = animator.beginBatch(list) { completed.add(2) }
        animator.animateMove(list, cell, 0f, 120f)
        animator.endBatch(second)
        cell.animate().cancel()
        cell.animate().cancel()
        assertEquals("cancellation completes in the animation callback", listOf(1, 2), completed)
      }
      instrumentation.waitForIdleSync()
      assertEquals(listOf(1, 2), completed)
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test
  fun a_detached_list_completes_when_its_animation_ends_or_is_cancelled() {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    try {
      instrumentation.runOnMainSync {
        val root = FrameLayout(activity)
        activity.setContentView(root)
        for (cancel in listOf(false, true)) {
          val list = ShadowListKitListView(activity)
          val cell = ShadowListKitListCell(activity, "row")
          root.addView(list)
          list.addView(cell)
          assertTrue(list.isAttachedToWindow)
          var animation: ValueAnimator? = null
          cell.animate().setUpdateListener { animation = it }
          val animator = ShadowListKitDefaultItemAnimator()
          var completions = 0
          val batch = animator.beginBatch(list) {
            assertFalse("completion does not need the list to reattach", list.isAttachedToWindow)
            ++completions
          }
          animator.animateInsert(list, cell)
          animator.endBatch(batch)
          val running = requireNotNull(animation)
          assertEquals(0, completions)
          root.removeView(list)
          assertFalse(list.isAttachedToWindow)
          if (cancel) running.cancel() else running.end()
          assertEquals("completion runs before the animation callback returns", 1, completions)
          running.cancel()
          assertEquals("completion runs once", 1, completions)
        }
      }
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  private class Rows :
    ShadowListKitListView.DataSource,
    ShadowListKitListView.Sizing {
    val keys = MutableList(30) { "row-$it" }
    override fun numberOfItems(listView: ShadowListKitListView): Int = keys.size
    override fun keyForItem(listView: ShadowListKitListView, index: Int): String = keys[index]
    override fun cellForItem(listView: ShadowListKitListView, index: Int): ShadowListKitListCell =
      listView.dequeueReusableCell("row")
    override fun sizeForItem(listView: ShadowListKitListView, index: Int, crossSize: Int): Int = 120
  }

  private fun runBatch(
    animated: Boolean,
    nested: Boolean = false,
    afterReturn: (ShadowListKitListView) -> Unit = {},
    check: (ShadowListKitListView, Boolean) -> Unit
  ) {
    val instrumentation = InstrumentationRegistry.getInstrumentation()
    val context = instrumentation.context
    val activity = instrumentation.startActivitySync(
      Intent().setClassName(
        context.packageName,
        ShadowListKitBoardTestActivity::class.java.name
      ).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
    )
    val done = CountDownLatch(1)
    val finished = AtomicBoolean(false)
    try {
      instrumentation.runOnMainSync {
        val view = ShadowListKitListView(activity)
        view.animatesChanges = animated
        view.registerCell("row") { ShadowListKitListCell(it, "row") }
        val rows = Rows()
        view.dataSource = rows
        activity.setContentView(
          view,
          FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT)
        )
        view.reloadData()
        view.addOnLayoutChangeListener(object : View.OnLayoutChangeListener {
          override fun onLayoutChange(
            v: View,
            left: Int,
            top: Int,
            right: Int,
            bottom: Int,
            oldLeft: Int,
            oldTop: Int,
            oldRight: Int,
            oldBottom: Int
          ) {
            view.removeOnLayoutChangeListener(this)
            view.post {
              val updates = {
                rows.keys.removeAt(2)
                rows.keys.add(1, "new-row")
                view.deleteItems(intArrayOf(2))
                view.insertItems(intArrayOf(1))
              }
              view.performBatchUpdates({
                if (nested) view.performBatchUpdates(updates) { check(view, it) } else updates()
              }) { result ->
                finished.set(result)
                check(view, result)
                done.countDown()
              }
              afterReturn(view)
            }
          }
        })
      }
      assertTrue("the completion must run", done.await(10, TimeUnit.SECONDS))
      assertTrue(finished.get())
    } finally {
      instrumentation.runOnMainSync { activity.finish() }
      instrumentation.waitForIdleSync()
    }
  }

  @Test
  fun an_animated_batch_completes_after_its_animations_end() {
    val running = AtomicInteger(-1)
    runBatch(animated = true) { list, _ ->
      var count = 0
      for (index in 0 until list.childCount) {
        val child = list.getChildAt(index)
        if (child is ShadowListKitListCell && child.hasTransientState()) ++count
        if (child is ShadowListKitListCell) assertEquals("every cell is back at full opacity", 1f, child.alpha, 0.001f)
      }
      running.set(count)
    }
    assertEquals("no cell is still animating at the completion", 0, running.get())
  }

  @Test
  fun a_nested_batch_waits_for_the_animations_started_by_the_outer_commit() {
    val completions = AtomicInteger(0)
    runBatch(animated = true, nested = true) { list, result ->
      assertTrue(result)
      for (index in 0 until list.childCount) {
        val cell = list.getChildAt(index) as? ShadowListKitListCell ?: continue
        assertEquals("insertions have finished fading in", 1f, cell.alpha, 0.001f)
        assertEquals("moves have finished", 0f, cell.translationY, 0.001f)
      }
      completions.incrementAndGet()
    }
    assertEquals("both batches complete", 2, completions.get())
  }

  @Test
  fun the_outer_batch_has_applied_its_layout_when_performBatchUpdates_returns() {
    val checked = AtomicBoolean(false)
    runBatch(animated = true, nested = true, afterReturn = { list ->
      val first = requireNotNull(list.cellForItem(0))
      val inserted = requireNotNull(list.cellForItem(1))
      assertSame("the inserted row is mounted", list.mounted["new-row"], inserted)
      assertEquals("the inserted row is laid out", first.bottom, inserted.top)
      assertFalse("the deleted row has left the mounted rows", list.mounted.containsKey("row-2"))
      checked.set(true)
    }) { _, result -> assertTrue(result) }
    assertTrue(checked.get())
  }

  @Test
  fun a_batch_without_animations_completes_on_the_next_turn() {
    val inserted = AtomicBoolean(false)
    runBatch(animated = false) { list, _ ->
      inserted.set(
        (0 until list.childCount).any {
          (list.getChildAt(it) as? ShadowListKitListCell)?.visibility ==
            View.VISIBLE
        }
      )
    }
    assertTrue(inserted.get())
  }
}
