#pragma once

#include "CraigsListContainerEventEmitter.h"
#include "CraigsListContainerProps.h"
#include "CraigsListContainerState.h"
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <jsi/jsi.h>

namespace facebook::react {

JSI_EXPORT extern const char CraigsListContainerComponentName[];

/*
 * `ShadowNode` for <CraigsListContainer> component.
 */
using CraigsListContainerShadowNode = ConcreteViewShadowNode<
  CraigsListContainerComponentName,
  CraigsListContainerProps,
  CraigsListContainerEventEmitter,
  CraigsListContainerState>;
}
