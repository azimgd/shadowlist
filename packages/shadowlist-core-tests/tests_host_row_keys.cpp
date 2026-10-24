/*
 * RowKeyStore and TreeRows tests: the row keys Fabric keeps natively by version instead of in
 * props, and the section and tree flattens that used to run in JS. The section fuzz test is the
 * one sectionFlatten.test.ts ran against the JS flatten, now against a full rebuild.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>
#include <shadowlist-core/host/TreeRows.hpp>

#include <cstdint>
#include <functional>
#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

std::vector<std::string> keysOf(std::initializer_list<const char*> keys) {
  return std::vector<std::string>(keys.begin(), keys.end());
}

/*
 * A tree node for the tests: key and children.
 */
struct Node {
  std::string key;
  std::vector<Node> children;
};

/*
 * The flatten TreeList ran in JS: depth first, children only of expanded nodes.
 */
void flatten(const std::vector<Node>& nodes, std::uint32_t depth, const std::set<std::string>& expanded,
  std::vector<TreeRow>& rows) {
  for (const Node& node : nodes) {
    bool hasChildren = !node.children.empty();
    bool isExpanded = hasChildren && expanded.count(node.key) > 0;
    rows.push_back({node.key, depth, hasChildren, isExpanded});
    if (isExpanded) {
      flatten(node.children, depth + 1, expanded, rows);
    }
  }
}

const Node* findNode(const std::vector<Node>& nodes, const std::string& key, std::uint32_t& depth, std::uint32_t at = 0) {
  for (const Node& node : nodes) {
    if (node.key == key) {
      depth = at;
      return &node;
    }
    if (const Node* found = findNode(node.children, key, depth, at + 1)) {
      return found;
    }
  }
  return nullptr;
}

std::vector<Node> sampleTree() {
  // a(b(c, d), e), f(g)
  return {
    {"a", {{"b", {{"c", {}}, {"d", {}}}}, {"e", {}}}},
    {"f", {{"g", {}}}},
  };
}

void checkRows(const TreeRows& tree, const std::vector<TreeRow>& expected) {
  CHECK_EQ(tree.size(), expected.size());
  for (std::size_t row = 0; row < expected.size() && row < tree.size(); ++row) {
    TreeRow actual = tree.rowAt(row);
    CHECK_EQ(actual.key, expected[row].key);
    CHECK_EQ(actual.depth, expected[row].depth);
    CHECK_EQ(actual.hasChildren, expected[row].hasChildren);
    CHECK_EQ(actual.expanded, expected[row].expanded);
  }
}

/*
 * The section rows Fabric's SectionList keys natively: header, items qualified by the section
 * key, footer, built from scratch.
 */
struct TestSection {
  std::string key;
  std::vector<std::string> items;
};

std::vector<std::string> referenceRows(const std::vector<TestSection>& sections, bool header, bool footer) {
  std::vector<std::string> rows;
  for (const TestSection& section : sections) {
    if (header) {
      rows.push_back(ListSections::headerRowKey(section.key));
    }
    for (const std::string& item : section.items) {
      rows.push_back(ListSections::qualifiedItemRowKey(section.key, item));
    }
    if (footer) {
      rows.push_back(ListSections::footerRowKey(section.key));
    }
  }
  return rows;
}

}

TEST(section_row_keys_use_the_core_prefixes) {
  CHECK_EQ(ListSections::headerRowKey("s"), std::string("\x1fh:s"));
  CHECK_EQ(ListSections::footerRowKey("s"), std::string("\x1f" "f:s"));
  CHECK_EQ(ListSections::qualifiedItemRowKey("s", "7"), std::string("\x1f" "i:s\x1f" "7"));
  // Same item key in two sections, two rows.
  CHECK(ListSections::qualifiedItemRowKey("a", "0") != ListSections::qualifiedItemRowKey("b", "0"));
}

TEST(tree_rows_collapse_and_expand_splice_the_visible_descendants) {
  std::vector<Node> nodes = sampleTree();
  std::set<std::string> expanded = {"a", "b", "f"};
  std::vector<TreeRow> rows;
  flatten(nodes, 0, expanded, rows);
  TreeRows tree;
  tree.reset(rows);
  // a b c d e f g
  CHECK_EQ(tree.size(), std::size_t(7));
  CHECK_EQ(tree.subtreeEnd(0), std::size_t(5));
  CHECK_EQ(tree.subtreeEnd(1), std::size_t(4));
  CHECK_EQ(tree.subtreeEnd(2), std::size_t(3));
  CHECK_EQ(tree.subtreeEnd(5), std::size_t(7));

  TreeSplice collapsed = tree.collapse(1);
  CHECK_EQ(collapsed.start, std::size_t(2));
  CHECK_EQ(collapsed.deleted, std::size_t(2));
  CHECK_EQ(collapsed.inserted, std::size_t(0));
  expanded.erase("b");
  rows.clear();
  flatten(nodes, 0, expanded, rows);
  checkRows(tree, rows);

  // Expanding a walks only a's subtree, b stays collapsed under it.
  TreeSplice collapsedA = tree.collapse(0);
  CHECK_EQ(collapsedA.deleted, std::size_t(2));
  expanded.erase("a");
  expanded.insert("a");
  std::vector<TreeRow> under;
  std::uint32_t depth = 0;
  const Node* a = findNode(nodes, "a", depth);
  flatten(a->children, depth + 1, expanded, under);
  TreeSplice expandedA = tree.expand(0, under);
  CHECK_EQ(expandedA.start, std::size_t(1));
  CHECK_EQ(expandedA.deleted, std::size_t(0));
  CHECK_EQ(expandedA.inserted, std::size_t(2));
  rows.clear();
  flatten(nodes, 0, expanded, rows);
  checkRows(tree, rows);
}

TEST(tree_rows_ignore_expanding_a_leaf_or_a_row_past_the_end) {
  TreeRows tree;
  tree.reset({{"x", 0, false, false}});
  TreeSplice leaf = tree.expand(0, {{"y", 1, false, false}});
  CHECK_EQ(leaf.inserted, std::size_t(0));
  CHECK_EQ(tree.size(), std::size_t(1));
  TreeSplice past = tree.collapse(3);
  CHECK_EQ(past.deleted, std::size_t(0));
}

TEST(tree_rows_match_a_full_flatten_through_random_toggles) {
  // Four levels of three children, every toggle checked against the JS flatten.
  std::function<Node(const std::string&, int)> build = [&](const std::string& key, int level) {
    Node node{key, {}};
    if (level < 4) {
      for (int child = 0; child < 3; ++child) {
        node.children.push_back(build(key + "." + std::to_string(child), level + 1));
      }
    }
    return node;
  };
  std::vector<Node> nodes = {build("r0", 1), build("r1", 1)};
  std::set<std::string> expanded;
  std::vector<TreeRow> rows;
  flatten(nodes, 0, expanded, rows);
  RowKeyStore store;
  CHECK(store.resetTree(1, rows));
  std::uint64_t version = 1;
  std::uint32_t seed = 7;
  for (int step = 0; step < 300; ++step) {
    seed = seed * 1103515245u + 12345u;
    auto current = store.snapshot(version);
    std::size_t row = (seed >> 8) % current->size();
    TreeRow target = current->getTree().rowAt(row);
    if (!target.hasChildren) {
      continue;
    }
    std::uint64_t next = version + 1;
    if (target.expanded) {
      expanded.erase(target.key);
      auto splice = store.collapseTree(next, version, row);
      CHECK(splice.has_value());
    } else {
      expanded.insert(target.key);
      std::uint32_t depth = 0;
      const Node* node = findNode(nodes, target.key, depth);
      std::vector<TreeRow> under;
      flatten(node->children, depth + 1, expanded, under);
      auto splice = store.expandTree(next, version, row, under);
      CHECK(splice.has_value());
      CHECK_EQ(splice->inserted, under.size());
    }
    store.adopted(next);
    version = next;
    rows.clear();
    flatten(nodes, 0, expanded, rows);
    checkRows(store.snapshot(version)->getTree(), rows);
  }
}

TEST(row_key_store_splices_from_a_base_and_reports_end_edits) {
  RowKeyStore store;
  CHECK(store.setKeys(1, keysOf({"a", "b", "c"})));
  CHECK(store.spliceKeys(2, 1, 3, 0, keysOf({"d", "e"})));
  auto appended = store.snapshot(2);
  CHECK(appended->keys() == keysOf({"a", "b", "c", "d", "e"}));
  CHECK(appended->getEdit().kind == KeyEditKind::Append);
  CHECK_EQ(appended->getEdit().count, std::size_t(2));
  CHECK_EQ(appended->getBaseVersion(), std::uint64_t(1));

  CHECK(store.spliceKeys(3, 2, 0, 0, keysOf({"z"})));
  CHECK(store.snapshot(3)->getEdit().kind == KeyEditKind::Prepend);
  CHECK(store.spliceKeys(4, 3, 0, 2, {}));
  CHECK(store.snapshot(4)->getEdit().kind == KeyEditKind::TrimStart);
  CHECK(store.spliceKeys(5, 4, 1, 1, keysOf({"q"})));
  CHECK(store.snapshot(5)->keys() == keysOf({"b", "q", "d", "e"}));
  CHECK(store.snapshot(5)->getEdit().kind == KeyEditKind::Unknown);

  // A base that is not there, or a splice past its end, fails.
  CHECK(!store.spliceKeys(9, 42, 0, 0, keysOf({"x"})));
  CHECK(!store.spliceKeys(9, 5, 3, 2, {}));
}

TEST(row_key_store_drops_versions_no_commit_can_name) {
  RowKeyStore store;
  for (std::uint64_t version = 1; version <= 4; ++version) {
    store.setKeys(version, keysOf({"a"}));
  }
  store.adopted(2);
  // 2 and everything after it stay, along with nothing before since no commit preceded 2.
  CHECK(store.snapshot(1) != nullptr);
  store.adopted(4);
  // The version committed before 4 stays for a late clone, older ones go.
  CHECK(store.snapshot(1) == nullptr);
  CHECK(store.snapshot(2) != nullptr);
  CHECK(store.snapshot(4) != nullptr);
  store.adopted(5);
  CHECK(store.snapshot(2) == nullptr);
  CHECK(store.snapshot(4) != nullptr);
}

TEST(row_key_store_keeps_a_bounded_number_of_uncommitted_versions) {
  RowKeyStore store;
  store.setKeys(1, keysOf({"a"}));
  store.adopted(1);
  for (std::uint64_t version = 2; version < 2 + 3 * RowKeyStore::MAX_PENDING_VERSIONS; ++version) {
    store.setKeys(version, keysOf({"a"}));
  }
  CHECK_EQ(store.versionCount(), RowKeyStore::MAX_PENDING_VERSIONS + 1);
  CHECK(store.snapshot(1) != nullptr);
}

TEST(row_key_snapshot_finds_keys_near_a_hint_and_through_its_index) {
  std::vector<std::string> keys;
  for (int index = 0; index < 1000; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  keys.push_back("k10");  // a repeat
  RowKeyStore store;
  store.setKeys(1, keys);
  auto snapshot = store.snapshot(1);
  CHECK_EQ(snapshot->indexOfKey("k500", 500), std::size_t(500));
  CHECK_EQ(snapshot->indexOfKey("k500", 470), std::size_t(500));
  CHECK_EQ(snapshot->indexOfKey("k900", 3), std::size_t(900));
  CHECK_EQ(snapshot->indexOfKey("k10"), std::size_t(10));
  CHECK_EQ(snapshot->indexOfKey("missing", 10), UNDEFINED_INDEX);
  CHECK_EQ(snapshot->indexOfKey("k5", 99999), std::size_t(5));
}

TEST(row_key_store_builds_section_rows_and_sticky_headers) {
  RowKeyStore store;
  SectionsUpdate update;
  update.hasHeader = true;
  update.hasFooter = true;
  update.stickyHeaders = true;
  update.sectionKeys = keysOf({"s0", "s1", "s2"});
  update.itemCounts = {2, 0, 1};
  update.reuse = {UNDEFINED_INDEX, UNDEFINED_INDEX, UNDEFINED_INDEX};
  // Item keys repeat across sections, like index keys.
  update.itemKeys = keysOf({"0", "1", "0"});
  CHECK(store.setSections(1, 0, update));
  auto snapshot = store.snapshot(1);
  std::vector<TestSection> sections = {{"s0", keysOf({"0", "1"})}, {"s1", {}}, {"s2", keysOf({"0"})}};
  CHECK(snapshot->keys() == referenceRows(sections, true, true));
  CHECK(snapshot->getStickyRows() == std::vector<std::size_t>({0, 4, 6}));
  CHECK(snapshot->getKind() == RowKeysSnapshot::Kind::Sections);

  // Headers that do not stick give no sticky rows.
  update.stickyHeaders = false;
  CHECK(store.setSections(2, 0, update));
  CHECK(store.snapshot(2)->getStickyRows().empty());
}

TEST(row_key_store_refuses_reuse_that_does_not_match_the_base) {
  RowKeyStore store;
  SectionsUpdate update;
  update.hasHeader = true;
  update.sectionKeys = keysOf({"s0"});
  update.itemCounts = {2};
  update.reuse = {UNDEFINED_INDEX};
  update.itemKeys = keysOf({"a", "b"});
  CHECK(store.setSections(1, 0, update));
  SectionsUpdate wrongCount = update;
  wrongCount.itemCounts = {3};
  wrongCount.reuse = {0};
  wrongCount.itemKeys.clear();
  CHECK(!store.setSections(2, 1, wrongCount));
  SectionsUpdate wrongKey = update;
  wrongKey.sectionKeys = keysOf({"other"});
  wrongKey.reuse = {0};
  wrongKey.itemKeys.clear();
  CHECK(!store.setSections(3, 1, wrongKey));
  SectionsUpdate missingBase = update;
  missingBase.reuse = {0};
  missingBase.itemKeys.clear();
  CHECK(!store.setSections(4, 77, missingBase));
}

/*
 * The fuzz test sectionFlatten.test.ts ran on the JS flatten: random section edits, unchanged
 * sections reused, every result equal to a rebuild.
 */
TEST(row_key_store_section_updates_match_a_full_rebuild) {
  std::uint32_t seed = 11;
  auto random = [&seed]() {
    seed = (seed * 1103515245u + 12345u) % 2147483648u;
    return static_cast<double>(seed) / 2147483648.0;
  };
  int nextId = 0;
  int nextSection = 0;
  auto item = [&]() { return "i" + std::to_string(nextId++); };
  auto section = [&](int count) {
    TestSection made{"s" + std::to_string(nextSection++), {}};
    for (int index = 0; index < count; ++index) {
      made.items.push_back(item());
    }
    return made;
  };

  for (auto [hasHeader, hasFooter] : {std::pair{true, false}, {false, true}, {true, true}, {false, false}}) {
    RowKeyStore store;
    std::vector<TestSection> sections = {section(3), section(0), section(4)};
    // Which sections the previous version held, by key, and where.
    std::vector<std::string> previousKeys;
    std::vector<std::vector<std::string>> previousItems;
    std::uint64_t version = 0;

    for (int step = 0; step < 400; ++step) {
      std::vector<TestSection> next = sections;
      int change = static_cast<int>(random() * 7);
      std::size_t at = static_cast<std::size_t>(random() * static_cast<double>(next.size()));
      bool hasTarget = at < next.size();
      if (change == 0 && hasTarget) {
        next[at].items.insert(next[at].items.begin(), item());
      } else if (change == 1 && hasTarget && !next[at].items.empty()) {
        next[at].items.erase(next[at].items.begin());
      } else if (change == 2) {
        next.insert(next.begin() + static_cast<std::ptrdiff_t>(std::min(at, next.size())), section(1 + step % 3));
      } else if (change == 3 && next.size() > 1 && hasTarget) {
        next.erase(next.begin() + static_cast<std::ptrdiff_t>(at));
      } else if (change == 4 && hasTarget && !next[at].items.empty()) {
        next[at].items[0] = item();
      } else if (change == 5 && hasTarget) {
        next[at].items.push_back(item());
      }
      if (next.size() > 8) {
        next.erase(next.begin(), next.begin() + 4);
      }

      SectionsUpdate update;
      update.hasHeader = hasHeader;
      update.hasFooter = hasFooter;
      update.stickyHeaders = true;
      for (const TestSection& candidate : next) {
        update.sectionKeys.push_back(candidate.key);
        update.itemCounts.push_back(candidate.items.size());
        std::size_t reuse = UNDEFINED_INDEX;
        for (std::size_t previous = 0; previous < previousKeys.size(); ++previous) {
          if (previousKeys[previous] == candidate.key && previousItems[previous] == candidate.items) {
            reuse = previous;
          }
        }
        update.reuse.push_back(reuse);
        if (reuse == UNDEFINED_INDEX) {
          update.itemKeys.insert(update.itemKeys.end(), candidate.items.begin(), candidate.items.end());
        }
      }
      std::uint64_t nextVersion = version + 1;
      CHECK(store.setSections(nextVersion, version, update));
      store.adopted(nextVersion);
      auto snapshot = store.snapshot(nextVersion);
      CHECK(snapshot->keys() == referenceRows(next, hasHeader, hasFooter));
      std::vector<std::size_t> headers;
      if (hasHeader) {
        std::size_t row = 0;
        for (const TestSection& candidate : next) {
          headers.push_back(row);
          row += 1 + candidate.items.size() + (hasFooter ? 1 : 0);
        }
      }
      CHECK(snapshot->getStickyRows() == headers);

      sections = next;
      version = nextVersion;
      previousKeys.clear();
      previousItems.clear();
      for (const TestSection& kept : next) {
        previousKeys.push_back(kept.key);
        previousItems.push_back(kept.items);
      }
    }
  }
}

TEST(row_key_store_keeps_size_specs_by_version) {
  RowKeyStore store;
  RowSizeSpec spec;
  spec.key = "a";
  spec.text = "hello";
  store.setSizeSpecs(1, {spec});
  store.setSizeSpecs(2, {});
  store.adoptedSizeSpecs(1);
  store.adoptedSizeSpecs(2);
  CHECK(store.sizeSpecs(1) != nullptr);
  CHECK_EQ(store.sizeSpecs(1)->front().text, std::string("hello"));
  store.adoptedSizeSpecs(3);
  CHECK(store.sizeSpecs(1) == nullptr);
}

TEST(row_key_store_serves_commits_on_other_threads_while_the_renderer_writes) {
  // The renderer writes versions on one thread while commits read and index them on others.
  RowKeyStore store;
  std::vector<std::string> keys;
  for (int index = 0; index < 2000; ++index) {
    keys.push_back("k" + std::to_string(index));
  }
  store.setKeys(1, keys);
  std::atomic<std::uint64_t> latest{1};
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (std::uint64_t version = 2; version < 400; ++version) {
      store.spliceKeys(version, version - 1, 0, 0, {"p" + std::to_string(version)});
      latest.store(version);
    }
    done.store(true);
  });
  std::atomic<int> found{0};
  auto reader = [&] {
    while (!done.load()) {
      std::uint64_t version = latest.load();
      if (auto snapshot = store.snapshot(version)) {
        if (snapshot->indexOfKey("k1000") != UNDEFINED_INDEX) {
          found.fetch_add(1);
        }
        store.adopted(version);
      }
    }
  };
  std::thread first(reader);
  std::thread second(reader);
  writer.join();
  first.join();
  second.join();
  CHECK(found.load() > 0);
  CHECK_EQ(store.snapshot(399)->indexOfKey("k1000"), std::size_t(1398));
}
