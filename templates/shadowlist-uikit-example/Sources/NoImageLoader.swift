import ShadowListUtils

/*
 * Leaves every image view on its placeholder color.
 */
final class NoImageLoader: ShadowListUtilsImageLoader {
  override func load(_ url: String, into view: ShadowListUtilsImageView) {}
  override func cancel(_ view: ShadowListUtilsImageView) {}
}
