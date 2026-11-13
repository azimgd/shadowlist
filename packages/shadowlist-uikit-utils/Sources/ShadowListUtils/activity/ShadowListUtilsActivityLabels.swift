/*
 * unread is spoken before an unread activity. time writes the activity's age.
 */
public struct ShadowListUtilsActivityLabels {
  public var unread: String = "Unread"
  public var time = ShadowListUtilsRelativeTimeLabels()

  public init() {}
}
