import CoreGraphics

/*
 * A prepared user bubble, reply or real one-point end marker.
 */
final class ShadowListUtilsAssistantRowLayout: ShadowListUtilsRowLayout {
  enum Content {
    case user(ShadowListUtilsAssistantUserMessage.Layout)
    case assistant(ShadowListUtilsAssistantReplyMessage.Layout)
    case end
  }
  let content: Content

  init(width: CGFloat, height: CGFloat, content: Content) {
    self.content = content
    super.init(width: width, height: height)
  }
}
