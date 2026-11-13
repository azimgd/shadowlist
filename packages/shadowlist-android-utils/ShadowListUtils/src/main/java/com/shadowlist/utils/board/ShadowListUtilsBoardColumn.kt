package com.shadowlist.utils

/*
 * One column's stable key, title and application-owned items.
 */
data class ShadowListUtilsBoardColumn<Item>(val key: String, val title: String, val items: List<Item>)
