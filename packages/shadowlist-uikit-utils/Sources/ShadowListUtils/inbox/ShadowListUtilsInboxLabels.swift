/*
 * read and unread title the leading swipe action. markRead, markUnread, flag, unflag, delete,
 * reply and select title the menu, and flag, unflag and delete the trailing swipe actions too.
 * selected, unreadState and flaggedState are what a screen reader says of a row's state. time
 * words a message's age.
 */
public struct ShadowListUtilsInboxLabels {
  public var read: String = "Read"
  public var unread: String = "Unread"
  public var markRead: String = "Mark as Read"
  public var markUnread: String = "Mark as Unread"
  public var flag: String = "Flag"
  public var unflag: String = "Unflag"
  public var delete: String = "Delete"
  public var reply: String = "Reply"
  public var select: String = "Select"
  public var selected: String = "Selected"
  public var unreadState: String = "Unread"
  public var flaggedState: String = "Flagged"
  public var time = ShadowListUtilsRelativeTimeLabels()

  public init() {}
}
