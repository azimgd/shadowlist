/*
 * image is what a screen reader says for an image. placeholder and send are the composer's.
 * sending is the hint of a message on its way, failed the line under a message that did not
 * arrive, and retryHint what pressing that line does.
 */
public struct ShadowListUtilsChatLabels: Hashable {
  public var image: String = "Image"
  public var placeholder: String = "Message"
  public var send: String = "Send"
  public var sending: String = "Sending"
  public var failed: String = "Not delivered. Tap to retry."
  public var retryHint: String = "Sends the message again"

  public init() {}
}
