#import "SLModule.h"
#import "SLCommitHook.h"
#import "SLModuleSpecJSI.h"
#import <ReactCommon/RCTTurboModule.h>
#import <React/RCTSurfacePresenter.h>
#import <React/RCTScheduler.h>
#import <React/RCTSurfacePresenterStub.h>

using namespace facebook::react;

@implementation Shadowlist {
  __weak RCTSurfacePresenter* _surfacePresenter;
  std::shared_ptr<SLCommitHook> commitHook_;
}

RCT_EXPORT_MODULE()

int nextFamilyTag = -2;

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -2e9;
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

- (ShadowNode::Shared)cloneShadowNodeTree:(const ShadowNode::Shared&)shadowNode
{
  auto const &componentDescriptor = shadowNode->getComponentDescriptor();
  PropsParserContext propsParserContext{shadowNode->getSurfaceId(), *componentDescriptor.getContextContainer().get()};
  
  nextFamilyTag = adjustFamilyTag(nextFamilyTag);

  auto const fragment = ShadowNodeFamilyFragment{nextFamilyTag, shadowNode->getSurfaceId(), nullptr};
  auto const family = componentDescriptor.createFamily(fragment);
  auto const props = componentDescriptor.cloneProps(propsParserContext, shadowNode->getProps(), {});
  auto const state = componentDescriptor.createInitialState(props, family);
  auto const nextShadowNode = componentDescriptor.createShadowNode(
    ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state}, family);
    
  for (const auto &childShadowNode : shadowNode->getChildren()) {
    auto const clonedChildShadowNode = [self cloneShadowNodeTree:childShadowNode];
    componentDescriptor.appendChild(nextShadowNode, clonedChildShadowNode);
  }
    
  return nextShadowNode;
}

- (void)setSurfacePresenter:(id<RCTSurfacePresenterStub>)surfacePresenter
{
  _surfacePresenter = surfacePresenter;
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
{
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
      commitHook_->unregisterContainerNode(shadowNode);

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
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<SLModuleSpecJSI>(params);
}

- (void)setup
{
  commitHook_ = std::make_shared<SLCommitHook>(_surfacePresenter.scheduler.uiManager);
}

@end
