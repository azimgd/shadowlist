#pragma once

#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

#include "ShadowListTemplateViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

JSI_EXPORT extern const char ShadowListTemplateViewComponentName[];

/*
 * `ShadowNode` for <ShadowListTemplateView> component.
 */
using ShadowListTemplateViewShadowNode = ConcreteViewShadowNode<
  ShadowListTemplateViewComponentName,
  ShadowListTemplateViewProps,
  ShadowListTemplateViewEventEmitter,
  ShadowListTemplateViewState>;

}
