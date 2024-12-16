#import "SLModule.h"
#import "SLCommitHook.h"
#import "SLModuleSpecJSI.h"
#import "SLModuleJSI.h"
#import "SLRuntimeManager.h"
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
  self->_surfacePresenter = surfacePresenter;
}

- (void)installJSIBindingsWithRuntime:(facebook::jsi::Runtime &)runtime
{
  self->runtime_ = &runtime;
  SLRuntimeManager::getInstance().setRuntime(self->runtime_);
  SLModuleJSI::install(runtime, commitHook_);
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
