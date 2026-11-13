import Foundation

/*
 * totalVotes writes the footer. option is the label a screen reader says with its whole percent.
 */
public struct ShadowListUtilsPollLabels {
  public var totalVotes: (Double) -> String = { count in
    count == 1 ? "1 vote" : "\(NumberFormatter.localizedString(from: NSNumber(value: count), number: .decimal)) votes"
  }
  public var option: (String, Int) -> String = { "\($0), \($1)%" }

  public init() {}
}
