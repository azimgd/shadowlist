package com.shadowlist.kit

/*
 * A source node in depth-first order, or one visible row returned by the shared core.
 */
data class ShadowListKitTreeRow(val key: String, val depth: Int, val hasChildren: Boolean, val expanded: Boolean) {
  init {
    require(depth >= 0) { "Tree depth must not be negative" }
  }
}
