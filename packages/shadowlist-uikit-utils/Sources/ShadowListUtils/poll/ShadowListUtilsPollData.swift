/*
 * A poll and the app's selected option. Presses never change this model automatically.
 */
public struct ShadowListUtilsPollData: Hashable {
  public var question: String
  public var options: [ShadowListUtilsPollOption]
  public var selectedKey: String?

  public init(question: String = "", options: [ShadowListUtilsPollOption] = [], selectedKey: String? = nil) {
    self.question = question
    self.options = options
    self.selectedKey = selectedKey
  }
}
