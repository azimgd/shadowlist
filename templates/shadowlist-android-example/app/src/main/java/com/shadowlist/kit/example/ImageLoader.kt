package com.shadowlist.kit.example

import android.content.Context
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Matrix
import android.graphics.Outline
import android.graphics.Paint
import android.os.Handler
import android.os.Looper
import android.util.LruCache
import android.view.View
import android.view.ViewOutlineProvider
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.Executors
import kotlin.math.max

/*
 * Loads remote images off the UI thread, decodes them near the size they show at, and keeps
 * the decoded bitmaps in memory. A view that gets a new URL before the old one arrives drops
 * the old result.
 */
object ImageLoader {
  private val cache = object : LruCache<String, Bitmap>(96 shl 20) {
    override fun sizeOf(key: String, value: Bitmap): Int = value.allocationByteCount
  }
  private val executor = Executors.newFixedThreadPool(4)
  private val main = Handler(Looper.getMainLooper())
  private val waiting = HashMap<String, MutableList<(Bitmap?) -> Unit>>()

  private fun key(url: String, width: Int, height: Int) = "$url#${width}x$height"

  fun cached(url: String, width: Int, height: Int): Bitmap? = cache.get(key(url, width, height))

  /*
   * Calls done on the UI thread with the decoded bitmap, or null on failure.
   */
  fun load(url: String, width: Int, height: Int, done: (Bitmap?) -> Unit) {
    val key = key(url, width, height)
    cache.get(key)?.let { done(it); return }
    waiting[key]?.let { it.add(done); return }
    waiting[key] = mutableListOf(done)
    executor.execute {
      val bitmap = runCatching { download(url, width, height) }.getOrNull()
      main.post { deliver(key, bitmap) }
    }
  }

  private fun deliver(key: String, bitmap: Bitmap?) {
    if (bitmap != null) cache.put(key, bitmap)
    waiting.remove(key)?.forEach { it(bitmap) }
  }

  private fun download(url: String, width: Int, height: Int): Bitmap? {
    val connection = URL(url).openConnection() as HttpURLConnection
    connection.useCaches = true
    val bytes = connection.inputStream.use { it.readBytes() }
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    BitmapFactory.decodeByteArray(bytes, 0, bytes.size, bounds)
    var sample = 1
    while (bounds.outWidth / (sample * 2) >= width && bounds.outHeight / (sample * 2) >= height) sample *= 2
    val options = BitmapFactory.Options().apply { inSampleSize = sample }
    return BitmapFactory.decodeByteArray(bytes, 0, bytes.size, options)?.also { it.prepareToDraw() }
  }
}

/*
 * A view that shows a remote image, center cropped with rounded corners.
 */
class RemoteImageView(context: Context) : View(context) {
  private var url: String? = null
  private var bitmap: Bitmap? = null
  private val matrix = Matrix()
  private val paint = Paint(Paint.FILTER_BITMAP_FLAG)

  var cornerRadius = 0f
    set(value) {
      field = value
      clipToOutline = value > 0
      invalidateOutline()
    }

  init {
    setBackgroundColor(Theme.elevated2)
    outlineProvider = object : ViewOutlineProvider() {
      override fun getOutline(view: View, outline: Outline) {
        outline.setRoundRect(0, 0, view.width, view.height, cornerRadius)
      }
    }
  }

  fun setImage(url: String?, width: Int, height: Int) {
    if (url == this.url) return
    this.url = url
    bitmap = null
    invalidate()
    if (url == null || !Settings.images || width <= 0 || height <= 0) return
    ImageLoader.cached(url, width, height)?.let { bitmap = it; return }
    ImageLoader.load(url, width, height) { loaded ->
      if (this.url == url) {
        bitmap = loaded
        invalidate()
      }
    }
  }

  override fun onDraw(canvas: Canvas) {
    val image = bitmap ?: return
    val scale = max(width / image.width.toFloat(), height / image.height.toFloat())
    matrix.setScale(scale, scale)
    matrix.postTranslate((width - image.width * scale) / 2, (height - image.height * scale) / 2)
    canvas.drawBitmap(image, matrix, paint)
  }
}
