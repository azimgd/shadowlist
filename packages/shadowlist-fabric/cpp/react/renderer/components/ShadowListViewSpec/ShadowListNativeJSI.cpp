#include "ShadowListNativeJSI.h"

#include "ShadowListNativeEngine.h"

#include <jsi/JSIDynamic.h>
#include <jsi/jsi.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace facebook::react {

namespace {

constexpr const char* GLOBAL_NAME = "__shadowListNative";

using HostFunction = std::function<jsi::Value(jsi::Runtime&, const jsi::Value*, std::size_t)>;

std::string stringArgument(jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count, std::size_t index) {
  if (index >= count || !arguments[index].isString()) {
    return {};
  }
  return arguments[index].getString(runtime).utf8(runtime);
}

std::vector<std::string> stringsArgument(jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count, std::size_t index) {
  std::vector<std::string> strings;
  if (index >= count || !arguments[index].isObject()) {
    return strings;
  }
  auto object = arguments[index].getObject(runtime);
  if (!object.isArray(runtime)) {
    return strings;
  }
  auto array = object.getArray(runtime);
  std::size_t size = array.size(runtime);
  strings.reserve(size);
  for (std::size_t item = 0; item < size; ++item) {
    auto value = array.getValueAtIndex(runtime, item);
    strings.push_back(value.isString() ? value.getString(runtime).utf8(runtime) : std::string{});
  }
  return strings;
}

folly::dynamic dynamicArgument(jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count, std::size_t index) {
  if (index >= count || arguments[index].isUndefined()) {
    return nullptr;
  }
  return jsi::dynamicFromValue(runtime, arguments[index]);
}

/*
 * Copies an Int32Array out of its buffer in one go instead of one JSI call per element.
 * Returns an empty list for anything else.
 */
std::vector<std::int32_t> int32ArrayArgument(
  jsi::Runtime& runtime,
  const jsi::Value* arguments,
  std::size_t count,
  std::size_t index) {
  std::vector<std::int32_t> values;
  if (index >= count || !arguments[index].isObject()) {
    return values;
  }
  auto view = arguments[index].getObject(runtime);
  auto bufferValue = view.getProperty(runtime, "buffer");
  if (!bufferValue.isObject()) {
    return values;
  }
  auto bufferObject = bufferValue.getObject(runtime);
  if (!bufferObject.isArrayBuffer(runtime)) {
    return values;
  }
  auto buffer = bufferObject.getArrayBuffer(runtime);
  auto offsetValue = view.getProperty(runtime, "byteOffset");
  auto lengthValue = view.getProperty(runtime, "length");
  auto bytesValue = view.getProperty(runtime, "BYTES_PER_ELEMENT");
  if (!offsetValue.isNumber() || !lengthValue.isNumber() || !bytesValue.isNumber() || bytesValue.getNumber() != 4) {
    return values;
  }
  // Checked as numbers first, since casting NaN or a huge value is undefined behavior.
  double offsetNumber = offsetValue.getNumber();
  double lengthNumber = lengthValue.getNumber();
  double bufferSize = static_cast<double>(buffer.size(runtime));
  if (!(offsetNumber >= 0.0) || !(lengthNumber > 0.0) || offsetNumber + lengthNumber * 4.0 > bufferSize) {
    return values;
  }
  auto offset = static_cast<std::size_t>(offsetNumber);
  auto length = static_cast<std::size_t>(lengthNumber);
  values.resize(length);
  std::memcpy(values.data(), buffer.data(runtime) + offset, length * 4);
  return values;
}

double numberArgument(const jsi::Value* arguments, std::size_t count, std::size_t index, double fallback) {
  return index < count && arguments[index].isNumber() ? arguments[index].getNumber() : fallback;
}

/*
 * A non-negative whole number up to limit. NaN, infinity and negatives give fallback and bigger
 * values give limit, since casting those to an integer is undefined behavior.
 */
std::size_t sizeArgument(double value, std::size_t limit, std::size_t fallback) {
  if (!(value >= 0.0) || !std::isfinite(value)) {
    return fallback;
  }
  return value >= static_cast<double>(limit) ? limit : static_cast<std::size_t>(value);
}

// Row counts come from 32-bit typed arrays, so nothing bigger is valid.
constexpr std::size_t MAX_ROWS = static_cast<std::size_t>(INT32_MAX);

std::vector<std::size_t> indicesArgument(jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count, std::size_t index) {
  std::vector<std::size_t> indices;
  if (index >= count || !arguments[index].isObject()) {
    return indices;
  }
  auto object = arguments[index].getObject(runtime);
  if (!object.isArray(runtime)) {
    return indices;
  }
  auto array = object.getArray(runtime);
  std::size_t size = array.size(runtime);
  indices.reserve(size);
  for (std::size_t item = 0; item < size; ++item) {
    auto value = array.getValueAtIndex(runtime, item);
    indices.push_back(value.isNumber() ? sizeArgument(value.getNumber(), SIZE_MAX, SIZE_MAX) : SIZE_MAX);
  }
  return indices;
}

void define(jsi::Runtime& runtime, jsi::Object& target, const char* name, unsigned int length, HostFunction function) {
  target.setProperty(
    runtime,
    name,
    jsi::Function::createFromHostFunction(
      runtime,
      jsi::PropNameID::forAscii(runtime, name),
      length,
      [function = std::move(function)](
        jsi::Runtime& runtime, const jsi::Value&, const jsi::Value* arguments, std::size_t count) -> jsi::Value {
        return function(runtime, arguments, count);
      }));
}

/*
 * Returned by open. It keeps the engine alive while JS holds it and goes away with a discarded
 * render or a reload, so engines don't leak. close lets go at unmount without waiting for GC.
 */
class EngineHandle final : public jsi::HostObject {
public:
  explicit EngineHandle(std::shared_ptr<ShadowListNativeEngine> engine) : engine_(std::move(engine)) {}
  void close() {
    engine_.reset();
  }

private:
  std::shared_ptr<ShadowListNativeEngine> engine_;
};

void install(jsi::Runtime& runtime) {
  auto global = runtime.global();
  if (global.hasProperty(runtime, GLOBAL_NAME)) {
    return;
  }
  jsi::Object binding(runtime);

  // setData(listId, items, keys, templates, scrollToStart) returns the row count.
  define(runtime, binding, "setData", 5, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    bool scrollToStart = count > 4 && arguments[4].isBool() && arguments[4].getBool();
    auto size = engine->setData(
      dynamicArgument(runtime, arguments, count, 1),
      stringsArgument(runtime, arguments, count, 2),
      stringsArgument(runtime, arguments, count, 3),
      scrollToStart);
    return jsi::Value(static_cast<double>(size));
  });

  /*
   * setIndexed(listId, count, order, indexField, valueField, extraIndices, extraItems,
   * extraTemplates, scrollToStart, ids) returns the row count. Order and ids are Int32Arrays.
   */
  define(runtime, binding, "setIndexed", 10, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    double rows = numberArgument(arguments, count, 1, 0);
    bool scrollToStart = count > 8 && arguments[8].isBool() && arguments[8].getBool();
    auto size = engine->setIndexed(
      sizeArgument(rows, MAX_ROWS, 0),
      int32ArrayArgument(runtime, arguments, count, 2),
      stringArgument(runtime, arguments, count, 3),
      stringArgument(runtime, arguments, count, 4),
      indicesArgument(runtime, arguments, count, 5),
      dynamicArgument(runtime, arguments, count, 6),
      stringsArgument(runtime, arguments, count, 7),
      scrollToStart,
      int32ArrayArgument(runtime, arguments, count, 9));
    return jsi::Value(static_cast<double>(size));
  });

  /*
   * insertItems(listId, index, items, keys, templates) returns the row count.
   * An index past the end appends.
   */
  define(runtime, binding, "insertItems", 5, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    double index = numberArgument(arguments, count, 1, -1);
    auto size = engine->insertItems(
      sizeArgument(index, SIZE_MAX, SIZE_MAX),
      dynamicArgument(runtime, arguments, count, 2),
      stringsArgument(runtime, arguments, count, 3),
      stringsArgument(runtime, arguments, count, 4));
    return jsi::Value(static_cast<double>(size));
  });

  // updateItem(listId, key, patch, template, replace) returns whether the row was found.
  define(runtime, binding, "updateItem", 5, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(false);
    }
    bool replace = count > 4 && arguments[4].isBool() && arguments[4].getBool();
    bool updated = engine->updateItem(
      stringArgument(runtime, arguments, count, 1),
      dynamicArgument(runtime, arguments, count, 2),
      stringArgument(runtime, arguments, count, 3),
      replace);
    return jsi::Value(updated);
  });

  // removeItems(listId, keys) returns the row count.
  define(runtime, binding, "removeItems", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    auto size = engine->removeItems(stringsArgument(runtime, arguments, count, 1));
    return jsi::Value(static_cast<double>(size));
  });

  // moveItem(listId, key, toIndex) returns whether the row moved.
  define(runtime, binding, "moveItem", 3, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(false);
    }
    double toIndex = numberArgument(arguments, count, 2, 0);
    bool moved = engine->moveItem(
      stringArgument(runtime, arguments, count, 1), sizeArgument(toIndex, SIZE_MAX, 0));
    return jsi::Value(moved);
  });

  // scrollToIndex(listId, index, viewPosition). Pass -1 for the end or -2 for the start.
  define(runtime, binding, "scrollToIndex", 3, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    double index = numberArgument(arguments, count, 1, -1);
    double viewPosition = numberArgument(arguments, count, 2, 0);
    if (std::isnan(index)) {
      return jsi::Value::undefined();
    }
    // Keep -1 (end) and -2 (start), and a row index the engine can cast to size_t.
    engine->requestScroll(
      index < 0 ? index : static_cast<double>(sizeArgument(index, MAX_ROWS, 0)),
      std::isfinite(viewPosition) ? viewPosition : 0.0);
    return jsi::Value::undefined();
  });

  // setTemplateStyle(listId, template, elementId, style). A null style clears it.
  define(runtime, binding, "setTemplateStyle", 4, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    engine->setTemplateStyle(
      stringArgument(runtime, arguments, count, 1),
      stringArgument(runtime, arguments, count, 2),
      dynamicArgument(runtime, arguments, count, 3));
    return jsi::Value::undefined();
  });

  // configure(listId, options) takes initialRows, padRows and cacheRows.
  define(runtime, binding, "configure", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    engine->configure(dynamicArgument(runtime, arguments, count, 1));
    return jsi::Value::undefined();
  });

  // getItem(listId, key) returns the item or undefined.
  define(runtime, binding, "getItem", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    auto item = engine->getItem(stringArgument(runtime, arguments, count, 1));
    return item.isNull() ? jsi::Value::undefined() : jsi::valueFromDynamic(runtime, item);
  });

  // getKeys(listId) returns every row key.
  define(runtime, binding, "getKeys", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    auto keys = engine ? engine->getKeys() : std::vector<std::string>{};
    auto array = jsi::Array(runtime, keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
      array.setValueAtIndex(runtime, index, jsi::String::createFromUtf8(runtime, keys[index]));
    }
    return jsi::Value(runtime, array);
  });

  // getCount(listId) returns the row count.
  define(runtime, binding, "getCount", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    return jsi::Value(engine ? static_cast<double>(engine->size()) : 0.0);
  });

  // resolveTag(listId, tag) returns the row's key, index and repeatIndex, or null.
  define(runtime, binding, "resolveTag", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::null();
    }
    double tag = numberArgument(arguments, count, 1, 0);
    if (!(tag >= static_cast<double>(INT_MIN) && tag <= static_cast<double>(INT_MAX))) {
      return jsi::Value::null();
    }
    auto hit = engine->resolveTag(static_cast<Tag>(tag));
    if (!hit) {
      return jsi::Value::null();
    }
    jsi::Object result(runtime);
    result.setProperty(runtime, "key", jsi::String::createFromUtf8(runtime, hit->key));
    result.setProperty(runtime, "index", static_cast<double>(hit->index));
    result.setProperty(runtime, "repeatIndex", static_cast<double>(hit->repeatIndex));
    return jsi::Value(runtime, result);
  });

  // open(listId) returns a handle that keeps the list's engine alive while held.
  define(runtime, binding, "open", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::open(stringArgument(runtime, arguments, count, 0));
    return jsi::Value(
      runtime, jsi::Object::createFromHostObject(runtime, std::make_shared<EngineHandle>(std::move(engine))));
  });
  // close(handle) lets go of the engine right away, on unmount.
  define(runtime, binding, "close", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    if (count > 0 && arguments[0].isObject()) {
      auto object = arguments[0].getObject(runtime);
      if (object.isHostObject<EngineHandle>(runtime)) {
        object.getHostObject<EngineHandle>(runtime)->close();
      }
    }
    return jsi::Value::undefined();
  });

  global.setProperty(runtime, GLOBAL_NAME, std::move(binding));
}

}

void installShadowListNativeJSI(const std::shared_ptr<const ContextContainer>& contextContainer) {
  if (!contextContainer) {
    return;
  }
  auto weakRuntimeScheduler = contextContainer->find<std::weak_ptr<RuntimeScheduler>>(RuntimeSchedulerKey);
  auto runtimeScheduler = weakRuntimeScheduler ? weakRuntimeScheduler->lock() : nullptr;
  if (!runtimeScheduler) {
    return;
  }
  runtimeScheduler->scheduleWork([](jsi::Runtime& runtime) { install(runtime); });
}

}
