package com.shadowlist.kit.example

import android.content.Intent

/*
 * The list implementation a screen runs on, picked with --es SLEngine.
 */
enum class Engine(val id: String, val title: String) {
  // SLKListView with sizes from the precomputed layouts.
  SHADOWLIST("sl", "SLKListView"),
  // SLKListView measuring each row through its cell on the UI thread.
  SHADOWLIST_AUTO("sl-auto", "SLKListView (self-sizing)"),
  // RecyclerView with every row's height from the same precomputed layouts.
  RECYCLER("recycler", "RecyclerView"),
  // RecyclerView with wrap_content rows measured on the UI thread.
  RECYCLER_AUTO("recycler-auto", "RecyclerView (self-sizing)");

  val isShadowList: Boolean get() = this == SHADOWLIST || this == SHADOWLIST_AUTO
  val selfSizing: Boolean get() = this == SHADOWLIST_AUTO || this == RECYCLER_AUTO

  companion object {
    fun of(id: String?): Engine = entries.firstOrNull { it.id == id } ?: SHADOWLIST
  }
}

/*
 * Launch extras, the same names the iOS and React Native examples read:
 * am start --es SLRoute Feed --es SLCount 1000 --es SLEngine sl.
 */
object Settings {
  var route: String? = null
    private set
  var engine: Engine = Engine.SHADOWLIST
    private set
  var count: Int = 1000
    private set
  var images: Boolean = true
    private set

  // SLPadding N: list padding in dp, kept clear of rows at rest like content insets.
  var padding: Int = 0
    private set

  // SLAnimate 1: SLKListView animates inserts and deletes.
  var animate: Boolean = false
    private set

  fun read(intent: Intent) {
    route = intent.getStringExtra("SLRoute")
    engine = Engine.of(intent.getStringExtra("SLEngine"))
    count = intent.getStringExtra("SLCount")?.toIntOrNull()?.takeIf { it > 0 } ?: 1000
    images = intent.getStringExtra("SLImages") != "0"
    padding = intent.getStringExtra("SLPadding")?.toIntOrNull()?.coerceAtLeast(0) ?: 0
    animate = intent.getStringExtra("SLAnimate") == "1"
  }
}
