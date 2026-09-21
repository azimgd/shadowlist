#include "ShadowListNativeJSI.h"

#include "ShadowListNativeEngine.h"

#include <jsi/JSIDynamic.h>
#include <jsi/jsi.h>
#include <react/renderer/runtimescheduler/RuntimeScheduler.h>

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

double numberArgument(const jsi::Value* arguments, std::size_t count, std::size_t index, double fallback) {
  return index < count && arguments[index].isNumber() ? arguments[index].getNumber() : fallback;
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
 * What `open` returns: holds the engine while JS holds the object. A host object dies with a
 * render React throws away (garbage) and with the runtime (a reload), so neither leaks an engine;
 * `close` drops it at unmount without waiting for GC.
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

  // setData(listId, items, keys, templates?, scrollToStart?) -> count
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

  // insertItems(listId, index, items, keys, templates?) -> count; index past the end appends
  define(runtime, binding, "insertItems", 5, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    double index = numberArgument(arguments, count, 1, -1);
    auto size = engine->insertItems(
      index < 0 ? static_cast<std::size_t>(-1) : static_cast<std::size_t>(index),
      dynamicArgument(runtime, arguments, count, 2),
      stringsArgument(runtime, arguments, count, 3),
      stringsArgument(runtime, arguments, count, 4));
    return jsi::Value(static_cast<double>(size));
  });

  // updateItem(listId, key, patch, template?, replace?) -> bool
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

  // removeItems(listId, keys) -> count
  define(runtime, binding, "removeItems", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(0);
    }
    auto size = engine->removeItems(stringsArgument(runtime, arguments, count, 1));
    return jsi::Value(static_cast<double>(size));
  });

  // moveItem(listId, key, toIndex) -> bool
  define(runtime, binding, "moveItem", 3, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value(false);
    }
    double toIndex = numberArgument(arguments, count, 2, 0);
    bool moved = engine->moveItem(
      stringArgument(runtime, arguments, count, 1), toIndex < 0 ? 0 : static_cast<std::size_t>(toIndex));
    return jsi::Value(moved);
  });

  // scrollToIndex(listId, index, viewPosition); -1 scrolls to the end, -2 to the start
  define(runtime, binding, "scrollToIndex", 3, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    engine->requestScroll(numberArgument(arguments, count, 1, -1), numberArgument(arguments, count, 2, 0));
    return jsi::Value::undefined();
  });

  // setTemplateStyle(listId, template, elementId, style | null)
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

  // configure(listId, { initialRows, padRows, cacheRows })
  define(runtime, binding, "configure", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    engine->configure(dynamicArgument(runtime, arguments, count, 1));
    return jsi::Value::undefined();
  });

  // getItem(listId, key) -> item | undefined
  define(runtime, binding, "getItem", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::undefined();
    }
    auto item = engine->getItem(stringArgument(runtime, arguments, count, 1));
    return item.isNull() ? jsi::Value::undefined() : jsi::valueFromDynamic(runtime, item);
  });

  // getKeys(listId) -> string[]
  define(runtime, binding, "getKeys", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    auto keys = engine ? engine->getKeys() : std::vector<std::string>{};
    auto array = jsi::Array(runtime, keys.size());
    for (std::size_t index = 0; index < keys.size(); ++index) {
      array.setValueAtIndex(runtime, index, jsi::String::createFromUtf8(runtime, keys[index]));
    }
    return jsi::Value(runtime, array);
  });

  // getCount(listId) -> number
  define(runtime, binding, "getCount", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    return jsi::Value(engine ? static_cast<double>(engine->size()) : 0.0);
  });

  // resolveTag(listId, tag) -> { key, index, repeatIndex } | null
  define(runtime, binding, "resolveTag", 2, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::find(stringArgument(runtime, arguments, count, 0));
    if (!engine) {
      return jsi::Value::null();
    }
    auto hit = engine->resolveTag(static_cast<Tag>(numberArgument(arguments, count, 1, 0)));
    if (!hit) {
      return jsi::Value::null();
    }
    jsi::Object result(runtime);
    result.setProperty(runtime, "key", jsi::String::createFromUtf8(runtime, hit->key));
    result.setProperty(runtime, "index", static_cast<double>(hit->index));
    result.setProperty(runtime, "repeatIndex", static_cast<double>(hit->repeatIndex));
    return jsi::Value(runtime, result);
  });

  // open(listId) -> handle: keeps the list's engine alive while held (see EngineHandle).
  define(runtime, binding, "open", 1, [](jsi::Runtime& runtime, const jsi::Value* arguments, std::size_t count) {
    auto engine = ShadowListNativeRegistry::open(stringArgument(runtime, arguments, count, 0));
    return jsi::Value(
      runtime, jsi::Object::createFromHostObject(runtime, std::make_shared<EngineHandle>(std::move(engine))));
  });
  // close(handle): drops the handle's hold now (unmount).
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
