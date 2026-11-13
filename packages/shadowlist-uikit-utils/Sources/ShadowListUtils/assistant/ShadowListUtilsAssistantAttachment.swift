import UIKit

/*
 * An image or file supplied by the application.
 */
public struct ShadowListUtilsAssistantAttachment: Hashable {
  public enum Kind: Hashable { case image, file }
  public var key: String
  public var kind: ShadowListUtilsAssistantAttachment.Kind
  public var name: String
  public var detail: String?
  public var uri: String?
  public var color: UIColor?

  public init(
    key: String, kind: ShadowListUtilsAssistantAttachment.Kind, name: String, detail: String? = nil, uri: String? = nil, color: UIColor? = nil
  ) {
    self.key = key
    self.kind = kind
    self.name = name
    self.detail = detail
    self.uri = uri
    self.color = color
  }
}
