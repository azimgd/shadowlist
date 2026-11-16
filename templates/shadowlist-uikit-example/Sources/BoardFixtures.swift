import ShadowListUtils

enum BoardFixtures {
  static let columns: [ShadowListUtilsBoardColumn<BoardExampleCard>] = [
    ShadowListUtilsBoardColumn(key: "backlog", title: "Backlog", items: [
      BoardExampleCard(key: "c1", title: "Seat map shows the wrong row letters", label: "Bug", due: "Oct 28"),
      BoardExampleCard(key: "c2", title: "Boarding pass dark mode", label: "Design", due: "Oct 30"),
      BoardExampleCard(key: "c3", title: "Interview five frequent flyers about rebooking", label: "Research", due: "Nov 2"),
      BoardExampleCard(key: "c4", title: "Rotate the push certificate", label: "Ops", due: "Nov 4"),
      BoardExampleCard(key: "c5", title: "Rewrite the delay notice so it says what to do next", label: "Copy", due: "Nov 5"),
      BoardExampleCard(key: "c6", title: "Gate change banner", label: "Design", due: "Nov 7"),
      BoardExampleCard(key: "c7", title: "Lounge pass expires a day early in Tokyo", label: "Bug", due: "Nov 8"),
      BoardExampleCard(key: "c8", title: "Baggage tracking survey", label: "Research", due: "Nov 11"),
    ]),
    ShadowListUtilsBoardColumn(key: "doing", title: "In progress", items: [
      BoardExampleCard(key: "c9", title: "Trip timeline widget", label: "Design", due: "Oct 27"),
      BoardExampleCard(key: "c10", title: "Check-in crashes when the passport scan is cancelled", label: "Bug", due: "Oct 27"),
      BoardExampleCard(key: "c11", title: "Move receipts to the new store", label: "Ops", due: "Oct 29"),
      BoardExampleCard(key: "c12", title: "Onboarding headlines", label: "Copy", due: "Oct 31"),
    ]),
    ShadowListUtilsBoardColumn(key: "done", title: "Done", items: [
      BoardExampleCard(key: "c13", title: "Upgrade offer sheet", label: "Design", due: "Oct 20"),
      BoardExampleCard(key: "c14", title: "Wallet pass loses the gate after an update", label: "Bug", due: "Oct 21"),
      BoardExampleCard(key: "c15", title: "Status page on the new host", label: "Ops", due: "Oct 23"),
    ]),
  ]
}
