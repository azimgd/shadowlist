#include "TestFramework.hpp"

#include <shadowlist-core/host/TreeSource.hpp>

using namespace slt;
using namespace azimgd::shadowlist;

TEST(tree_source_keeps_hidden_expansion_intent_and_splices_only_the_branch) {
  TreeSource source;
  source.reset({{"root", 0, true}, {"branch", 1, true}, {"leaf", 2}, {"sibling", 1}, {"other", 0}});
  source.setExpandedKeys({"branch", "missing", "leaf"});
  CHECK_EQ(source.rows().size(), std::size_t{2});
  CHECK_EQ(source.expandedKeys().size(), std::size_t{3});
  CHECK_EQ(source.expandedKeys()[0], std::string{"branch"});
  TreeSplice opened = source.expand(0);
  CHECK_EQ(opened.start, std::size_t{1});
  CHECK_EQ(opened.deleted, std::size_t{0});
  CHECK_EQ(opened.inserted, std::size_t{3});
  CHECK_EQ(source.rows().size(), std::size_t{5});
  CHECK_EQ(source.rows().rowAt(2).key, std::string{"leaf"});
  CHECK_EQ(source.rows().rowAt(2).depth, std::uint32_t{2});
  CHECK(source.rows().rowAt(1).expanded);
  TreeSplice closed = source.collapse(0);
  CHECK_EQ(closed.deleted, std::size_t{3});
  CHECK_EQ(source.rows().rowAt(1).key, std::string{"other"});
  CHECK_EQ(source.expandedKeys()[0], std::string{"branch"});
  source.expand(0);
  CHECK_EQ(source.rows().rowAt(2).key, std::string{"leaf"});
  source.collapse(1);
  CHECK_EQ(source.rows().rowAt(2).key, std::string{"sibling"});
}

TEST(tree_source_replaces_topology_and_rebuilds_controlled_expanded_keys) {
  TreeSource source;
  source.reset({{"a", 0, true, true}, {"child", 1}, {"b", 0, true, true}, {"last", 1}});
  CHECK_EQ(source.rows().size(), std::size_t{4});
  source.setExpandedKeys({"b"});
  CHECK_EQ(source.rows().size(), std::size_t{3});
  CHECK_EQ(source.rows().rowAt(1).key, std::string{"b"});
  CHECK_EQ(source.rows().rowAt(2).key, std::string{"last"});
  CHECK_EQ(source.expand(2).inserted, std::size_t{0});
  CHECK_EQ(source.expand(50).start, source.rows().size());
  CHECK_EQ(source.collapse(50).deleted, std::size_t{0});
  source.reset({{"new", 0}});
  CHECK_EQ(source.expandedKeys()[0], std::string{"b"});
  CHECK_EQ(source.rows().size(), std::size_t{1});
  source.reset({});
  CHECK_EQ(source.rows().size(), std::size_t{0});
}

TEST(tree_source_keeps_unknown_and_leaf_expansion_for_future_children) {
  TreeSource source;
  source.reset({{"leaf", 0}});
  source.setExpandedKeys({"leaf", "future"});
  CHECK(!source.rows().rowAt(0).expanded);
  source.reset({{"leaf", 0, true}, {"future", 1, true}, {"loaded", 2}});
  CHECK_EQ(source.rows().size(), std::size_t{3});
  CHECK(source.rows().rowAt(0).expanded);
  CHECK(source.rows().rowAt(1).expanded);
  source.collapse(0);
  CHECK_EQ(source.expandedKeys().size(), std::size_t{1});
  CHECK_EQ(source.expandedKeys()[0], std::string{"future"});
  source.setExpandedKeys({});
  source.expand(0);
  CHECK_EQ(source.rows().size(), std::size_t{2});
  CHECK(!source.rows().rowAt(1).expanded);
}

TEST(tree_source_composes_controlled_toggle_proposals_and_restores_rendered_rows) {
  TreeSource source;
  source.reset({{"a", 0, true}, {"child", 1}, {"b", 0, true}, {"other", 1}});
  source.toggle("a");
  auto proposal = source.expandedKeys();
  source.setExpandedKeys({});
  CHECK_EQ(source.rows().size(), std::size_t{2});
  source.setExpandedKeys(proposal);
  source.toggle("b");
  proposal = source.expandedKeys();
  CHECK_EQ(proposal.size(), std::size_t{2});
  source.setExpandedKeys({});
  CHECK_EQ(source.rows().size(), std::size_t{2});
  source.setExpandedKeys(proposal);
  CHECK_EQ(source.rows().size(), std::size_t{4});
  source.toggle("notLoaded");
  source.toggle("child");
  CHECK_EQ(source.expandedKeys().size(), std::size_t{4});
  CHECK(!source.rows().rowAt(1).expanded);
}
