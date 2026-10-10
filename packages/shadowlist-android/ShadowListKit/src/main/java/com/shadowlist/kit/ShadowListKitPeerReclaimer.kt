package com.shadowlist.kit

import java.lang.ref.PhantomReference
import java.lang.ref.ReferenceQueue

/*
 * Frees the native peer of an owner dropped without destroy(), like a list that loaded data but
 * never attached to a window. Each owner registers its handle here. Once the owner is collected
 * its peer is freed by the next register() or release(). Every call runs on the UI thread.
 */
internal class ShadowListKitPeerReclaimer(private val free: (handle: Long) -> Unit) {
  /*
   * A peer's handle, enqueued once its owner is collected.
   */
  private class Entry(owner: Any, val handle: Long, queue: ReferenceQueue<Any>) : PhantomReference<Any>(owner, queue)

  private val queue = ReferenceQueue<Any>()

  /*
   * Holds the entries reachable until their peer is freed.
   */
  private val entries = HashMap<Long, Entry>()

  /*
   * Track owner's peer and free the peers of owners collected since the last call.
   */
  fun register(owner: Any, handle: Long) {
    drain()
    entries[handle] = Entry(owner, handle, queue)
  }

  /*
   * The owner freed its peer itself. Forget it and free the peers of collected owners.
   */
  fun release(handle: Long) {
    entries.remove(handle)?.clear()
    drain()
  }

  private fun drain() {
    while (true) {
      val entry = queue.poll() as? Entry ?: return
      if (entries[entry.handle] !== entry) continue
      entries.remove(entry.handle)
      free(entry.handle)
    }
  }
}
