/*
 * The existing Assistant callbacks copied into one immutable row snapshot.
 */
struct ShadowListUtilsAssistantHandlers {
  var onCopy: ((String) -> Void)?
  var onCopyCode: ((String) -> Void)?
  var onShare: ((String) -> Void)?
  var onRegenerate: ((String) -> Void)?
  var onRetry: ((String) -> Void)?
  var onSelectVariant: ((String, Int) -> Void)?
  var onFeedback: ((String, ShadowListUtilsAssistantFeedback?) -> Void)?
  var onFollowUp: ((String) -> Void)?
  var onOpenLink: ((String) -> Void)?
  var onEdit: ((String) -> Void)?
}
