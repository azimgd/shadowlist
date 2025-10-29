#pragma once

#include <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowlistViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

#include "ShadowlistElementViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

JSI_EXPORT extern const char ShadowlistElementViewComponentName[];

/*
 * `ShadowNode` for <ShadowlistElementView> component.
 */
using ShadowlistElementViewShadowNode = ConcreteViewShadowNode<
  ShadowlistElementViewComponentName,
  ShadowlistElementViewProps,
  ShadowlistElementViewEventEmitter,
  ShadowlistElementViewState>;

}
