#pragma once

#include <jsi/jsi.h>
#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>

#include "ShadowListElementViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListElementViewComponentName[];

/*
 * `ShadowNode` for <ShadowListElementView> component.
 */
using ShadowListElementViewShadowNode = ConcreteViewShadowNode<
  ShadowListElementViewComponentName,
  ShadowListElementViewProps,
  ShadowListElementViewEventEmitter,
  ShadowListElementViewState>;

}
