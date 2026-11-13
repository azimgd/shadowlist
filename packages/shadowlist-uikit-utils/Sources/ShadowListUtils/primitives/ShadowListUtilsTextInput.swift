import UIKit

/*
 * The editing owner shared by native fields and composers. App values and uncontrolled drafts
 * use the existing ChatInput contract. Native marked text remains owned by the input method.
 */
final class ShadowListUtilsTextInput: NSObject, UITextFieldDelegate, UITextViewDelegate {
  enum Input {
    case textField(UITextField)
    case textView(UITextView)
  }
  var value: String? { didSet { show(value ?? draft) } }
  var onChangeText: ((String) -> Void)?
  var onContentSizeChange: (() -> Void)?
  var onSubmitEditing: (() -> Void)?
  var maxLength = Int.max
  private var input: Input
  private var draft: String
  private var updating = false

  init(input: Input, defaultValue: String = "") {
    self.input = input; draft = defaultValue
    super.init()
    attach()
    show(defaultValue)
  }

  func bind(_ input: Input) {
    switch self.input {
    case let .textField(field):
      field.removeTarget(self, action: #selector(editingChanged), for: .editingChanged)
      if field.delegate === self { field.delegate = nil }
      (field as? ShadowListUtilsTextInputField)?.editingOwner = nil
    case let .textView(view):
      if view.delegate === self { view.delegate = nil }
      (view as? ShadowListUtilsTextInputView)?.editingOwner = nil
    }
    self.input = input
    attach()
    show(value ?? draft)
  }

  private func attach() {
    switch input {
    case let .textField(field):
      field.delegate = self
      field.addTarget(self, action: #selector(editingChanged), for: .editingChanged)
      (field as? ShadowListUtilsTextInputField)?.editingOwner = self
    case let .textView(view):
      view.delegate = self
      (view as? ShadowListUtilsTextInputView)?.editingOwner = self
    }
  }

  var text: String {
    switch input { case let .textField(field): return field.text ?? ""; case let .textView(view): return view.text ?? "" }
  }
  private var composing: Bool {
    switch input { case let .textField(field): return field.markedTextRange != nil; case let .textView(view): return view.markedTextRange != nil }
  }
  private var editable: Bool {
    switch input { case let .textField(field): return field.isEnabled; case let .textView(view): return view.isEditable }
  }
  private var selection: NSRange {
    switch input {
    case let .textField(field):
      guard let range = field.selectedTextRange else { return NSRange(location: 0, length: 0) }
      return NSRange(location: field.offset(from: field.beginningOfDocument, to: range.start), length: field.offset(from: range.start, to: range.end))
    case let .textView(view): return view.selectedRange
    }
  }
  private func setSelection(_ range: NSRange) {
    let length = (text as NSString).length
    let start = min(max(0, range.location), length)
    let count = min(max(0, range.length), length - start)
    switch input {
    case let .textField(field):
      guard let beginning = field.position(from: field.beginningOfDocument, offset: start), let end = field.position(from: beginning, offset: count) else { return }
      field.selectedTextRange = field.textRange(from: beginning, to: end)
    case let .textView(view): view.selectedRange = NSRange(location: start, length: count)
    }
  }
  private func show(_ next: String, notifySize: Bool = true) {
    guard !composing, text != next else { return }
    var previous = selection
    if previous.length == 0 {
      let fromEnd = max(0, (text as NSString).length - previous.location)
      previous.location = max(0, (next as NSString).length - fromEnd)
    }
    updating = true
    switch input {
    case let .textField(field): field.text = next
    case let .textView(view):
      let attributes = view.typingAttributes
      view.attributedText = NSAttributedString(string: next, attributes: attributes)
      view.typingAttributes = attributes
    }
    setSelection(previous)
    updating = false
    if notifySize { onContentSizeChange?() }
  }

  func clearDraft() { draft = ""; show(value ?? draft) }
  func commitComposition() { if let value { show(value) } }
  @objc private func editingChanged() { changed() }
  func changed() {
    guard !updating else { return }
    let next = text
    if value == nil { draft = next }
    onChangeText?(next)
    if let value { show(value, notifySize: false) }
    onContentSizeChange?()
  }

  private func shouldChange(range: NSRange, replacement: String) -> Bool {
    guard editable else { return false }
    let previous = text as NSString
    guard range.location >= 0, range.length >= 0, range.location <= previous.length, range.length <= previous.length - range.location else { return false }
    let replacement = replacement as NSString
    var allowed = max(0, max(0, maxLength) - previous.length + range.length)
    if replacement.length <= allowed { return true }
    guard replacement.length > 1, allowed > 0 else { return false }
    let cutoff = replacement.rangeOfComposedCharacterSequence(at: allowed - 1)
    if NSMaxRange(cutoff) > allowed { allowed = cutoff.location }
    guard allowed > 0 else { return false }
    show(previous.replacingCharacters(in: range, with: replacement.substring(to: allowed)), notifySize: false)
    setSelection(NSRange(location: range.location + allowed, length: 0))
    changed()
    return false
  }
  func textView(_ textView: UITextView, shouldChangeTextIn range: NSRange, replacementText text: String) -> Bool { shouldChange(range: range, replacement: text) }
  func textViewDidChange(_ textView: UITextView) { changed() }
  func textField(_ textField: UITextField, shouldChangeCharactersIn range: NSRange, replacementString string: String) -> Bool { shouldChange(range: range, replacement: string) }
  func textFieldShouldClear(_ textField: UITextField) -> Bool {
    textField.unmarkText()
    show("", notifySize: false); changed(); return false
  }
  func textFieldShouldReturn(_ textField: UITextField) -> Bool {
    textField.resignFirstResponder(); onSubmitEditing?(); return true
  }
}
