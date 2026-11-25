package com.shadowlist.kit

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.view.ViewPropertyAnimator

/*
 * Animates items for animatesChanges. The list works out which items were inserted, deleted and
 * moved, places them and calls these to animate. animateDelete must call completion when it
 * ends, which gives the cell back to the reuse pool. animateMove gets a cell already at its new
 * place, and fromX and fromY are where it showed before, relative to that place.
 */
interface ShadowListKitItemAnimator {
  fun animateInsert(listView: ShadowListKitListView, cell: ShadowListKitListCell)
  fun animateDelete(listView: ShadowListKitListView, cell: ShadowListKitListCell, completion: () -> Unit)
  fun animateMove(listView: ShadowListKitListView, cell: ShadowListKitListCell, fromX: Float, fromY: Float)
}

/*
 * The animator a list starts with: inserted items fade in, deleted items fade out and items
 * that stay slide, for durationMs.
 */
open class ShadowListKitDefaultItemAnimator : ShadowListKitItemAnimator {
  companion object {
    val CHANGE_DURATION_MS: Long get() = ShadowListKitCore.CHANGE_DURATION_MS
  }

  var durationMs = CHANGE_DURATION_MS

  /*
   * Only batches still collecting animations are held here. Each batch owns its completion
   * and the identities it waits for, even when several batches share a layout pass.
   */
  private val batches = HashMap<Batch, ShadowListKitListView>()

  internal class Batch(private val completion: () -> Unit) {
    private sealed interface State {
      class Collecting(val animations: MutableSet<Any> = HashSet()) : State
      class Waiting(val animations: MutableSet<Any>) : State
      data object Complete : State
    }

    sealed interface Event {
      class Started(val animation: Any) : Event
      class Ended(val animation: Any) : Event
      data object LayoutFinished : Event
      data object Discarded : Event
    }

    private var state: State = State.Collecting()

    fun transition(event: Event) {
      val current = state
      if (current == State.Complete) return
      if (event == Event.Discarded) {
        state = State.Complete
        return
      }
      when (current) {
        is State.Collecting -> when (event) {
          is Event.Started -> current.animations.add(event.animation)
          is Event.Ended -> current.animations.remove(event.animation)
          Event.LayoutFinished -> state = State.Waiting(current.animations)
          Event.Discarded -> Unit
        }
        is State.Waiting -> if (event is Event.Ended) current.animations.remove(event.animation)
        State.Complete -> Unit
      }
      val waiting = state as? State.Waiting ?: return
      if (waiting.animations.isNotEmpty()) return
      state = State.Complete
      completion()
    }
  }

  internal fun beginBatch(list: ShadowListKitListView, completion: () -> Unit): Batch {
    val batch = Batch(completion)
    batches[batch] = list
    return batch
  }

  internal fun stopCollecting(batch: Batch) {
    batches.remove(batch)
  }

  internal fun endBatch(batch: Batch) {
    stopCollecting(batch)
    batch.transition(Batch.Event.LayoutFinished)
  }

  internal fun discardBatch(batch: Batch) {
    batches.remove(batch)
    batch.transition(Batch.Event.Discarded)
  }

  override fun animateInsert(listView: ShadowListKitListView, cell: ShadowListKitListCell) {
    cell.animate().cancel()
    cell.translationX = 0f
    cell.translationY = 0f
    cell.alpha = 0f
    start(listView, cell.animate().alpha(1f).setDuration(durationMs))
  }

  override fun animateDelete(listView: ShadowListKitListView, cell: ShadowListKitListCell, completion: () -> Unit) {
    start(listView, cell.animate().alpha(0f).setDuration(durationMs).withEndAction(completion))
  }

  override fun animateMove(listView: ShadowListKitListView, cell: ShadowListKitListCell, fromX: Float, fromY: Float) {
    if (fromX == 0f && fromY == 0f && cell.translationX == 0f && cell.translationY == 0f) return
    cell.animate().cancel()
    cell.translationX = fromX
    cell.translationY = fromY
    start(listView, cell.animate().translationX(0f).translationY(0f).alpha(1f).setDuration(durationMs))
  }

  /*
   * Register before starting, then listen to the actual animation. Reusing a cell's property
   * animator cannot replace that listener. Cancellation also delivers onAnimationEnd.
   */
  private fun start(list: ShadowListKitListView, animator: ViewPropertyAnimator) {
    val owners = batches.filterValues { it === list }.keys
    if (owners.isEmpty()) {
      animator.start()
      return
    }
    val token = Any()
    for (batch in owners) batch.transition(Batch.Event.Started(token))
    animator.setListener(object : AnimatorListenerAdapter() {
      override fun onAnimationStart(animation: Animator) {
        animator.setListener(null)
        animation.addListener(this)
      }

      override fun onAnimationEnd(animation: Animator) {
        animation.removeListener(this)
        for (batch in owners) batch.transition(Batch.Event.Ended(token))
      }
    })
    animator.start()
  }
}
