#pragma once

#ifdef ANDROID
#include <ReactCommon/JavaTurboModule.h>
#include <ReactCommon/TurboModule.h>
#include <jsi/jsi.h>
#include "SLElementComponentDescriptor.h"

namespace facebook::react {

JSI_EXPORT
std::shared_ptr<TurboModule> SLElementSpec_ModuleProvider(const std::string &moduleName, const JavaTurboModule::InitParams &params);

}
#endif
