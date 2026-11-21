import ShadowListKit
import UIKit
import XCTest

/*
 * The width every layout test lays a row out at.
 */
let testWidth: CGFloat = 390

/*
 * Puts a list into a window of a phone's size and lays it out, the way a screen would.
 */
@MainActor
@discardableResult
func host(_ list: ShadowListKitListView, size: CGSize = CGSize(width: testWidth, height: 800)) -> UIWindow {
  let window = UIWindow(frame: CGRect(origin: .zero, size: size))
  list.frame = window.bounds
  window.addSubview(list)
  window.isHidden = false
  list.layoutIfNeeded()
  return window
}
