/*
 * The RN turn states. Done and failed own only their applicable result fields.
 */
public enum ShadowListUtilsAssistantTurnStatus: Hashable {
  case streaming
  case done(sources: [ShadowListUtilsAssistantSource] = [], followUps: [String] = [])
  case stopped
  case failed(error: String? = nil)
}
