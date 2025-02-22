#pragma once

#include <ReactCommon/BindingsInstallerHolder.h>
#include <react/fabric/JFabricUIManager.h>
#include <fbjni/fbjni.h>
#include "SLCommitHook.h"

#include <string>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

using namespace facebook;
using namespace facebook::jni;

class ShadowlistModule : public jni::HybridClass<ShadowlistModule> {
  public:
  static auto constexpr kJavaDescriptor = "Lcom/shadowlist/ShadowlistModule;";
  static jni::local_ref<jhybriddata> initHybrid(jni::alias_ref<jhybridobject> jThis);
  static void registerNatives();

  ~ShadowlistModule();

  private:
  friend HybridBase;
  jni::global_ref<ShadowlistModule::javaobject> javaPart_;
  std::shared_ptr<SLCommitHook> commitHook_;

  explicit ShadowlistModule(jni::alias_ref<ShadowlistModule::javaobject> jThis);

  void createCommitHook(jni::alias_ref<facebook::react::JFabricUIManager::javaobject> fabricUIManager);
};

}
