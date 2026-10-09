package com.shadowlist;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.HashMap;
import java.util.function.LongConsumer;

/*
 * Frees the native peer of an owner dropped without destroy(). Each owner registers its handle
 * here. Once the owner is collected its peer is freed by the next register() or release().
 */
final class ShadowListPeerReclaimer {
  /*
   * A peer's handle, enqueued once its owner is collected.
   */
  private static final class Entry extends PhantomReference<Object> {
    final long handle;

    Entry(Object owner, long handle, ReferenceQueue<Object> queue) {
      super(owner, queue);
      this.handle = handle;
    }
  }

  private final LongConsumer mFree;
  private final ReferenceQueue<Object> mQueue = new ReferenceQueue<>();

  // Holds the entries reachable until their peer is freed.
  private final HashMap<Long, Entry> mEntries = new HashMap<>();

  ShadowListPeerReclaimer(LongConsumer free) {
    mFree = free;
  }

  /*
   * Track owner's peer and free the peers of owners collected since the last call.
   */
  synchronized void register(Object owner, long handle) {
    drain();
    mEntries.put(handle, new Entry(owner, handle, mQueue));
  }

  /*
   * The owner freed its peer itself. Forget it and free the peers of collected owners.
   */
  synchronized void release(long handle) {
    Entry entry = mEntries.remove(handle);
    if (entry != null) {
      entry.clear();
    }
    drain();
  }

  private void drain() {
    while (true) {
      Entry entry = (Entry) mQueue.poll();
      if (entry == null) {
        return;
      }
      if (mEntries.get(entry.handle) != entry) {
        continue;
      }
      mEntries.remove(entry.handle);
      mFree.accept(entry.handle);
    }
  }
}
