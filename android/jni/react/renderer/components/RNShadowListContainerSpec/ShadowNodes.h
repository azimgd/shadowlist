#pragma once

#include <react/renderer/components/RNShadowListContainerSpec/EventEmitters.h>
#include <react/renderer/components/RNShadowListContainerSpec/Props.h>
#include <react/renderer/components/RNShadowListContainerSpec/States.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListContainerComponentName[];

/*
 * `ShadowNode` for <ShadowListContainer> component.
 */
using ShadowListContainerShadowNode = ConcreteViewShadowNode<
    ShadowListContainerComponentName,
    ShadowListContainerProps,
    ShadowListContainerEventEmitter,
    ShadowListContainerState>;

JSI_EXPORT extern const char ShadowListItemComponentName[];

/*
 * `ShadowNode` for <ShadowListItem> component.
 */
using ShadowListItemShadowNode = ConcreteViewShadowNode<
    ShadowListItemComponentName,
    ShadowListItemProps,
    ShadowListItemEventEmitter,
    ShadowListItemState>;

}
