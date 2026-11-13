package com.shadowlist.utils

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.drawable.BitmapDrawable
import android.net.http.HttpResponseCache
import android.os.Handler
import android.os.Looper
import android.util.LruCache
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.util.WeakHashMap
import java.util.concurrent.Executors
import java.util.concurrent.Future

/*
 * Loads the images the templates show. The loader the library starts with fetches each url
 * once, decodes it off the UI thread at the size its view shows it at, and keeps the decoded
 * images in memory. An app that has its own image stack subclasses this, overrides load and
 * cancel, and sets shared. The loader the library starts with needs the INTERNET permission.
 */
open class ShadowListUtilsImageLoader {
  /*
   * One fetch and the views that wait for it. connection is set on the fetch's thread and
   * closed from the UI thread when nobody waits any more.
   */
  private class Waiting {
    val views = ArrayList<ShadowListUtilsImageView>()
    var task: Future<*>? = null

    @Volatile
    var connection: HttpURLConnection? = null

    @Volatile
    var cancelled = false
  }

  private val cache = object : LruCache<String, Bitmap>(DECODED_CAPACITY) {
    override fun sizeOf(key: String, value: Bitmap): Int = value.allocationByteCount
  }
  private val executor = Executors.newFixedThreadPool(FETCH_THREADS) { work ->
    Thread(work, "ShadowListUtilsImage").apply { isDaemon = true }
  }
  private val closer = Executors.newSingleThreadExecutor { work ->
    Thread(work, "ShadowListUtilsImageClose").apply { isDaemon = true }
  }
  private val main = Handler(Looper.getMainLooper())

  /*
   * The fetch of each image, and the image each view waits for. Both are only touched on the
   * UI thread. A view that went away takes its entry with it.
   */
  private val waiting = HashMap<String, Waiting>()
  private val waitingKeys = WeakHashMap<ShadowListUtilsImageView, String>()

  /*
   * Sets the view's image when the image arrives, unless the view's url changed meanwhile.
   * Called on the UI thread, once the view has its size.
   */
  open fun load(url: String, into: ShadowListUtilsImageView) {
    val width = into.width
    val height = into.height
    val key = "$url#${width}x$height"
    cache.get(key)?.let {
      show(into, it)
      return
    }
    waitingKeys[into] = key
    waiting[key]?.let {
      it.views.add(into)
      return
    }
    val entry = Waiting()
    entry.views.add(into)
    waiting[key] = entry
    val context = into.context.applicationContext
    entry.task = executor.submit {
      val bitmap = runCatching { fetch(context, url, width, height, entry) }.getOrNull()
      main.post { finish(key, entry, bitmap) }
    }
  }

  /*
   * Ends what the view waits for. A fetch nobody waits for any more is aborted. Called on the
   * UI thread.
   */
  open fun cancel(view: ShadowListUtilsImageView) {
    val key = waitingKeys.remove(view) ?: return
    val entry = waiting[key] ?: return
    entry.views.remove(view)
    if (entry.views.isNotEmpty()) return
    waiting.remove(key)
    entry.cancelled = true
    entry.task?.cancel(true)
    // Closing the connection ends a read that is blocked on the network.
    entry.connection?.let { connection -> closer.execute { connection.disconnect() } }
  }

  /*
   * Keep the decoded image and hand it to every view still waiting for this url.
   */
  private fun finish(key: String, entry: Waiting, bitmap: Bitmap?) {
    if (waiting[key] !== entry) return
    waiting.remove(key)
    if (bitmap != null) cache.put(key, bitmap)
    for (view in entry.views) {
      if (waitingKeys[view] != key) continue
      waitingKeys.remove(view)
      if (bitmap != null) show(view, bitmap)
    }
  }

  private fun show(view: ShadowListUtilsImageView, bitmap: Bitmap) {
    view.image = BitmapDrawable(view.resources, bitmap)
  }

  private fun fetch(context: Context, url: String, width: Int, height: Int, entry: Waiting): Bitmap? {
    installHttpCache(context)
    if (entry.cancelled) return null
    val connection = URL(url).openConnection() as HttpURLConnection
    entry.connection = connection
    try {
      connection.connectTimeout = TIMEOUT_MS
      connection.readTimeout = TIMEOUT_MS
      // A response on disk is good however old it is, like an image that never changes.
      connection.addRequestProperty("Cache-Control", "max-stale=$MAX_STALE_SECONDS")
      val bytes = connection.inputStream.use { it.readBytes() }
      if (entry.cancelled) return null
      val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
      BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
      var sample = 1
      while (bounds.outWidth / (sample * 2) >= width && bounds.outHeight / (sample * 2) >= height) sample *= 2
      val options = BitmapFactory.Options().apply { inSampleSize = sample }
      return BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)?.also { it.prepareToDraw() }
    } finally {
      entry.connection = null
      connection.disconnect()
    }
  }

  companion object {
    /*
     * The loader every image view of the library uses.
     */
    @JvmStatic
    @Volatile
    var shared: ShadowListUtilsImageLoader = ShadowListUtilsImageLoader()

    private const val DECODED_CAPACITY = 96 shl 20
    private const val DISK_CAPACITY = 512L shl 20
    private const val DISK_FOLDER = "shadowlist-utils-images"
    private const val FETCH_THREADS = 4
    private const val TIMEOUT_MS = 15_000
    private const val MAX_STALE_SECONDS = Int.MAX_VALUE

    /*
     * Fetched responses are kept on disk by the platform's HTTP cache, which serves every
     * HttpURLConnection of the process. An app that installed its own keeps it.
     */
    @Synchronized
    private fun installHttpCache(context: Context) {
      if (HttpResponseCache.getInstalled() != null) return
      runCatching { HttpResponseCache.install(File(context.cacheDir, DISK_FOLDER), DISK_CAPACITY) }
    }
  }
}
