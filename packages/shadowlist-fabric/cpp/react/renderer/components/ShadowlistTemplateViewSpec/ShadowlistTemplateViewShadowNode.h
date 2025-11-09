#pragma once

#include <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowlistViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

#include "ShadowlistTemplateViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

JSI_EXPORT extern const char ShadowlistTemplateViewComponentName[];

/*
 * `ShadowNode` for <ShadowlistTemplateView> component.
 */
using ShadowlistTemplateViewShadowNode = ConcreteViewShadowNode<
  ShadowlistTemplateViewComponentName,
  ShadowlistTemplateViewProps,
  ShadowlistTemplateViewEventEmitter,
  ShadowlistTemplateViewState>;

}
