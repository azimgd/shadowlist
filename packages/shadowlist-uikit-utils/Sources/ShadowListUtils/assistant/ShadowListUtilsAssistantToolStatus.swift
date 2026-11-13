/*
 * A stopped tool is not a failed tool. Only done and failed carry output.
 */
public enum ShadowListUtilsAssistantToolStatus: Hashable {
  case running
  case done(output: String)
  case failed(output: String? = nil)
  case stopped
}
