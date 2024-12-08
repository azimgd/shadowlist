#import "SLModule.h"
#import "SLCommitHook.h"
#import "SLModuleSpecJSI.h"
#import <ReactCommon/RCTTurboModule.h>
#import <React/RCTSurfacePresenter.h>
#import <React/RCTScheduler.h>
#import <React/RCTSurfacePresenterStub.h>

using namespace facebook::react;

@implementation SLModule {
  __weak RCTSurfacePresenter* _surfacePresenter;
  std::shared_ptr<SLCommitHook> commitHook_;
  facebook::jsi::Runtime *runtime_;
}

RCT_EXPORT_MODULE()

- (void)setSurfacePresenter:(id<RCTSurfacePresenterStub>)surfacePresenter
{
  _surfacePresenter = surfacePresenter;
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
{
  self->runtime_ = &runtime;

  auto registerContainerFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_registerContainerNode"),
    1,
    [=](facebook::jsi::Runtime &runtime,
    const facebook::jsi::Value &thisValue,
    const facebook::jsi::Value *arguments,
    size_t count) -> facebook::jsi::Value
  {
    auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
    self->commitHook_->registerContainerNode(shadowNode);

    return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_registerContainerNode", std::move(registerContainerFamily));

  auto unregisterContainerFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_unregisterContainerNode"),
    1,
    [=](facebook::jsi::Runtime &runtime,
      const facebook::jsi::Value &thisValue,
      const facebook::jsi::Value *arguments,
      size_t count) -> facebook::jsi::Value
    {
      auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
      self->commitHook_->unregisterContainerNode(shadowNode);

      return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_unregisterContainerNode", std::move(unregisterContainerFamily));

  auto registerElementFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_registerElementNode"),
    1,
    [=](facebook::jsi::Runtime &runtime,
    const facebook::jsi::Value &thisValue,
    const facebook::jsi::Value *arguments,
    size_t count) -> facebook::jsi::Value
  {
    auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
    self->commitHook_->registerElementNode(shadowNode);

    return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_registerElementNode", std::move(registerElementFamily));
  auto unregisterElementFamily = facebook::jsi::Function::createFromHostFunction(
    runtime,
    facebook::jsi::PropNameID::forAscii(runtime, "__NATIVE_unregisterElementNode"),
    1,
    [=](facebook::jsi::Runtime &runtime,
      const facebook::jsi::Value &thisValue,
      const facebook::jsi::Value *arguments,
      size_t count) -> facebook::jsi::Value
    {
      auto shadowNode = shadowNodeFromValue(runtime, arguments[0]);
      self->commitHook_->unregisterElementNode(shadowNode);

      return facebook::jsi::Value::undefined();
  });
  runtime.global().setProperty(runtime, "__NATIVE_unregisterElementNode", std::move(unregisterContainerFamily));
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<SLModuleSpecJSI>(params);
}

- (void)setup
{
  self->commitHook_ = std::make_shared<SLCommitHook>(self->_surfacePresenter.scheduler.uiManager, self->runtime_);
}

@end
