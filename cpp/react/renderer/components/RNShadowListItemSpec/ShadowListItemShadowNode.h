#pragma once

#include "ShadowListItemEventEmitter.h"
#include "ShadowListItemProps.h"
#include "ShadowListItemState.h"
#include <react/renderer/components/view/conversions.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListItemComponentName[];

/*
 * `ShadowNode` for <ShadowListItem> component.
 */
class ShadowListItemShadowNode final : public ConcreteViewShadowNode<
  ShadowListItemComponentName,
  ShadowListItemProps,
  ShadowListItemEventEmitter,
  ShadowListItemState> {

  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
};

}
