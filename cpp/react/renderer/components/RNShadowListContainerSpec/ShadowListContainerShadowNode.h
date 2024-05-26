#pragma once

#include "ShadowListContainerEventEmitter.h"
#include "ShadowListContainerProps.h"
#include "ShadowListContainerState.h"
#include <react/renderer/graphics/Point.h>
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
};

}
