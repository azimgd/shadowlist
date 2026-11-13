import ShadowListKit
import UIKit

#if canImport(ShadowListUtilsCore)
import ShadowListUtilsCore
#endif

/*
 * One shared-parser block. Typesetting and hit testing use the kit's same measured lines.
 */
final class ShadowListUtilsAssistantMarkdownBlock: UIView {
  let block: ShadowListUtilsMarkdownBlock
  let theme: ShadowListUtilsTheme
  var onCopyCode: ((String) -> Void)?
  var onOpenLink: ((String) -> Void)?
  var labels: ShadowListUtilsAssistantLabels
  private let scroll = UIScrollView()
  private let border = UIView()
  private let language = UILabel()
  private let copyButton = UIButton(type: .system)
  private var resetCopied: DispatchWorkItem?
  private var textViews: [ShadowListKitTextView] = []
  private var texts: [NSAttributedString] = []
  private var styles: [ShadowListUtilsTextStyle] = []
  private var layouts: [ShadowListKitTextLayout] = []

  init(block: ShadowListUtilsMarkdownBlock, theme: ShadowListUtilsTheme, labels: ShadowListUtilsAssistantLabels) {
    self.block = block
    self.theme = theme
    self.labels = labels
    super.init(frame: .zero)
    scroll.showsHorizontalScrollIndicator = false
    addSubview(scroll)
    border.backgroundColor = theme.colors.separator
    addSubview(border)
    if block.type == .code {
      backgroundColor = theme.colors.elevated
      layer.cornerRadius = theme.radius.sm
      language.text = block.language.isEmpty ? labels.codeLanguageFallback : block.language
      language.font = .systemFont(ofSize: theme.typography.caption.fontSize * ShadowListUtilsPlatform.textScale)
      language.textColor = theme.colors.secondaryLabel
      addSubview(language)
      copyButton.setTitle(labels.copy, for: .normal)
      copyButton.accessibilityLabel = labels.copyCode
      copyButton.isHidden = !block.closed
      copyButton.addTarget(self, action: #selector(copyCode), for: .touchUpInside)
      addSubview(copyButton)
    }
    for (text, style) in Self.makeTexts(block: block, theme: theme) { append(text, style: style) }
  }

  @available(*, unavailable)
  required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

  deinit { resetCopied?.cancel() }

  override func didMoveToWindow() {
    super.didMoveToWindow()
    if window == nil {
      resetCopied?.cancel()
      resetCopied = nil
      copyButton.setTitle(labels.copy, for: .normal)
      copyButton.accessibilityLabel = labels.copyCode
    }
  }


  static func makeTexts(block: ShadowListUtilsMarkdownBlock, theme: ShadowListUtilsTheme)
    -> [(NSAttributedString, ShadowListUtilsTextStyle)] {
    var result: [(NSAttributedString, ShadowListUtilsTextStyle)] = []
    func append(_ text: NSAttributedString, style: ShadowListUtilsTextStyle) { result.append((text, style)) }
    if block.type == .code {
      append(NSAttributedString(string: block.code.components(separatedBy: "\n").map { $0.isEmpty ? "\u{a0}" : $0 }.joined(separator: "\n"), attributes: [
        .font: UIFont.monospacedSystemFont(ofSize: 14 * ShadowListUtilsPlatform.textScale, weight: .regular),
        .foregroundColor: theme.colors.label,
      ]), style: ShadowListUtilsTextStyle(fontSize: 14, fontWeight: .regular, lineHeight: 20, letterSpacing: 0))
    } else if block.type != .rule {
      var groups = block.runs
      if block.type == .table {
        let columns = block.rowCellCounts.first?.intValue ?? 0
        groups = []
        var at = 0
        for count in block.rowCellCounts {
          for column in 0..<columns { groups.append(column < count.intValue ? block.runs[at + column] : []) }
          at += count.intValue
        }
      }
      for (index, runs) in groups.enumerated() {
        let style: ShadowListUtilsTextStyle
        if block.type == .heading {
          style = block.level == 1 ? theme.typography.title2 : block.level == 2 ? theme.typography.title3 : theme.typography.headline
        } else {
          style = block.type == .table ? theme.typography.subhead : theme.typography.body
        }
        let text = NSMutableAttributedString(string: "")
        if block.type == .list {
          let marker = block.ordered ? "\(NSNumber(value: block.start + Double(index)).stringValue). " : "• "
          text.append(NSAttributedString(string: marker, attributes: [.foregroundColor: theme.colors.secondaryLabel]))
        }
        let headerCell = block.type == .table && index < (block.rowCellCounts.first?.intValue ?? 0)
        for run in runs {
          var attributes: [NSAttributedString.Key: Any] = [
            .foregroundColor: block.type == .quote ? theme.colors.secondaryLabel : theme.colors.label,
          ]
          let fontSize = style.fontSize * ShadowListUtilsPlatform.textScale
          if run.style == .bold || headerCell { attributes[.font] = UIFont.systemFont(ofSize: fontSize, weight: .semibold) }
          if run.style == .italic { attributes[.font] = UIFont.italicSystemFont(ofSize: fontSize) }
          if run.style == .code {
            attributes[.font] = UIFont.monospacedSystemFont(
              ofSize: theme.typography.subhead.fontSize * ShadowListUtilsPlatform.textScale, weight: .regular)
            attributes[.backgroundColor] = theme.colors.fill
          }
          if run.style == .link {
            attributes[.foregroundColor] = theme.colors.accent
            attributes[.underlineStyle] = NSUnderlineStyle.single.rawValue
            if let href = run.href { attributes[.link] = href }
          }
          text.append(NSAttributedString(string: run.text, attributes: attributes))
        }
        append(text, style: style)
      }
    }
    return result
  }

  private func append(_ text: NSAttributedString, style: ShadowListUtilsTextStyle) {
    let view = ShadowListKitTextView(frame: .zero)
    view.isAccessibilityElement = true
    view.accessibilityLabel = text.string
    view.accessibilityTraits = block.type == .heading ? .header : .staticText
    view.isUserInteractionEnabled = true
    view.addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(openLink(_:))))
    view.accessibilityCustomActions = text.rangesOfLinks.map { label, href in
      UIAccessibilityCustomAction(name: label) { [weak self] _ in self?.open(href) ?? false }
    }
    scroll.addSubview(view)
    textViews.append(view)
    texts.append(text)
    styles.append(style)
  }

  override func sizeThatFits(_ size: CGSize) -> CGSize {
    CGSize(width: size.width, height: place(width: size.width))
  }

  override func layoutSubviews() {
    super.layoutSubviews()
    _ = place(width: bounds.width)
  }

  struct Layout {
    let block: ShadowListUtilsMarkdownBlock
    let width: CGFloat
    let height: CGFloat
    let texts: [NSAttributedString]
    let layouts: [ShadowListKitTextLayout]
    let frames: [CGRect]
    let contentWidth: CGFloat
    let contentHeight: CGFloat
    let padding: CGFloat
    let quoteInset: CGFloat
    let top: CGFloat
  }

  /*
   * Pure typesetting: a list's row layout can run this off the UI thread. The view applies it.
   */
  static func layout(block: ShadowListUtilsMarkdownBlock, width: CGFloat, theme: ShadowListUtilsTheme) -> Layout {
    let content = makeTexts(block: block, theme: theme)
    let padding = block.type == .code || block.type == .table ? theme.spacing.md : 0
    let quoteInset: CGFloat = block.type == .quote ? theme.spacing.md : 0
    let top: CGFloat = block.type == .code ? 40 : padding
    let columns = block.type == .table ? max(1, block.rowCellCounts.first?.intValue ?? 1) : 1
    let textWidth: CGFloat = block.type == .table ? 140 - 2 * padding : block.type == .code ? 100000 : max(1, width - quoteInset)
    var x: CGFloat = padding
    var y: CGFloat = padding
    var rowHeight: CGFloat = 0
    var contentWidth: CGFloat = 0
    var layouts: [ShadowListKitTextLayout] = []
    var frames: [CGRect] = []
    for (index, entry) in content.enumerated() {
      if block.type == .table, index > 0, index % columns == 0 {
        x = padding
        y += rowHeight + 2 * padding
        rowHeight = 0
      }
      let layout = entry.1.layout(entry.0, width: textWidth)
      layouts.append(layout)
      let frame = CGRect(x: x, y: y, width: max(1, layout.size.width), height: layout.size.height)
      frames.append(frame)
      rowHeight = max(rowHeight, frame.height)
      contentWidth = max(contentWidth, frame.maxX + padding)
      if block.type == .table { x += 140 }
      else { y = frame.maxY + (block.type == .list ? theme.spacing.xs : 0) }
    }
    let height = block.type == .table ? y + rowHeight + 2 * padding : max(0, y - (block.type == .list ? theme.spacing.xs : 0))
    return Layout(block: block, width: width, height: block.type == .rule ? ShadowListUtilsTheme.hairline : top + height + padding,
      texts: content.map { $0.0 }, layouts: layouts, frames: frames, contentWidth: contentWidth,
      contentHeight: height, padding: padding, quoteInset: quoteInset, top: top)
  }

  private func place(width: CGFloat) -> CGFloat {
    guard width > 0 else { return 0 }
    let layout = Self.layout(block: block, width: width, theme: theme)
    apply(layout)
    return layout.height
  }

  func apply(_ layout: Layout) {
    layouts = layout.layouts
    texts = layout.texts
    if block.type == .rule {
      border.frame = CGRect(x: 0, y: 0, width: layout.width, height: layout.height)
      return
    }
    for index in textViews.indices {
      textViews[index].textLayout = layout.layouts[index]
      textViews[index].textColor = theme.colors.label
      textViews[index].frame = layout.frames[index]
    }
    scroll.frame = CGRect(x: layout.quoteInset, y: layout.top,
      width: max(1, layout.width - layout.quoteInset), height: layout.contentHeight)
    scroll.contentSize = CGSize(width: max(scroll.bounds.width, layout.contentWidth), height: layout.contentHeight)
    border.isHidden = block.type != .quote
    if block.type == .quote { border.frame = CGRect(x: 0, y: 0, width: 3, height: layout.contentHeight) }
    language.frame = CGRect(x: layout.padding, y: 0, width: max(0, layout.width - 110), height: layout.top)
    copyButton.frame = CGRect(x: max(0, layout.width - 100), y: 0, width: 100 - layout.padding, height: layout.top)
  }

  @objc func copyCode() {
    guard block.closed else { return }
    onCopyCode?(block.code)
    copyButton.setTitle(labels.copied, for: .normal)
    copyButton.accessibilityLabel = labels.copied
    resetCopied?.cancel()
    let reset = DispatchWorkItem { [weak self] in
      guard let self else { return }
      self.copyButton.setTitle(self.labels.copy, for: .normal)
      self.copyButton.accessibilityLabel = self.labels.copyCode
    }
    resetCopied = reset
    DispatchQueue.main.asyncAfter(deadline: .now() + 1.5, execute: reset)
  }

  @objc private func openLink(_ gesture: UITapGestureRecognizer) {
    guard let view = gesture.view as? ShadowListKitTextView,
          let index = textViews.firstIndex(where: { $0 === view }), index < layouts.count else { return }
    let at = layouts[index].characterIndex(at: gesture.location(in: view))
    guard at != UInt(NSNotFound), at < texts[index].length,
          let href = texts[index].attribute(.link, at: Int(at), effectiveRange: nil) as? String else { return }
    open(href)
  }

  @discardableResult
  func open(_ href: String) -> Bool {
    if let onOpenLink { onOpenLink(href); return true }
    else if ShadowListUtilsCore.isSafeUrl(href), let url = URL(string: ShadowListUtilsCore.trim(href)) {
      UIApplication.shared.open(url)
      return true
    }
    return false
  }
}

private extension NSAttributedString {
  var rangesOfLinks: [(String, String)] {
    var result: [(String, String)] = []
    enumerateAttribute(.link, in: NSRange(location: 0, length: length)) { value, range, _ in
      if let href = value as? String { result.append((attributedSubstring(from: range).string, href)) }
    }
    return result
  }
}
