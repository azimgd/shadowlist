package com.shadowlist.utils

/*
 * The person whose activity the row shows. Without avatarColor the name picks its palette color.
 */
data class ShadowListUtilsActivityActor(val name: String, val avatarUrl: String? = null, val avatarColor: Int? = null)
