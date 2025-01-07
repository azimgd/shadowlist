#ifndef SLRuntimeManager_H
#define SLRuntimeManager_H

#include <react/renderer/uimanager/UIManager.h>

namespace facebook::react {

using namespace facebook::react;
using namespace facebook::jsi;

class SLRuntimeManager {
private:
  jsi::Runtime *_runtime;
  std::unordered_map<int, int> _tagRegistry;

  SLRuntimeManager() : _runtime(nullptr) {}

public:
  static SLRuntimeManager& getInstance() {
    static SLRuntimeManager instance;
    return instance;
  }

  jsi::Runtime* getRuntime() const {
    return _runtime;
  }

  void setRuntime(jsi::Runtime *runtime) {
    _runtime = runtime;
  }

  void addTag(int key, int value) {
    _tagRegistry[key] = value;
  }

  int getTag(int key) const {
    auto it = _tagRegistry.find(key);
    return it != _tagRegistry.end() ? it->second : -1;
  }

  void removeTag(int key) {
    _tagRegistry.erase(key);
  }

  SLRuntimeManager(const SLRuntimeManager&) = delete;
  SLRuntimeManager& operator=(const SLRuntimeManager&) = delete;
};

}
#endif
