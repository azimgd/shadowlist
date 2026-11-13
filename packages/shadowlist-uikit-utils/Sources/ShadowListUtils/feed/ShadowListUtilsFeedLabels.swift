/*
 * time words a post's age. image is what a screen reader says for an image without an alt text,
 * and loading for a placeholder row.
 */
public struct ShadowListUtilsFeedLabels {
  public var time = ShadowListUtilsRelativeTimeLabels()
  public var image: String = "Image"
  public var loading: String = "Loading post"

  public init() {}
}
