#pragma once

#include "SLContentEventEmitter.h"
#include "SLContentProps.h"
#include "SLContentState.h"
#include "SLContentShadowNode.h"
#include <jsi/jsi.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <react/renderer/core/LayoutMetrics.h>

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

JSI_EXPORT extern const char SLContentComponentName[];

/*
 * `ShadowNode` for <SLContent> component.
 */
class SLContentShadowNode final : public ConcreteViewShadowNode<
  SLContentComponentName,
  SLContentProps,
  SLContentEventEmitter,
  SLContentState,
  true> {
 public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

#pragma mark - LayoutableShadowNode

  void layout(LayoutContext layoutContext) override;
  void appendChild(const ShadowNode::Shared& child) override;
};

}
