package com.shadowlist.utils

/*
 * A person in a list. Without an avatarColor the avatar takes the palette color of the name.
 */
data class ShadowListUtilsContactItem(
  val key: String,
  val name: String,
  val subtitle: String? = null,
  val avatarUrl: String? = null,
  val avatarColor: Int? = null,
)

/*
 * A group of contacts under one title. key is the section's identity.
 */
data class ShadowListUtilsContactSection(
  val key: String,
  val title: String,
  val items: List<ShadowListUtilsContactItem>,
)
