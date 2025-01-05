#ifndef SLRuntimeManager_H
#define SLRuntimeManager_H

#include <react/renderer/uimanager/UIManager.h>

namespace facebook::react {

using namespace facebook::react;
using namespace facebook::jsi;

class SLRuntimeManager {
private:
  jsi::Runtime *_runtime;

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

  SLRuntimeManager(const SLRuntimeManager&) = delete;
  SLRuntimeManager& operator=(const SLRuntimeManager&) = delete;
};

}
#endif
