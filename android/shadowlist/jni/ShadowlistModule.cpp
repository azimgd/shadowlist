#include <react/fabric/Binding.h>
#include <react/renderer/scheduler/Scheduler.h>

#include "ShadowlistModule.h"
#include "SLModuleJSI.h"

namespace facebook::react {

using namespace facebook;
using namespace react;

ShadowlistModule::ShadowlistModule(jni::alias_ref<ShadowlistModule::javaobject> jThis): javaPart_(jni::make_global(jThis)) {}

ShadowlistModule::~ShadowlistModule() {}

void ShadowlistModule::registerNatives() {
  registerHybrid({
    makeNativeMethod("initHybrid", ShadowlistModule::initHybrid),
    makeNativeMethod("createCommitHook", ShadowlistModule::createCommitHook),
  });
}

void ShadowlistModule::createCommitHook(jni::alias_ref<facebook::react::JFabricUIManager::javaobject> fabricUIManager) {
  const auto &uiManager = fabricUIManager->getBinding()->getScheduler()->getUIManager();
  commitHook_ = std::make_shared<SLCommitHook>(uiManager);

  jni::make_local(BindingsInstallerHolder::newObjectCxxArgs([&](jsi::Runtime& runtime) {
  }));
}

jni::local_ref<ShadowlistModule::jhybriddata> ShadowlistModule::initHybrid(jni::alias_ref<jhybridobject> jThis) {
  return makeCxxInstance(jThis);
}

}
