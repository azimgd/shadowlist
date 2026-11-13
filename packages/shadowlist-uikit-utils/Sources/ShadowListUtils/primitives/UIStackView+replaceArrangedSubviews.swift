import UIKit

/* Retain native children when only their values change; no detach or editor focus churn. */
extension UIStackView {
  func replaceArrangedSubviews(_ children: [UIView]) {
    for view in arrangedSubviews where !children.contains(where: { $0 === view }) {
      removeArrangedSubview(view)
      view.removeFromSuperview()
    }
    for (index, child) in children.enumerated() {
      if arrangedSubviews.indices.contains(index), arrangedSubviews[index] === child { continue }
      if arrangedSubviews.contains(where: { $0 === child }) { removeArrangedSubview(child) }
      insertArrangedSubview(child, at: index)
    }
  }
}
