#pragma once

#include "CraigsListContainerEventEmitter.h"
#include "CraigsListContainerProps.h"
#include "CraigsListContainerState.h"
#include "CraigsListFenwickTree.hpp"
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

  void layout(LayoutContext layoutContext) override;

  void calculateContainerMeasurements(LayoutContext layoutContext);

  private:

  /**
   * Measurements
   */
  Rect scrollContainer_;
  Rect scrollContent_;
  CraigsListFenwickTree scrollContentTree_;

  /**
   * Caster
   */
  static YogaLayoutableShadowNode& shadowNodeFromContext(YGNodeConstRef yogaNode);
};

}
