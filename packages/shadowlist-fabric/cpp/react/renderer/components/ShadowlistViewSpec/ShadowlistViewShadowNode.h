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

  /*
   * Clone constructor. The core instances (container/virtualizer/header/footer)
   * live on the ShadowNode so they are freed when the node family is destroyed,
   * but inherited constructors default-initialize derived members, so the clone
   * must carry the shared instances forward from its source. This keeps a single
   * core per list instance shared across all its committed clones.
   */
  ShadowlistViewShadowNode(
    const ShadowNode& sourceShadowNode,
    const ShadowNodeFragment& fragment);

#pragma mark - LayoutableShadowNode

  void layout(LayoutContext layoutContext) override;
  void appendChild(const std::shared_ptr<const ShadowNode>& nextElementShadowNode) override;
  void replaceChild(
    const ShadowNode& prevElementShadowNode,
    const std::shared_ptr<const ShadowNode>& nextElementShadowNode,
    size_t suggestedIndex = SIZE_MAX) override;

  void setContainerManager(std::shared_ptr<azimgd::shadowlist::Container> containerManager);
  void setVirtualizerManager(std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager);
  void setHeaderSize(std::shared_ptr<double> headerSize);
  void setFooterSize(std::shared_ptr<double> footerSize);

  const std::shared_ptr<azimgd::shadowlist::Container>& getContainerManager() const { return containerManager_; }
  const std::shared_ptr<azimgd::shadowlist::Virtualizer>& getVirtualizerManager() const { return virtualizerManager_; }
  const std::shared_ptr<double>& getHeaderSize() const { return headerSize_; }
  const std::shared_ptr<double>& getFooterSize() const { return footerSize_; }

  private:
  std::shared_ptr<azimgd::shadowlist::Container> containerManager_;
  std::shared_ptr<azimgd::shadowlist::Virtualizer> virtualizerManager_;

  /*
   * Header/footer sizes are measured during layout and fed back into the next
   * frame's Virtualizer::update so the core can position elements after the header
   */
  std::shared_ptr<double> headerSize_;
  std::shared_ptr<double> footerSize_;
};

}
