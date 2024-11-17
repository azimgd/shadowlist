#pragma once

#include "SLElementEventEmitter.h"
#include "SLElementProps.h"
#include "SLElementState.h"
#include "SLElementShadowNode.h"
#include <jsi/jsi.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <react/renderer/core/LayoutMetrics.h>

namespace facebook::react {

JSI_EXPORT extern const char SLElementComponentName[];

/*
 * `ShadowNode` for <SLElement> component.
 */
class SLElementShadowNode final : public ConcreteViewShadowNode<
  SLElementComponentName,
  SLElementProps,
  SLElementEventEmitter,
  SLElementState,
  true> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

#pragma mark - LayoutableShadowNode
};

}
