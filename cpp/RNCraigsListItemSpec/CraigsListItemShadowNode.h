#pragma once

#include "CraigsListItemEventEmitter.h"
#include "CraigsListItemProps.h"
#include "CraigsListItemState.h"
#include <react/renderer/components/view/conversions.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutConstraints.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char CraigsListItemComponentName[];

/*
 * `ShadowNode` for <CraigsListItem> component.
 */
class CraigsListItemShadowNode final : public ConcreteViewShadowNode<
  CraigsListItemComponentName,
  CraigsListItemProps,
  CraigsListItemEventEmitter,
  CraigsListItemState> {

  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;
};

}
