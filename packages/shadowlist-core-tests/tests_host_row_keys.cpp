/*
 * RowKeyStore and TreeRows tests: the row keys Fabric keeps natively by version, and the
 * section and tree flattens. The section fuzz test checks the flatten against a full rebuild,
 * and the tree fuzz test checks TreeRows against a full flatten of a random tree. See
 * tests_fuzz.cpp for how the fuzz tests run.
 */

#include "FuzzHelpers.hpp"
#include "TestFramework.hpp"

#include <shadowlist-core/host/ListSections.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>
#include <shadowlist-core/host/TreeRows.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <utility>
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
void flatten(
  const std::vector<Node>& nodes,
  std::uint32_t depth,
  const std::set<std::string>& expanded,
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

const Node*
findNode(const std::vector<Node>& nodes, const std::string& key, std::uint32_t& depth, std::uint32_t at = 0) {
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
  CHECK_EQ(
    ListSections::footerRowKey("s"), std::string(
                                       "\x1f"
                                       "f:s"));
  CHECK_EQ(
    ListSections::qualifiedItemRowKey("s", "7"), std::string(
                                                   "\x1f"
                                                   "i:s\x1f"
                                                   "7"));
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

/*
 * A new version of a tree is built from its base, which stays as it was.
 */
TEST(tree_rows_expand_and_collapse_from_a_base_leave_the_base_alone) {
  std::vector<Node> nodes = sampleTree();
  std::set<std::string> expanded = {"a", "b", "f"};
  std::vector<TreeRow> rows;
  flatten(nodes, 0, expanded, rows);
  TreeRows base;
  base.reset(rows);

  TreeRows collapsed;
  TreeSplice splice = collapsed.collapseFrom(base, 0);
  CHECK_EQ(splice.start, std::size_t(1));
  CHECK_EQ(splice.deleted, std::size_t(4));
  checkRows(base, rows);
  std::set<std::string> collapsedSet = {"b", "f"};
  std::vector<TreeRow> collapsedRows;
  flatten(nodes, 0, collapsedSet, collapsedRows);
  checkRows(collapsed, collapsedRows);

  TreeRows reopened;
  std::vector<TreeRow> under;
  std::uint32_t depth = 0;
  const Node* a = findNode(nodes, "a", depth);
  flatten(a->children, depth + 1, expanded, under);
  splice = reopened.expandFrom(collapsed, 0, under);
  CHECK_EQ(splice.inserted, under.size());
  checkRows(reopened, rows);
  checkRows(collapsed, collapsedRows);
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

TEST(row_key_store_splices_from_a_base_and_reports_its_edit) {
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
  const KeyEdit& replaced = store.snapshot(5)->getEdit();
  CHECK(replaced.kind == KeyEditKind::Splice);
  CHECK_EQ(replaced.start, std::size_t(1));
  CHECK_EQ(replaced.deleted, std::size_t(1));
  CHECK_EQ(replaced.inserted, std::size_t(1));
  // Every key replaced is no edit the core can use.
  CHECK(store.spliceKeys(6, 5, 0, 4, keysOf({"x"})));
  CHECK(store.snapshot(6)->getEdit().kind == KeyEditKind::Unknown);

  // A base that is not there, or a splice past its end, fails.
  CHECK(!store.spliceKeys(9, 42, 0, 0, keysOf({"x"})));
  CHECK(!store.spliceKeys(9, 6, 3, 2, {}));
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
  keys.push_back("k10"); // a repeat
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
 * Random section edits with unchanged sections reused. Every result equals a rebuild.
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
  auto read = [&] {
    std::uint64_t version = latest.load();
    if (auto snapshot = store.snapshot(version)) {
      if (snapshot->indexOfKey("k1000") != UNDEFINED_INDEX) {
        found.fetch_add(1);
      }
      store.adopted(version);
    }
  };
  /*
   * The writer can finish before a reader gets a version that is still kept. The read after the
   * loop finds the last version, which the store never drops.
   */
  auto reader = [&] {
    while (!done.load()) {
      read();
    }
    read();
  };
  std::thread first(reader);
  std::thread second(reader);
  writer.join();
  first.join();
  second.join();
  CHECK(found.load() > 0);
  CHECK_EQ(store.snapshot(399)->indexOfKey("k1000"), std::size_t(1398));
}

namespace {

std::vector<std::string> longKeys(const std::string& prefix, std::size_t count) {
  std::vector<std::string> keys;
  for (std::size_t index = 0; index < count; ++index) {
    keys.push_back(prefix + "-0123456789abcdef-0123456789abcdef-" + std::to_string(index));
  }
  return keys;
}

/*
 * Whether next's rows from nextStart hold base's key strings from baseStart, the same objects.
 */
bool sharesKeys(KeyList base, std::size_t baseStart, KeyList next, std::size_t nextStart, std::size_t count) {
  for (std::size_t offset = 0; offset < count; ++offset) {
    if (&base[baseStart + offset] != &next[nextStart + offset]) {
      return false;
    }
  }
  return true;
}

}

TEST(row_key_store_splices_share_the_keys_they_keep) {
  RowKeyStore store;
  std::vector<std::string> model = longKeys("a", 100);
  CHECK(store.setKeys(1, model));
  auto first = store.snapshot(1);

  CHECK(store.spliceKeys(2, 1, 100, 0, longKeys("end", 3)));
  auto appended = store.snapshot(2);
  CHECK(sharesKeys(first->keys(), 0, appended->keys(), 0, 100));

  CHECK(store.spliceKeys(3, 2, 0, 0, longKeys("start", 2)));
  auto prepended = store.snapshot(3);
  CHECK(sharesKeys(appended->keys(), 0, prepended->keys(), 2, 103));

  CHECK(store.spliceKeys(4, 3, 50, 1, longKeys("middle", 1)));
  auto replaced = store.snapshot(4);
  CHECK(sharesKeys(prepended->keys(), 0, replaced->keys(), 0, 50));
  CHECK(sharesKeys(prepended->keys(), 51, replaced->keys(), 51, 54));

  CHECK(store.spliceKeys(5, 4, 100, 5, {}));
  CHECK(store.spliceKeys(6, 5, 0, 10, {}));
  auto trimmed = store.snapshot(6);
  CHECK(sharesKeys(replaced->keys(), 10, trimmed->keys(), 0, 90));

  // Versions spliced from one base at the same end each get their own keys.
  CHECK(store.spliceKeys(7, 1, 100, 0, longKeys("other", 2)));
  CHECK(store.spliceKeys(8, 1, 0, 0, longKeys("other-start", 1)));
  std::vector<std::string> other = model;
  for (const std::string& key : longKeys("other", 2)) {
    other.push_back(key);
  }
  CHECK(store.snapshot(7)->keys() == other);
  CHECK(sharesKeys(first->keys(), 0, store.snapshot(7)->keys(), 0, 100));
  CHECK(sharesKeys(first->keys(), 0, store.snapshot(8)->keys(), 1, 100));
  CHECK(store.snapshot(8)->keys()[0] == longKeys("other-start", 1)[0]);

  // Every earlier version still reads what it was made with.
  CHECK(first->keys() == model);
  std::vector<std::string> expected = model;
  for (const std::string& key : longKeys("end", 3)) {
    expected.push_back(key);
  }
  CHECK(appended->keys() == expected);
  std::vector<std::string> starts = longKeys("start", 2);
  expected.insert(expected.begin(), starts.begin(), starts.end());
  CHECK(prepended->keys() == expected);
  expected[50] = longKeys("middle", 1)[0];
  CHECK(replaced->keys() == expected);
  expected.erase(expected.begin() + 100, expected.end());
  expected.erase(expected.begin(), expected.begin() + 10);
  CHECK(trimmed->keys() == expected);
}

TEST(shared_keys_store_kept_keys_afresh_only_once_deleted_keys_pile_up) {
  std::vector<std::string> model = longKeys("k", 10);
  SharedKeys keys(model);
  std::size_t fresh = 0;
  constexpr std::size_t EDITS = 4 * SharedKeys::COMPACT_SLACK;
  for (std::size_t edit = 0; edit < EDITS; ++edit) {
    std::string key = "replaced-" + std::to_string(edit);
    SharedKeys next = keys.splice(5, 1, {key});
    model[5] = key;
    if (!sharesKeys(keys.list(), 0, next.list(), 0, 5)) {
      ++fresh;
    }
    keys = next;
    CHECK(keys.list() == model);
  }
  CHECK(fresh >= 1);
  CHECK(fresh <= EDITS / SharedKeys::COMPACT_SLACK);
}

TEST(row_key_store_sections_and_trees_share_the_keys_they_keep) {
  RowKeyStore store;
  SectionsUpdate update;
  update.hasHeader = true;
  update.sectionKeys = keysOf({"s0", "s1"});
  update.itemCounts = {2, 1};
  update.reuse = {UNDEFINED_INDEX, UNDEFINED_INDEX};
  update.itemKeys = keysOf({"a", "b", "c"});
  CHECK(store.setSections(1, 0, update));
  update.itemCounts = {2, 2};
  update.reuse = {0, UNDEFINED_INDEX};
  update.itemKeys = keysOf({"c", "d"});
  CHECK(store.setSections(2, 1, update));
  auto base = store.snapshot(1);
  auto next = store.snapshot(2);
  CHECK(sharesKeys(base->keys(), 0, next->keys(), 0, 3));
  std::vector<TestSection> sections = {{"s0", keysOf({"a", "b"})}, {"s1", keysOf({"c", "d"})}};
  CHECK(next->keys() == referenceRows(sections, true, false));

  std::vector<TreeRow> rows;
  for (const std::string& key : longKeys("node", 50)) {
    rows.push_back({key, 0, true, false});
  }
  CHECK(store.resetTree(3, rows));
  auto expanded = store.expandTree(4, 3, 20, {{"child-0", 1, false, false}, {"child-1", 1, false, false}});
  CHECK(expanded.has_value());
  auto tree = store.snapshot(3);
  auto grown = store.snapshot(4);
  CHECK(sharesKeys(tree->keys(), 0, grown->keys(), 0, 21));
  CHECK(sharesKeys(tree->keys(), 21, grown->keys(), 23, 29));
  CHECK(store.collapseTree(5, 4, 20).has_value());
  CHECK(sharesKeys(tree->keys(), 0, store.snapshot(5)->keys(), 0, 50));
}

namespace {

/*
 * The keys of the nodes under nodes that have children, depth first.
 */
void collectParents(const std::vector<Node>& nodes, std::vector<std::string>& keys) {
  for (const Node& node : nodes) {
    if (!node.children.empty()) {
      keys.push_back(node.key);
      collectParents(node.children, keys);
    }
  }
}

/*
 * Fails unless tree holds exactly rows, by rowAt, by its key list and by its depths.
 */
void checkTree(const Trail& trail, const TreeRows& tree, const std::vector<TreeRow>& rows, const std::string& what) {
  KeyList keys = tree.keys();
  if (tree.size() != rows.size() || keys.size() != rows.size()) {
    trail.failWith(
      what + " holds " + std::to_string(tree.size()) + " rows and " + std::to_string(keys.size()) + " keys for " +
      std::to_string(rows.size()) + " rows of the tree");
  }
  for (std::size_t row = 0; row < rows.size(); ++row) {
    TreeRow actual = tree.rowAt(row);
    const TreeRow& expected = rows[row];
    if (
      actual.key != expected.key || keys[row] != expected.key || actual.depth != expected.depth ||
      tree.depthAt(row) != expected.depth || actual.hasChildren != expected.hasChildren ||
      actual.expanded != expected.expanded) {
      trail.failWith(
        what + " has " + actual.key + " at depth " + std::to_string(actual.depth) + " children " +
        std::to_string(actual.hasChildren) + " expanded " + std::to_string(actual.expanded) + " at row " +
        std::to_string(row) + " where the tree has " + expected.key + " at depth " + std::to_string(expected.depth) +
        " children " + std::to_string(expected.hasChildren) + " expanded " + std::to_string(expected.expanded));
    }
  }
}

}

/*
 * TreeRows against a full flatten of a random tree. Nodes expand and collapse in place and
 * from a base, an expanded node is walked again after its branches changed, leaves and rows
 * past the end are toggled, and the tree is read afresh. The rows are always the flatten, a
 * splice names the rows that left and came, a base and every earlier version keep their rows,
 * and a node's subtree ends at the next row that is not deeper.
 */
TEST(fuzz_tree_rows_are_the_flatten_of_the_tree_through_expands_collapses_and_resets) {
  const FuzzKinds kinds = {
    {"expand", "expand from a base", "expand again", "collapse", "collapse from a base",
     "collapse with nothing showing", "expand a leaf", "row past the end", "reset"},
    {},
  };
  forEachFuzzSeed(__func__, 100, kinds, [](std::uint32_t seed, FuzzEpisode& episode) {
    Trail trail(seed, episode);
    std::mt19937 rng(seed);
    std::size_t made = 0;
    std::function<Node(int)> build = [&](int level) {
      Node node{"n" + std::to_string(made++), {}};
      int children = level < 4 && chance(rng, level == 0 ? 90 : 55) ? pickInt(rng, 1, 4) : 0;
      for (int child = 0; child < children; ++child) {
        node.children.push_back(build(level + 1));
      }
      return node;
    };
    std::vector<Node> nodes;
    std::set<std::string> expanded;
    std::vector<TreeRow> rows;
    TreeRows tree;
    // Read the tree afresh: new nodes now and then, and a new pick of the branches that show.
    auto reset = [&](bool rebuild) {
      if (rebuild) {
        nodes.clear();
        int roots = pickInt(rng, 1, 5);
        for (int root = 0; root < roots; ++root) {
          nodes.push_back(build(0));
        }
      }
      std::vector<std::string> parents;
      collectParents(nodes, parents);
      expanded.clear();
      for (const std::string& key : parents) {
        if (chance(rng, 40)) {
          expanded.insert(key);
        }
      }
      rows.clear();
      flatten(nodes, 0, expanded, rows);
      tree.reset(rows);
    };
    reset(true);
    trail.note("open with " + std::to_string(rows.size()) + " rows");

    // Earlier versions of the tree and the rows each one had.
    std::vector<std::pair<TreeRows, std::vector<TreeRow>>> earlier;
    for (int step = 0; step < 30; ++step) {
      int roll = pickPercent(rng);
      std::vector<TreeRow> previous = rows;
      std::size_t row = pickSize(rng, 0, rows.size() - 1);
      // Half the time the row is one that has children.
      std::vector<std::size_t> parents;
      for (std::size_t index = 0; index < rows.size(); ++index) {
        if (rows[index].hasChildren) {
          parents.push_back(index);
        }
      }
      if (!parents.empty() && chance(rng, 50)) {
        row = parents[pickSize(rng, 0, parents.size() - 1)];
      }
      TreeRow target = rows[row];
      // The rows showing under the row now.
      std::size_t showing = 0;
      while (row + 1 + showing < rows.size() && rows[row + 1 + showing].depth > target.depth) {
        ++showing;
      }
      bool fromBase = chance(rng, 40);
      TreeSplice splice;
      TreeSplice expected{row + 1, 0, 0};

      if (roll < 8) {
        episode.reach("row past the end");
        row = rows.size() + pickSize(rng, 0, 3);
        trail.note("toggle row " + std::to_string(row) + " past the end");
        splice = chance(rng, 50) ? tree.expand(row, {{"x", 1, false, false}}) : tree.collapse(row);
        expected = {rows.size(), 0, 0};
      } else if (roll < 16) {
        bool rebuild = chance(rng, 40);
        episode.reach("reset");
        reset(rebuild);
        trail.note(
          std::string("reset") + (rebuild ? " with new nodes" : "") + " to " + std::to_string(rows.size()) + " rows");
        checkTree(trail, tree, rows, "the tree after a reset");
        for (const auto& [version, held] : earlier) {
          checkTree(trail, version, held, "an earlier version after a reset");
        }
        continue;
      } else if (roll < 60) {
        std::vector<TreeRow> under;
        if (!target.hasChildren) {
          episode.reach("expand a leaf");
          under.push_back({"x", target.depth + 1, false, false});
        } else {
          std::uint32_t depth = 0;
          const Node* node = findNode(nodes, target.key, depth);
          if (target.expanded) {
            // A branch under it opened or closed in the data since it was walked.
            episode.reach("expand again");
            std::vector<std::string> below;
            collectParents(node->children, below);
            if (!below.empty()) {
              const std::string& key = below[pickSize(rng, 0, below.size() - 1)];
              if (expanded.erase(key) == 0) {
                expanded.insert(key);
              }
            }
          } else {
            episode.reach(fromBase ? "expand from a base" : "expand");
          }
          expanded.insert(target.key);
          flatten(node->children, depth + 1, expanded, under);
          expected = {row + 1, showing, under.size()};
        }
        trail.note(
          "expand row " + std::to_string(row) + " " + target.key + " with " + std::to_string(under.size()) + " rows" +
          (fromBase ? " from a base" : ""));
        if (fromBase) {
          TreeRows next;
          splice = next.expandFrom(tree, row, under);
          checkTree(trail, tree, previous, "the base of an expand");
          tree = std::move(next);
        } else {
          splice = tree.expand(row, under);
        }
      } else {
        if (target.hasChildren && target.expanded) {
          episode.reach(fromBase ? "collapse from a base" : "collapse");
          expected = {row + 1, showing, 0};
        } else {
          episode.reach("collapse with nothing showing");
        }
        expanded.erase(target.key);
        trail.note("collapse row " + std::to_string(row) + " " + target.key + (fromBase ? " from a base" : ""));
        if (fromBase) {
          TreeRows next;
          splice = next.collapseFrom(tree, row);
          checkTree(trail, tree, previous, "the base of a collapse");
          tree = std::move(next);
        } else {
          splice = tree.collapse(row);
        }
      }

      rows.clear();
      flatten(nodes, 0, expanded, rows);
      checkTree(trail, tree, rows, "the tree");
      bool asExpected =
        splice.start == expected.start && splice.deleted == expected.deleted && splice.inserted == expected.inserted;
      if (!asExpected) {
        trail.failWith(
          "the splice is " + std::to_string(splice.deleted) + " out and " + std::to_string(splice.inserted) +
          " in at " + std::to_string(splice.start) + " expected " + std::to_string(expected.deleted) + " out and " +
          std::to_string(expected.inserted) + " in at " + std::to_string(expected.start));
      }
      // The rows before the splice and after it are the ones the tree had.
      bool spliced = previous.size() - splice.deleted + splice.inserted == rows.size();
      for (std::size_t index = 0; spliced && index < previous.size(); ++index) {
        bool before = index < splice.start;
        if (before || index >= splice.start + splice.deleted) {
          std::size_t after = before ? index : index - splice.deleted + splice.inserted;
          spliced = rows[after].key == previous[index].key;
        }
      }
      if (!spliced) {
        trail.failWith("the splice does not turn the rows before into the rows now");
      }
      for (int sample = 0; sample < 3 && !rows.empty(); ++sample) {
        std::size_t from = pickSize(rng, 0, rows.size() - 1);
        std::size_t end = from + 1;
        while (end < rows.size() && rows[end].depth > rows[from].depth) {
          ++end;
        }
        if (tree.subtreeEnd(from) != end) {
          trail.failWith(
            "the subtree of row " + std::to_string(from) + " ends at " + std::to_string(tree.subtreeEnd(from)) +
            " expected " + std::to_string(end));
        }
      }
      if (tree.subtreeEnd(rows.size() + 2) != rows.size()) {
        trail.failWith("the subtree of a row past the end does not end with the rows");
      }
      for (const auto& [version, held] : earlier) {
        checkTree(trail, version, held, "an earlier version");
      }
      if (step % 6 == 0) {
        if (earlier.size() == 4) {
          earlier.erase(earlier.begin());
        }
        earlier.emplace_back(tree, rows);
      }
      episode.observe(rows.size());
      episode.observe(splice.start);
      episode.observe(splice.deleted);
      episode.observe(splice.inserted);
    }
  });
}
