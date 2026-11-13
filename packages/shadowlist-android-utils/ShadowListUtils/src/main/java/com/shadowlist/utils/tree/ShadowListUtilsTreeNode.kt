package com.shadowlist.utils

/*
 * A file-tree node. null children is a leaf; an empty list can still name an empty folder.
 */
data class ShadowListUtilsTreeNode(
  val key: String,
  val name: String,
  val children: List<ShadowListUtilsTreeNode>? = null,
  val kind: ShadowListUtilsTreeNodeKind? = null,
)
