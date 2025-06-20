#pragma once

#include "SLContainerEventEmitter.h"
#include "SLContainerProps.h"
#include "SLContainerState.h"
#include "SLContainerShadowNode.h"
#include "SLElementShadowNode.h"
#include "SLElementProps.h"
#include "SLRegistryManager.h"
#include "SLMeasurementsManager.h"
#include <jsi/jsi.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <react/renderer/core/LayoutMetrics.h>

#include <string>
#include <ranges>

#ifndef RCT_DEBUG
#include <iostream>
#include <iomanip>
#ifdef ANDROID
#include <android/log.h>
#endif
#endif

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

JSI_EXPORT extern const char SLContainerComponentName[];

/*
 * `ShadowNode` for <SLContainer> component.
 */
class SLContainerShadowNode final : public ConcreteViewShadowNode<
  SLContainerComponentName,
  SLContainerProps,
  SLContainerEventEmitter,
  SLContainerState,
  true> {
  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

#pragma mark - LayoutableShadowNode
  void setRegistryManager(std::shared_ptr<SLRegistryManager> registryManager);
  void resetRegistryManager();
  void setMeasurementsManager(std::shared_ptr<SLMeasurementsManager> measurementsManager);
  void resetMeasurementsManager();

  void layout(LayoutContext layoutContext) override;
  void appendChild(const ShadowNode::Shared& child) override;

  LayoutMetrics layoutElement(LayoutContext layoutContext, ShadowNode::Unshared shadowNode, int numColumns);
  LayoutMetrics adjustElement(Point origin, ShadowNode::Unshared shadowNode);
  LayoutMetrics resizeElement(Size size, ShadowNode::Unshared shadowNode);

  float getRelativeSizeFromSize(Size size);
  float getRelativePointFromPoint(Point point);

  private:
  std::shared_ptr<SLRegistryManager> registryManager;
  std::shared_ptr<SLMeasurementsManager> measurementsManager;
};

}
