#include "SLModuleJSI.h"

namespace facebook::react {

using namespace facebook;

void SLModuleJSI::install(facebook::jsi::Runtime &runtime, std::shared_ptr<SLCommitHook> &commitHook) {
  auto registerContainerFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_registerContainerNode"),
    1,
    [&](facebook::jsi::Runtime &runtime,
    const facebook::jsi::Value &thisValue,
    const facebook::jsi::Value *arguments,
    size_t count) -> facebook::jsi::Value
  {
    auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
    commitHook->registerContainerNode(shadowNode);

    return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_registerContainerNode", std::move(registerContainerFamily));

  auto unregisterContainerFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_unregisterContainerNode"),
    1,
    [&](facebook::jsi::Runtime &runtime,
      const facebook::jsi::Value &thisValue,
      const facebook::jsi::Value *arguments,
      size_t count) -> facebook::jsi::Value
    {
      auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
      commitHook->unregisterContainerNode(shadowNode);

      return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_unregisterContainerNode", std::move(unregisterContainerFamily));

  auto registerElementFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_registerElementNode"),
    1,
    [&](facebook::jsi::Runtime &runtime,
    const facebook::jsi::Value &thisValue,
    const facebook::jsi::Value *arguments,
    size_t count) -> facebook::jsi::Value
  {
    auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
    commitHook->registerElementNode(shadowNode);

    return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_registerElementNode", std::move(registerElementFamily));

  auto unregisterElementFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_unregisterElementNode"),
    1,
    [&](facebook::jsi::Runtime &runtime,
      const facebook::jsi::Value &thisValue,
      const facebook::jsi::Value *arguments,
      size_t count) -> facebook::jsi::Value
    {
      auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
      commitHook->unregisterElementNode(shadowNode);

      return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_unregisterElementNode", std::move(unregisterElementFamily));
}

}
