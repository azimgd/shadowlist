#ifndef SLRuntimeManager_H
#define SLRuntimeManager_H

#include <react/renderer/uimanager/UIManager.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

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

  void shiftIndices(int offset) {
    for (auto& entry : _tagRegistry) {
      entry.second += offset;
    }
  }
  
  void addIndexToTag(int tag, int elementDataIndex) {
    _tagRegistry[tag] = elementDataIndex;
  }

  int getIndexFromTag(int tag) const {
    auto it = _tagRegistry.find(tag);
    return it != _tagRegistry.end() ? it->second : -1;
  }

  SLRuntimeManager(const SLRuntimeManager&) = delete;
  SLRuntimeManager& operator=(const SLRuntimeManager&) = delete;
};

}
#endif
