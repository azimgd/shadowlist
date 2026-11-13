package com.shadowlist.utils

/*
 * read and unread title the leading swipe action. markRead, markUnread, flag, unflag, delete,
 * reply and select title the menu, and flag, unflag and delete the trailing swipe actions too.
 * selected, unreadState and flaggedState are what a screen reader says of a row's state. time
 * words a message's age.
 */
data class ShadowListUtilsInboxLabels(
  val read: String = "Read",
  val unread: String = "Unread",
  val markRead: String = "Mark as Read",
  val markUnread: String = "Mark as Unread",
  val flag: String = "Flag",
  val unflag: String = "Unflag",
  val delete: String = "Delete",
  val reply: String = "Reply",
  val select: String = "Select",
  val selected: String = "Selected",
  val unreadState: String = "Unread",
  val flaggedState: String = "Flagged",
  val time: ShadowListUtilsRelativeTimeLabels = ShadowListUtilsRelativeTimeLabels(),
)
