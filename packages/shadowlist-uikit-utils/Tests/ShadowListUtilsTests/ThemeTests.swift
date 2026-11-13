@testable import ShadowListUtils
import UIKit
import XCTest

/*
 * The theme tokens: the values the JS themes hold, the system theme and overrides.
 */
@MainActor
final class ThemeTests: XCTestCase {
  private func components(_ color: UIColor, _ style: UIUserInterfaceStyle = .light) -> [CGFloat] {
    var (red, green, blue, alpha) = (CGFloat(0), CGFloat(0), CGFloat(0), CGFloat(0))
    color.resolvedColor(with: UITraitCollection(userInterfaceStyle: style)).getRed(&red, green: &green, blue: &blue, alpha: &alpha)
    return [(red * 255).rounded(), (green * 255).rounded(), (blue * 255).rounded(), (alpha * 100).rounded() / 100]
  }

  /*
   * A CSS color as the JS themes write it: #RRGGBB or rgba(r,g,b,a).
   */
  private func components(css: String) throws -> [CGFloat] {
    if css.hasPrefix("#") {
      let value = try XCTUnwrap(UInt32(css.dropFirst(), radix: 16))
      return [CGFloat((value >> 16) & 0xFF), CGFloat((value >> 8) & 0xFF), CGFloat(value & 0xFF), 1]
    }
    let numbers = css.drop { $0 != "(" }.dropFirst().dropLast().split(separator: ",").compactMap {
      Double($0.trimmingCharacters(in: .whitespaces))
    }
    XCTAssertEqual(numbers.count, 4, css)
    return numbers.map { CGFloat($0) }
  }

  private let colorPaths: [(String, KeyPath<ShadowListUtilsThemeColors, UIColor>)] = [
    ("background", \.background), ("elevated", \.elevated), ("elevated2", \.elevated2),
    ("groupedBackground", \.groupedBackground), ("groupedCell", \.groupedCell), ("label", \.label),
    ("secondaryLabel", \.secondaryLabel), ("tertiaryLabel", \.tertiaryLabel), ("separator", \.separator), ("fill", \.fill),
    ("accent", \.accent), ("accentSoft", \.accentSoft), ("onAccent", \.onAccent), ("blue", \.blue), ("green", \.green),
    ("orange", \.orange), ("red", \.red), ("redSoft", \.redSoft),
  ]

  private let stylePaths: [(String, KeyPath<ShadowListUtilsThemeTypography, ShadowListUtilsTextStyle>)] = [
    ("largeTitle", \.largeTitle), ("title2", \.title2), ("title3", \.title3), ("headline", \.headline), ("body", \.body),
    ("callout", \.callout), ("subhead", \.subhead), ("footnote", \.footnote), ("caption", \.caption),
  ]

  func testTheLightColorsAreTheLightThemeOfTheTemplates() {
    let colors = ShadowListUtilsTheme.light.colors
    XCTAssertEqual(components(colors.background), [255, 255, 255, 1])
    XCTAssertEqual(components(colors.elevated), [242, 242, 247, 1])
    XCTAssertEqual(components(colors.elevated2), [229, 229, 234, 1])
    XCTAssertEqual(components(colors.label), [0, 0, 0, 1])
    XCTAssertEqual(components(colors.secondaryLabel), [60, 60, 67, 0.6])
    XCTAssertEqual(components(colors.tertiaryLabel), [60, 60, 67, 0.3])
    XCTAssertEqual(components(colors.separator), [60, 60, 67, 0.29])
    XCTAssertEqual(components(colors.fill), [118, 118, 128, 0.12])
    XCTAssertEqual(components(colors.accent), [0, 122, 255, 1])
    XCTAssertEqual(components(colors.accentSoft), [0, 122, 255, 0.16])
    XCTAssertEqual(components(colors.green), [52, 199, 89, 1])
    XCTAssertEqual(components(colors.orange), [255, 149, 0, 1])
    XCTAssertEqual(components(colors.red), [255, 59, 48, 1])
    XCTAssertEqual(components(colors.redSoft), [255, 59, 48, 0.16])
    XCTAssertEqual(colors.avatarPalette.count, 10)
    XCTAssertEqual(components(colors.avatarPalette[0]), [255, 107, 107, 1])
    XCTAssertEqual(components(colors.avatarPalette[9]), [192, 108, 132, 1])
  }

  func testTheDarkColorsAreTheDarkThemeOfTheTemplates() {
    let colors = ShadowListUtilsTheme.dark.colors
    XCTAssertEqual(components(colors.background), [0, 0, 0, 1])
    XCTAssertEqual(components(colors.elevated), [28, 28, 30, 1])
    XCTAssertEqual(components(colors.elevated2), [44, 44, 46, 1])
    XCTAssertEqual(components(colors.groupedCell), [28, 28, 30, 1])
    XCTAssertEqual(components(colors.label), [255, 255, 255, 1])
    XCTAssertEqual(components(colors.secondaryLabel), [235, 235, 245, 0.6])
    XCTAssertEqual(components(colors.separator), [84, 84, 88, 0.6])
    XCTAssertEqual(components(colors.fill), [118, 118, 128, 0.24])
    XCTAssertEqual(components(colors.accent), [10, 132, 255, 1])
    XCTAssertEqual(components(colors.green), [48, 209, 88, 1])
    XCTAssertEqual(components(colors.orange), [255, 159, 10, 1])
    XCTAssertEqual(components(colors.red), [255, 69, 59, 1])
  }

  func testBothThemesShareTheAvatarPalette() {
    XCTAssertEqual(ShadowListUtilsTheme.dark.colors.avatarPalette, ShadowListUtilsTheme.light.colors.avatarPalette)
    XCTAssertEqual(ShadowListUtilsTheme.light.colors.avatarPalette.count, 10)
  }

  func testTheSystemThemeFollowsTheDeviceAppearance() {
    let system = ShadowListUtilsTheme.system.colors
    for (name, path) in colorPaths {
      XCTAssertEqual(components(system[keyPath: path], .light), components(ShadowListUtilsTheme.light.colors[keyPath: path]), name)
      XCTAssertEqual(components(system[keyPath: path], .dark), components(ShadowListUtilsTheme.dark.colors[keyPath: path]), name)
    }
  }

  func testTheTextStylesAreTheTypographyOfTheTemplates() {
    let typography = ShadowListUtilsTheme.light.typography
    let expected: [(ShadowListUtilsTextStyle, CGFloat, ShadowListUtilsFontWeight, CGFloat, CGFloat)] = [
      (typography.largeTitle, 34, .bold, 41, 0.37), (typography.title2, 22, .bold, 28, 0.35),
      (typography.title3, 20, .semibold, 25, 0.38), (typography.headline, 17, .semibold, 22, -0.43),
      (typography.body, 17, .regular, 22, -0.43), (typography.callout, 16, .regular, 21, -0.31),
      (typography.subhead, 15, .regular, 20, -0.24), (typography.footnote, 13, .regular, 18, -0.08),
      (typography.caption, 12, .regular, 16, 0),
    ]
    for (style, fontSize, fontWeight, lineHeight, letterSpacing) in expected {
      XCTAssertEqual(style, ShadowListUtilsTextStyle(fontSize: fontSize, fontWeight: fontWeight, lineHeight: lineHeight, letterSpacing: letterSpacing))
    }
  }

  func testSpacingRadiusAndTheGroupedTokensAreTheBaseTokensOfTheTemplates() {
    let theme = ShadowListUtilsTheme.light
    XCTAssertEqual([theme.spacing.xxs, theme.spacing.xs, theme.spacing.sm, theme.spacing.md, theme.spacing.lg, theme.spacing.xl, theme.spacing.xxl], [2, 4, 8, 12, 16, 20, 24])
    XCTAssertEqual([theme.radius.sm, theme.radius.md, theme.radius.lg, theme.radius.xl, theme.radius.pill], [8, 12, 16, 20, 999])
    XCTAssertEqual([theme.grouped.inset, theme.grouped.radius, theme.grouped.rowInset, theme.grouped.rowHeight], [16, 10, 16, 44])
    XCTAssertEqual(theme.grouped.captionCase, .upper)
    XCTAssertTrue(theme.grouped.chevron)
    XCTAssertEqual(theme.rowInset, 68)
    XCTAssertEqual(theme.tapTarget, 44)
    XCTAssertEqual(ShadowListUtilsTheme.hairline, 1 / UIScreen.main.scale)
  }

  func testACopiedThemeChangesOneTokenAndKeepsTheRest() {
    var theme = ShadowListUtilsTheme.light
    theme.colors.accent = .orange
    theme.spacing.lg = 20
    XCTAssertEqual(theme.colors.accent, .orange)
    XCTAssertNotEqual(theme, .light)
    XCTAssertEqual(components(ShadowListUtilsTheme.light.colors.accent), [0, 122, 255, 1])
    XCTAssertEqual(ShadowListUtilsTheme.light.spacing.lg, 16)
  }

  func testATextStyleTypesetsLinesOfItsLineHeight() {
    let body = ShadowListUtilsTheme.light.typography.body
    let one = body.layout("One line", width: 300)
    XCTAssertEqual(one.lineCount, 1)
    XCTAssertEqual(one.size.height, body.lineHeight)
    let wrapped = body.layout(String(repeating: "word ", count: 40), width: 200)
    XCTAssertGreaterThan(wrapped.lineCount, 2)
    XCTAssertEqual(wrapped.size.height, CGFloat(wrapped.lineCount) * body.lineHeight)
    XCTAssertEqual(body.layout(String(repeating: "word ", count: 40), width: 200, maxLines: 2).lineCount, 2)
  }

  func testAWeightChangesOnlyTheWeightOfAStyle() {
    let typography = ShadowListUtilsTheme.light.typography
    XCTAssertEqual(typography.body.with(.semibold), typography.headline)
    XCTAssertEqual(typography.footnote.with(.bold), ShadowListUtilsTextStyle(fontSize: 13, fontWeight: .bold, lineHeight: 18, letterSpacing: -0.08))
    XCTAssertEqual(typography.footnote.fontWeight, .regular)
  }

  func testATextStyleFollowsTheReadersTextSize() {
    let body = ShadowListUtilsTheme.light.typography.body
    let plain = body.layout("One line", width: 300)
    withTextSize(2) {
      let large = body.layout("One line", width: 300)
      XCTAssertEqual(large.size.height, 44)
      XCTAssertEqual(body.scaledLineHeight(), 44)
      XCTAssertGreaterThan(large.size.width, plain.size.width * 1.8)
      XCTAssertTrue(ShadowListUtilsPlatform.isLargeText)
      // A style that is not scaled keeps its size, like initials in an avatar.
      XCTAssertEqual(body.layout("One line", width: 300, maxLines: 1, scaled: false).size, plain.size)
    }
    withTextSize(1.25) {
      // A line height between pixels rounds up to the next one.
      XCTAssertEqual(body.scaledLineHeight(), ShadowListUtilsPlatform.ceilToPixel(27.5))
      XCTAssertFalse(ShadowListUtilsPlatform.isLargeText)
    }
    XCTAssertEqual(body.layout("One line", width: 300).size, plain.size)
  }

  /*
   * theme.json holds the JS lightTheme and darkTheme as two cases, with input.scheme and the
   * theme as the output. A token changed in JS fails here until the native theme follows.
   */
  func testTheThemesMatchTheVectors() throws {
    let cases = try Vectors.cases("theme.json")
    let themes = ["light": ShadowListUtilsTheme.light, "dark": ShadowListUtilsTheme.dark]
    XCTAssertEqual(Set(cases.compactMap { ($0["input"] as? [String: Any])?["scheme"] as? String }), ["light", "dark"])
    for entry in cases {
      let name = try XCTUnwrap((entry["input"] as? [String: Any])?["scheme"] as? String)
      let theme = try XCTUnwrap(themes[name], name)
      let vector = try XCTUnwrap(entry["output"] as? [String: Any], name)
      let colors = try XCTUnwrap(vector["colors"] as? [String: Any])
      for (token, path) in colorPaths {
        let css = try XCTUnwrap(colors[token] as? String, "\(name) \(token)")
        XCTAssertEqual(components(theme.colors[keyPath: path]), try components(css: css), "\(name) \(token)")
      }
      let palette = try XCTUnwrap(colors["avatarPalette"] as? [String])
      XCTAssertEqual(try theme.colors.avatarPalette.map { components($0) }, try palette.map { try components(css: $0) })
      let typography = try XCTUnwrap(vector["typography"] as? [String: [String: Any]])
      let weights: [String: ShadowListUtilsFontWeight] = ["400": .regular, "600": .semibold, "700": .bold]
      for (token, path) in stylePaths {
        let style = try XCTUnwrap(typography[token], token)
        let weight = try XCTUnwrap(weights[try XCTUnwrap(style["fontWeight"] as? String)])
        let expected = ShadowListUtilsTextStyle(
          fontSize: try XCTUnwrap(style["fontSize"] as? CGFloat), fontWeight: weight,
          lineHeight: try XCTUnwrap(style["lineHeight"] as? CGFloat), letterSpacing: try XCTUnwrap(style["letterSpacing"] as? CGFloat))
        XCTAssertEqual(theme.typography[keyPath: path], expected, token)
      }
      let spacing = try XCTUnwrap(vector["spacing"] as? [String: CGFloat])
      XCTAssertEqual(
        spacing, ["xxs": theme.spacing.xxs, "xs": theme.spacing.xs, "sm": theme.spacing.sm, "md": theme.spacing.md,
                  "lg": theme.spacing.lg, "xl": theme.spacing.xl, "xxl": theme.spacing.xxl])
      let radius = try XCTUnwrap(vector["radius"] as? [String: CGFloat])
      XCTAssertEqual(
        radius, ["sm": theme.radius.sm, "md": theme.radius.md, "lg": theme.radius.lg, "xl": theme.radius.xl, "pill": theme.radius.pill])
      let grouped = try XCTUnwrap(vector["grouped"] as? [String: Any])
      XCTAssertEqual(grouped["inset"] as? CGFloat, theme.grouped.inset)
      XCTAssertEqual(grouped["radius"] as? CGFloat, theme.grouped.radius)
      XCTAssertEqual(grouped["rowInset"] as? CGFloat, theme.grouped.rowInset)
      XCTAssertEqual(grouped["rowHeight"] as? CGFloat, theme.grouped.rowHeight)
      XCTAssertEqual(grouped["captionCase"] as? String, theme.grouped.captionCase == .upper ? "upper" : "none")
      XCTAssertEqual(grouped["chevron"] as? Bool, theme.grouped.chevron)
      XCTAssertEqual(vector["rowInset"] as? CGFloat, theme.rowInset)
      XCTAssertEqual(vector["tapTarget"] as? CGFloat, theme.tapTarget)
    }
  }
}
