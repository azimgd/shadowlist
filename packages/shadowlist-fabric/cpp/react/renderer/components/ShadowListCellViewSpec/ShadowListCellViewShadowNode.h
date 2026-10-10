#pragma once

#include <jsi/jsi.h>
#include <react/renderer/components/ShadowListViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowListViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>

#include "ShadowListCellViewState.h"

namespace facebook::react {

JSI_EXPORT extern const char ShadowListCellViewComponentName[];

using ShadowListCellViewShadowNode = ConcreteViewShadowNode<
  ShadowListCellViewComponentName,
  ShadowListCellViewProps,
  ShadowListCellViewEventEmitter,
  ShadowListCellViewState>;

}
