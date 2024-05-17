#pragma once

#include "ShadowListContainerEventEmitter.h"
#include "ShadowListContainerProps.h"
#include "ShadowListContainerState.h"
#include "ShadowListFenwickTree.hpp"
#include <react/renderer/components/view/conversions.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListContainerComponentName[];

/*
 * `ShadowNode` for <ShadowListContainer> component.
 */
class ShadowListContainerShadowNode final : public ConcreteViewShadowNode<
  ShadowListContainerComponentName,
  ShadowListContainerProps,
  ShadowListContainerEventEmitter,
  ShadowListContainerState> {

  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

  void layout(LayoutContext layoutContext) override;

  void calculateContainerMeasurements(LayoutContext layoutContext, bool horizontal, bool inverted);

  private:

  /*
   * Measurements
   */
  Rect scrollContainer_;
  Rect scrollContent_;
  ShadowListFenwickTree scrollContentTree_;

  /*
   * Caster
   */
  static YogaLayoutableShadowNode& shadowNodeFromContext(YGNodeConstRef yogaNode);
};

}
