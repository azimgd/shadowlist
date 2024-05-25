#pragma once

#include "ShadowListContentEventEmitter.h"
#include "ShadowListContentProps.h"
#include "ShadowListContentState.h"
#include <react/renderer/components/view/conversions.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListContentComponentName[];

/*
 * `ShadowNode` for <ShadowListContent> component.
 */
class ShadowListContentShadowNode final : public ConcreteViewShadowNode<
  ShadowListContentComponentName,
  ShadowListContentProps,
  ShadowListContentEventEmitter,
  ShadowListContentState> {

  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
};

}
