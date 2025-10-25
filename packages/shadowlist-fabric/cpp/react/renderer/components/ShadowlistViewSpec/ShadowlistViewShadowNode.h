#pragma once

#include <react/renderer/components/ShadowlistViewSpec/EventEmitters.h>
#include <react/renderer/components/ShadowlistViewSpec/Props.h>
#include <react/renderer/components/view/ConcreteViewShadowNode.h>
#include <react/renderer/core/LayoutContext.h>
#include <jsi/jsi.h>

#include "ShadowlistViewState.h"

#include <shadowlist-core/Container.hpp>
#include <shadowlist-core/Virtualizer.hpp>

namespace facebook::react {

JSI_EXPORT extern const char ShadowlistViewComponentName[];

/*
 * `ShadowNode` for <ShadowlistView> component.
 */
class ShadowlistViewShadowNode final : public ConcreteViewShadowNode<
  ShadowlistViewComponentName,
  ShadowlistViewProps,
  ShadowlistViewEventEmitter,
  ShadowlistViewState> {
  
  public:
  using ConcreteViewShadowNode::ConcreteViewShadowNode;

#pragma mark - LayoutableShadowNode

  void layout(LayoutContext layoutContext) override;

  void setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager);
  void setVirtualizerManager(std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager);

  private:
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;
  std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager_;
};

}
