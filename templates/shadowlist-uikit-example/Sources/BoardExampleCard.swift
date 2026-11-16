import UIKit

struct BoardExampleCard {
  let key: String
  let title: String
  let label: String
  let due: String

  var color: UIColor {
    switch label {
    case "Bug": return AppTheme.colors.red
    case "Research": return AppTheme.colors.green
    case "Ops": return AppTheme.colors.orange
    case "Copy": return AppTheme.colors.secondaryLabel
    default: return AppTheme.colors.accent
    }
  }
}
