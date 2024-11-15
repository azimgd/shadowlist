#pragma once

#include "SLContainerEventEmitter.h"
#include "SLContainerProps.h"
#include "SLContainerState.h"
#include "SLContainerShadowNode.h"
#include <jsi/jsi.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <react/renderer/core/LayoutMetrics.h>

#include "SLFenwickTree.hpp"

namespace facebook::react {

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

  void layout(LayoutContext layoutContext) override;
  void appendChild(const ShadowNode::Shared& child) override;
  void replaceChild(
    const ShadowNode& oldChild,
    const ShadowNode::Shared& newChild,
    size_t suggestedIndex = SIZE_MAX) override;
  SLFenwickTree calculateChildrenMeasurements(ConcreteStateData stateData);
  Point calculateScrollPosition(ConcreteStateData stateData);
  Size calculateScrollContent(ConcreteStateData stateData);
  Size calculateScrollContainer(ConcreteStateData stateData);
  YogaLayoutableShadowNode& shadowNodeFromContext(YGNodeConstRef yogaNode);
};

}
