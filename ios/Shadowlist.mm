#import "Shadowlist.h"
#import "SLModuleJSI.h"
#import "SLRuntimeManager.h"
#import <ReactCommon/RCTTurboModule.h>
#import <React/RCTSurfacePresenter.h>
#import <React/RCTScheduler.h>
#import <React/RCTSurfacePresenterStub.h>

using namespace facebook::react;

@implementation Shadowlist {
  __weak RCTSurfacePresenter* _surfacePresenter;
  std::shared_ptr<SLCommitHook> commitHook_;
  facebook::jsi::Runtime *runtime_;
}

RCT_EXPORT_MODULE()

- (void)setSurfacePresenter:(id<RCTSurfacePresenterStub>)surfacePresenter
{
  self->_surfacePresenter = surfacePresenter;
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
{
  self->runtime_ = &runtime;
  SLRuntimeManager::getInstance().setRuntime(self->runtime_);
  SLModuleJSI::install(runtime);
  SLModuleJSI::install(runtime, commitHook_);
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::NativeShadowlistSpecJSI>(params);
}

- (void)setup
{
  self->commitHook_ = std::make_shared<SLCommitHook>(self->_surfacePresenter.scheduler.uiManager);
}
@end
