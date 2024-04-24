#pragma once

#include "CraigsListContainerEventEmitter.h"
#include "CraigsListContainerProps.h"
#include "CraigsListContainerState.h"
#include <react/renderer/components/view/conversions.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char CraigsListContainerComponentName[];

/*
 * `ShadowNode` for <CraigsListContainer> component.
 */
class CraigsListContainerShadowNode final : public ConcreteViewShadowNode<
  CraigsListContainerComponentName,
  CraigsListContainerProps,
  CraigsListContainerEventEmitter,
  CraigsListContainerState> {

  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

#pragma mark - LayoutableShadowNode
  void layout(LayoutContext layoutContext) override;

  /**
   * Measurements
   */
  Rect calculateContainerMeasurements(LayoutContext layoutContext);

  /**
   * Caster
   */
  static YogaLayoutableShadowNode& shadowNodeFromContext(YGNodeConstRef yogaNode);
};

}
