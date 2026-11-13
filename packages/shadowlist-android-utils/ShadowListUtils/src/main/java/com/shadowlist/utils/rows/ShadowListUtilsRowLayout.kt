package com.shadowlist.utils

/*
 * The frames of one row at one width, in pixels. Subclasses add the frames and the typeset text
 * of their row. A layout never changes once made, which lets a background thread hand it to the
 * UI thread.
 */
open class ShadowListUtilsRowLayout(val widthPx: Int, val heightPx: Int)
