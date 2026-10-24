#include "ShadowListStoreModule.h"

#include "ShadowListStoreJsi.h"

#include <ReactCommon/CxxTurboModuleUtils.h>
#include <ReactCommon/TurboModule.h>

#include <atomic>
#include <memory>
#include <utility>

namespace facebook::react {

namespace {

/*
 * The store functions as a TurboModule. They are synchronous and take only their arguments, so
 * the module hands out the plain JSI functions and keeps no state of its own.
 */
class ShadowListStoreModule final : public TurboModule {
public:
  explicit ShadowListStoreModule(std::shared_ptr<CallInvoker> jsInvoker) :
    TurboModule(shadowlist::STORE_MODULE_NAME, std::move(jsInvoker)) {}

protected:
  jsi::Value create(jsi::Runtime& runtime, const jsi::PropNameID& propName) override {
    return shadowlist::storeFunction(runtime, propName.utf8(runtime));
  }
};

}

void registerShadowListStoreModule() {
  static std::atomic<bool> registered{false};
  if (registered.exchange(true)) {
    return;
  }
  registerCxxModuleToGlobalModuleMap(shadowlist::STORE_MODULE_NAME, [](std::shared_ptr<CallInvoker> jsInvoker) {
    return std::make_shared<ShadowListStoreModule>(std::move(jsInvoker));
  });
}

}
