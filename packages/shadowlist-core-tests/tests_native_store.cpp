/*
 * Tests for the ShadowListNative data store and the host JSON value it holds items in.
 */

#include "TestFramework.hpp"

#include <shadowlist-core/host/JsonValue.hpp>
#include <shadowlist-core/host/NativeStore.hpp>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using namespace slt;
using namespace azimgd::shadowlist;

namespace {

JsonValue parsed(const char* text) {
  auto value = JsonValue::parse(text);
  if (!value) {
    ::slt::fail(std::string("bad JSON in test: ") + text);
  }
  return *value;
}

std::vector<std::string> keysOf(const NativeStore& store) {
  return *store.keys();
}

std::uint64_t versionOf(const NativeStore& store, const std::string& key) {
  NativeRow scratch;
  std::size_t position = 0;
  const NativeRow* row = store.find(key, scratch, position);
  return row ? row->version : 0;
}

}

TEST(jsonParsesEveryKind) {
  auto value = parsed(R"( {"n":null,"t":true,"f":false,"i":-12,"d":2.5e1,"s":"a\"b\\c\né😀","a":[1,[2]],"o":{}} )");
  CHECK(value.isObject());
  CHECK(value.find("n")->isNull());
  CHECK(value.find("t")->getBool());
  CHECK(!value.find("f")->getBool());
  CHECK(value.find("i")->isInt());
  CHECK_EQ(value.find("i")->getInt(), std::int64_t{-12});
  CHECK(value.find("d")->isDouble());
  CHECK_EQ(value.find("d")->getDouble(), 25.0);
  CHECK_EQ(value.find("s")->getString(), std::string("a\"b\\c\n\xC3\xA9\xF0\x9F\x98\x80"));
  CHECK_EQ(value.find("a")->size(), std::size_t{2});
  CHECK_EQ((*value.find("a"))[1][0].getInt(), std::int64_t{2});
  CHECK(value.find("o")->isObject());
  CHECK(value.find("o")->empty());
  CHECK(value.find("missing") == nullptr);
}

TEST(jsonRejectsInvalidText) {
  CHECK(!JsonValue::parse("").has_value());
  CHECK(!JsonValue::parse("{").has_value());
  CHECK(!JsonValue::parse("[1,]").has_value());
  CHECK(!JsonValue::parse("{\"a\" 1}").has_value());
  CHECK(!JsonValue::parse("\"open").has_value());
  CHECK(!JsonValue::parse("tru").has_value());
  CHECK(!JsonValue::parse("1 2").has_value());
  CHECK(!JsonValue::parse("-").has_value());
  CHECK(JsonValue::parse(" [] ").has_value());
}

TEST(jsonComparesNumbersByValueAndObjectsWithoutOrder) {
  CHECK(JsonValue(1) == JsonValue(1.0));
  CHECK(JsonValue(1) != JsonValue(1.5));
  CHECK(JsonValue("1") != JsonValue(1));
  CHECK(parsed(R"({"a":1,"b":[true]})") == parsed(R"({"b":[true],"a":1.0})"));
  CHECK(parsed(R"({"a":1})") != parsed(R"({"a":1,"b":null})"));
  CHECK(parsed("[1,2]") != parsed("[2,1]"));
}

TEST(jsonMergesFieldsAndErasesThem) {
  JsonValue item = JsonValue::object();
  item["a"] = 1;
  item["b"] = "x";
  item["a"] = 2;
  CHECK_EQ(item.size(), std::size_t{2});
  CHECK_EQ(item.find("a")->getInt(), std::int64_t{2});
  CHECK(item.erase("a"));
  CHECK(!item.erase("a"));
  CHECK_EQ(item.size(), std::size_t{1});
  // Writing a field into a non object makes it an object.
  JsonValue scalar = 3;
  scalar["x"] = true;
  CHECK(scalar.isObject());
}

TEST(nativeStoreSetDataKeepsVersionsOfUnchangedRows) {
  NativeStore store;
  CHECK(store.setData(parsed(R"([{"t":"a"},{"t":"b"},{"t":"c"}])"), {"a", "b", "c"}, {}));
  CHECK_EQ(store.size(), std::size_t{3});
  CHECK(keysOf(store) == std::vector<std::string>({"a", "b", "c"}));
  std::uint64_t keysVersion = store.keysVersion();
  std::uint64_t versionA = versionOf(store, "a");
  std::uint64_t versionB = versionOf(store, "b");

  // Same keys, one changed item: no new key list, only b rebinds.
  CHECK(!store.setData(parsed(R"([{"t":"a"},{"t":"B"},{"t":"c"}])"), {"a", "b", "c"}, {}));
  CHECK_EQ(store.keysVersion(), keysVersion);
  CHECK_EQ(versionOf(store, "a"), versionA);
  CHECK(versionOf(store, "b") != versionB);

  // A new template also rebinds, and duplicate keys keep their first row.
  CHECK(store.setData(parsed(R"([{"t":"a"},{"t":"x"},{"t":"B"}])"), {"a", "a", "b"}, {"other"}));
  CHECK(keysOf(store) == std::vector<std::string>({"a", "b"}));
  CHECK(versionOf(store, "a") != versionA);
  CHECK(store.keysVersion() > keysVersion);
  CHECK_EQ(store.item("a").find("t")->getString(), std::string("a"));

  // Items and keys pair up to the shorter list, and a non array holds nothing.
  store.setData(parsed(R"([{},{}])"), {"only"}, {});
  CHECK_EQ(store.size(), std::size_t{1});
  store.setData(parsed(R"({"a":1})"), {"a"}, {});
  CHECK_EQ(store.size(), std::size_t{0});
}

TEST(nativeStoreInsertRemoveAndMove) {
  NativeStore store;
  store.setData(parsed("[1,2,3]"), {"a", "b", "c"}, {});
  // Existing and repeated keys are skipped, and past the end appends.
  CHECK(store.insertItems(1, parsed("[10,11,12]"), {"x", "a", "x"}, {}));
  CHECK(keysOf(store) == std::vector<std::string>({"a", "x", "b", "c"}));
  CHECK(store.insertItems(99, parsed("[4]"), {"d"}, {}));
  CHECK(keysOf(store) == std::vector<std::string>({"a", "x", "b", "c", "d"}));
  CHECK(!store.insertItems(0, parsed("[1]"), {"a"}, {}));

  std::uint64_t keysVersion = store.keysVersion();
  CHECK(store.removeItems({"x", "missing"}));
  CHECK(store.keysVersion() > keysVersion);
  CHECK(!store.removeItems({"missing"}));
  CHECK(keysOf(store) == std::vector<std::string>({"a", "b", "c", "d"}));

  CHECK(store.moveItem("a", 2) == NativeStore::MoveResult::Moved);
  CHECK(keysOf(store) == std::vector<std::string>({"b", "c", "a", "d"}));
  CHECK(store.moveItem("d", 99) == NativeStore::MoveResult::Unchanged);
  CHECK(store.moveItem("missing", 0) == NativeStore::MoveResult::Missing);
  CHECK_EQ(*store.indexOf("a"), std::size_t{2});
  CHECK(!store.indexOf("x").has_value());
}

TEST(nativeStoreUpdateMergesOrReplaces) {
  NativeStore store;
  store.setData(parsed(R"([{"a":1,"b":2},"text"])"), {"k", "s"}, {});
  std::uint64_t version = versionOf(store, "k");
  CHECK(store.updateItem("k", parsed(R"({"b":null,"c":3})"), "", false));
  // Stored rows keep null fields as null.
  CHECK(store.item("k") == parsed(R"({"a":1,"b":null,"c":3})"));
  CHECK(versionOf(store, "k") > version);
  CHECK(store.updateItem("k", parsed(R"({"z":1})"), "big", true));
  CHECK(store.item("k") == parsed(R"({"z":1})"));
  NativeRow scratch;
  std::size_t position = 0;
  CHECK_EQ(store.find("k", scratch, position)->templateName, std::string("big"));
  // A patch on a non object row replaces it.
  CHECK(store.updateItem("s", parsed(R"({"a":1})"), "", false));
  CHECK(store.item("s") == parsed(R"({"a":1})"));
  CHECK(!store.updateItem("missing", JsonValue::object(), "", false));
}

TEST(nativeStoreIndexedRowsByPosition) {
  NativeStore store;
  std::vector<std::int32_t> order = {30, 10, 20};
  CHECK(store.setIndexed(3, order, "row", "value", {1}, parsed(R"([{"note":"x"}])"), {"special"}, {}));
  CHECK(store.indexed());
  CHECK(keysOf(store) == std::vector<std::string>({"0", "1", "2"}));
  CHECK(store.item("0") == parsed(R"({"row":0,"value":30})"));
  CHECK(store.item("1") == parsed(R"({"note":"x","row":1,"value":10})"));
  CHECK(store.item("3").isNull());
  // Leading zeros and non numbers are not rows.
  CHECK(!store.indexOf("01").has_value());
  CHECK(!store.indexOf("x").has_value());

  NativeRow scratch;
  std::size_t position = 0;
  const NativeRow* extra = store.find("1", scratch, position);
  CHECK_EQ(extra->templateName, std::string("special"));
  std::uint64_t extraVersion = extra->version;
  std::uint64_t plainVersion = store.find("0", scratch, position)->version;

  // Same order and extras: nothing changes, not even the keys.
  std::uint64_t keysVersion = store.keysVersion();
  CHECK(!store.setIndexed(3, order, "row", "value", {1}, parsed(R"([{"note":"x"}])"), {"special"}, {}));
  CHECK_EQ(store.keysVersion(), keysVersion);
  CHECK_EQ(store.find("1", scratch, position)->version, extraVersion);
  CHECK_EQ(store.find("0", scratch, position)->version, plainVersion);

  // A new order rebinds every row but keeps the keys.
  CHECK(!store.setIndexed(3, {1, 2, 3}, "row", "value", {}, JsonValue::array(), {}, {}));
  CHECK(store.find("0", scratch, position)->version != plainVersion);
  CHECK_EQ(store.item("2").find("value")->getInt(), std::int64_t{3});

  // Growing keeps the first keys and adds the rest.
  CHECK(store.setIndexed(5, {}, "row", "", {}, JsonValue::array(), {}, {}));
  CHECK(keysOf(store) == std::vector<std::string>({"0", "1", "2", "3", "4"}));
  CHECK(store.item("4") == parsed(R"({"row":4})"));

  // Updates merge into the extra data, and a null field drops it.
  CHECK(store.updateItem("2", parsed(R"({"a":1,"b":2})"), "", false));
  CHECK(store.updateItem("2", parsed(R"({"a":null})"), "", false));
  CHECK(store.item("2") == parsed(R"({"b":2,"row":2})"));
  // Insert, remove and move do nothing for indexed rows.
  CHECK(!store.insertItems(0, parsed("[1]"), {"x"}, {}));
  CHECK(!store.removeItems({"1"}));
  CHECK(store.moveItem("1", 0) == NativeStore::MoveResult::Missing);
}

TEST(nativeStoreIndexedRowsById) {
  NativeStore store;
  CHECK(store.setIndexed(3, {}, "row", "", {}, JsonValue::array(), {}, {7, 9, 7}));
  // A repeated id keeps its first row, and later ones get a key nothing finds.
  CHECK(keysOf(store) == std::vector<std::string>({"7", "9", "#dup2"}));
  CHECK_EQ(*store.indexOf("9"), std::size_t{1});
  CHECK(!store.indexOf("#dup2").has_value());
  CHECK(!store.indexOf("8").has_value());
  CHECK(store.item("9") == parsed(R"({"row":1})"));

  // Ids that don't match the count are dropped, which keys rows by position again.
  CHECK(store.setIndexed(3, {}, "row", "", {}, JsonValue::array(), {}, {1, 2}));
  CHECK(keysOf(store) == std::vector<std::string>({"0", "1", "2"}));

  // Back to stored rows always publishes new keys, even when they read the same.
  std::uint64_t keysVersion = store.keysVersion();
  CHECK(store.setData(parsed("[1,2,3]"), {"0", "1", "2"}, {}));
  CHECK(!store.indexed());
  CHECK(store.keysVersion() > keysVersion);
  CHECK(store.item("1") == JsonValue(2));
}

TEST(nativeStoreEvictsTheOldestIdleRows) {
  std::unordered_map<std::string, std::uint64_t> usedAt = {
    {"a", 1}, {"b", 5}, {"c", 3}, {"d", 9}, {"e", 9}, {"f", 2}};
  auto clock = [](std::uint64_t used) { return used; };
  // Rows used now (9) are never dropped. Of the idle ones the two newest stay.
  auto doomed = staleNativeRows(usedAt, 9, 2, clock);
  std::sort(doomed.begin(), doomed.end());
  CHECK(doomed == std::vector<std::string>({"a", "f"}));
  // Under the cap, or with few enough idle rows, nothing goes.
  CHECK(staleNativeRows(usedAt, 9, 6, clock).empty());
  CHECK(staleNativeRows(usedAt, 9, 4, clock).empty());
  CHECK_EQ(staleNativeRows(usedAt, 9, 0, clock).size(), std::size_t{4});
}
