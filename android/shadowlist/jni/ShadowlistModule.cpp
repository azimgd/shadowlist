#include <react/fabric/Binding.h>
#include <react/renderer/scheduler/Scheduler.h>

#include "ShadowlistModule.h"
#include "SLRuntimeManager.h"
#include "SLModuleJSI.h"

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

ShadowlistModule::ShadowlistModule(jni::alias_ref<ShadowlistModule::javaobject> jThis): javaPart_(jni::make_global(jThis)) {}

ShadowlistModule::~ShadowlistModule() {}

void ShadowlistModule::registerNatives() {
  registerHybrid({
    makeNativeMethod("initHybrid", ShadowlistModule::initHybrid),
    makeNativeMethod("createCommitHook", ShadowlistModule::createCommitHook),
    makeNativeMethod("injectJSIBindings", ShadowlistModule::injectJSIBindings)
  });
}

void ShadowlistModule::createCommitHook(jni::alias_ref<facebook::react::JFabricUIManager::javaobject> fabricUIManager) {
  const auto &uiManager = fabricUIManager->getBinding()->getScheduler()->getUIManager();
  commitHook_ = std::make_shared<SLCommitHook>(uiManager);
}

jni::local_ref<ShadowlistModule::jhybriddata> ShadowlistModule::initHybrid(jni::alias_ref<jhybridobject> jThis) {
  return makeCxxInstance(jThis);
}

jni::local_ref<BindingsInstallerHolder::javaobject> ShadowlistModule::injectJSIBindings() {
  return jni::make_local(BindingsInstallerHolder::newObjectCxxArgs([&](jsi::Runtime& runtime) {
    SLRuntimeManager::getInstance().setRuntime(&runtime);
    SLModuleJSI::install(runtime);
    SLModuleJSI::install(runtime, commitHook_);
  }));
}

}