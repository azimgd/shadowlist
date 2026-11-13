public struct ShadowListUtilsBoardLabels {
  public var emptyColumn: String
  public var emptyColumnReadOnly: String
  public var columnSummary: (String, Int) -> String

  public init(emptyColumn: String = "Drop cards here", emptyColumnReadOnly: String = "No cards",
              columnSummary: @escaping (String, Int) -> String = { "\($0), \($1) \($1 == 1 ? "card" : "cards")" }) {
    self.emptyColumn = emptyColumn
    self.emptyColumnReadOnly = emptyColumnReadOnly
    self.columnSummary = columnSummary
  }
}
