#pragma once

#include <jsi/jsi.h>

#include <shadowlist-core/host/ListSelection.hpp>
#include <shadowlist-core/host/RowKeyStore.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace facebook::react::shadowlist {

/*
 * The row keys and row size specs a list's JS writes for its native view, by version. They
 * never travel as props. JS makes one store per list with createStore and passes the store's id
 * in the keysStore prop, and the version its render wrote in keysVersion and sizeSpecsVersion.
 * The component descriptor finds the store by id, see findRowKeyStore. Stores live as long as
 * JS holds the handle or a node family holds the store.
 *
 * The functions, all synchronous on the JS thread:
 *   createStore() -> handle with an id
 *   setKeys(store, version, keys) -> bool
 *   spliceKeys(store, version, baseVersion, start, deleteCount, inserted) -> bool
 *   setSections(store, version, baseVersion, flags, sectionKeys, itemCounts, reuse, itemKeys) -> bool
 *     flags: 1 headers, 2 footers, 4 sticky headers, 8 item keys qualified by section
 *   resetTree(store, version, keys, depths, flags) -> bool, flags per row: 1 children, 2 expanded
 *   expandTree(store, version, baseVersion, row, keys, depths, flags) -> rows deleted, or -1
 *   collapseTree(store, version, baseVersion, row) -> rows deleted, or -1
 *   indexOfKey(store, version, key, hint) -> row, -1 when absent, -2 when the version is gone
 *   setSizeSpecs(store, version, keys, specs) -> undefined, specs being ItemSizeSpec objects for
 *     the rows of keys
 * A write fails when its base version is gone, and JS then writes everything.
 *
 * Each store also holds its list's selection, the core's ListSelection. Every host keeps one
 * implementation of the selection rules. The calls that change it return the selected keys in
 * the order they were selected, or null when nothing changed:
 *   setSelection(store, keys, multiple) -> undefined, the keys JS holds now, like a controlled
 *     selectedKeys
 *   selectKey(store, key) -> keys or null, single selection deselecting every other row
 *   selectOnlyKey(store, key) -> keys or null
 *   deselectKey(store, key) -> keys or null
 *   retainSelection(store, version) -> keys or null, dropping keys the version's rows lack
 *   selectedIndices(store, version) -> rows of the selected keys, low to high
 */
std::shared_ptr<azimgd::shadowlist::RowKeyStore> findRowKeyStore(std::int64_t id);

/*
 * One function above by name, or undefined.
 */
jsi::Value storeFunction(jsi::Runtime& runtime, const std::string& name);

/*
 * Every function above on one object, for hosts without a TurboModule like the benchmark.
 */
jsi::Object createStoreModuleObject(jsi::Runtime& runtime);

/*
 * The TurboModule name JS looks the functions up by.
 */
constexpr const char* STORE_MODULE_NAME = "ShadowListStore";

}
