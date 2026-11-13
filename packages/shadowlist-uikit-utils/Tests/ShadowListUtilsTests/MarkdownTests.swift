import ShadowListKit
@testable import ShadowListUtils
import UIKit
import ShadowListUtilsCore
import XCTest

final class MarkdownTests: XCTestCase {
  func testTheStepBridgeReplaysEverySharedIntegerUtf16Fixture() throws {
    for entry in try Vectors.cases("assistant-steps.json") {
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      let positions = try XCTUnwrap(input["positions"] as? [Any]).map { NSNumber(value: ($0 as? Int) ?? 0) }
      let steps = ShadowListUtilsCore.stepsOf(try XCTUnwrap(input["content"] as? String), positions: positions)
      let expected = try XCTUnwrap(entry["output"] as? [[String: Any]])
      XCTAssertEqual(steps.count, expected.count)
      for (step, value) in zip(steps, expected) {
        if let text = step as? ShadowListUtilsAssistantTextStep {
          XCTAssertEqual(value["kind"] as? String, "text")
          XCTAssertEqual(text.text, value["text"] as? String)
          XCTAssertEqual(Int(text.at), value["at"] as? Int)
        } else {
          let calls = try XCTUnwrap(step as? ShadowListUtilsAssistantCallsStep)
          XCTAssertEqual(value["kind"] as? String, "calls")
          XCTAssertEqual(calls.callIndices.map(\.intValue), value["callIndices"] as? [Int])
        }
      }
    }
  }

  func testTheTypedBridgeReplaysTheSharedMarkdownAndUrlVectors() throws {
    let types = ["heading", "paragraph", "quote", "list", "code", "table", "rule"]
    let styles = ["text", "bold", "italic", "code", "link"]
    func runs(_ runs: [ShadowListUtilsMarkdownInline]) -> [[String: Any]] {
      runs.map { run in
        var result: [String: Any] = ["style": styles[run.style.rawValue], "text": run.text]
        if let href = run.href { result["href"] = href }
        return result
      }
    }
    for entry in try Vectors.cases("markdown.json") {
      let name = try XCTUnwrap(entry["name"] as? String)
      let input = try XCTUnwrap(entry["input"] as? [String: Any])
      if entry["function"] as? String == "isSafeUrl" {
        XCTAssertEqual(ShadowListUtilsCore.isSafeUrl(try XCTUnwrap(input["url"] as? String)), entry["output"] as? Bool, name)
        continue
      }
      let source = try XCTUnwrap(input["chunks"] as? [String]).joined()
      let blocks = ShadowListUtilsCore.parseMarkdown(source).map { block -> [String: Any] in
        var result: [String: Any] = ["key": block.key, "type": types[block.type.rawValue], "raw": block.raw]
        switch block.type {
        case .heading:
          result["level"] = block.level
          result["inlines"] = runs(block.runs.first ?? [])
        case .paragraph, .quote: result["inlines"] = runs(block.runs.first ?? [])
        case .list:
          result["ordered"] = block.ordered
          result["start"] = block.start
          result["items"] = block.runs.map(runs)
        case .code:
          result["language"] = block.language
          result["code"] = block.code
          result["closed"] = block.closed
        case .table:
          var at = 0
          var rows: [[[[String: Any]]]] = []
          for count in block.rowCellCounts {
            let end = at + count.intValue
            rows.append(Array(block.runs[at..<end]).map(runs))
            at = end
          }
          result["header"] = rows.first ?? []
          result["rows"] = Array(rows.dropFirst())
        case .rule: break
        @unknown default: XCTFail("Unknown block")
        }
        return result
      }
      let expected = try XCTUnwrap(entry["output"] as? [String: Any])["blocks"]
      XCTAssertEqual(NSArray(array: blocks), try XCTUnwrap(expected as? NSArray), name)
    }
  }
}
