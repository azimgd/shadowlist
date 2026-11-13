package com.shadowlist.utils

import android.graphics.drawable.Drawable

/*
 * One poll option. key stays the same when its vote count changes.
 */
data class ShadowListUtilsPollOption(val key: String, val label: String, val votes: Double, val icon: Drawable? = null)
