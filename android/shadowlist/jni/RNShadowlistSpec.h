#pragma once

#include <ReactCommon/JavaTurboModule.h>
#include <ReactCommon/TurboModule.h>
#include <jsi/jsi.h>
#include "SLElementComponentDescriptor.h"
#include "SLContainerComponentDescriptor.h"

namespace facebook::react {

/**
 * JNI C++ class for module 'NativeShadowlist'
 */
class JSI_EXPORT NativeShadowlistSpecJSI : public JavaTurboModule {
public:
  NativeShadowlistSpecJSI(const JavaTurboModule::InitParams &params);
};

JSI_EXPORT
std::shared_ptr<TurboModule> RNShadowlistSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params);

}
