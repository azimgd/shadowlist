package com.shadowlist.utils

/*
 * A visible node's metadata and its expansion action, the same values a TreeList row renders.
 */
data class ShadowListUtilsTreeListRenderItemInfo(
  val item: ShadowListUtilsTreeNode,
  val depth: Int,
  val indent: Float,
  val isExpanded: Boolean,
  val hasChildren: Boolean,
  val toggle: () -> Unit,
)
