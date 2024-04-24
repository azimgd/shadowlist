#pragma once
#include <iostream>

#include "CraigsListContainerShadowNode.h"
#include "CraigsListItemShadowNode.h"
#include <react/renderer/core/ConcreteComponentDescriptor.h>
#include <react/renderer/componentregistry/ComponentDescriptorProviderRegistry.h>
#include <react/renderer/graphics/RectangleEdges.h>

namespace facebook::react {

class CraigsListContainerComponentDescriptor : public ConcreteComponentDescriptor<CraigsListContainerShadowNode> {
  using ConcreteComponentDescriptor::ConcreteComponentDescriptor;

  void adopt(ShadowNode& shadowNode) const override {
    auto& layoutableShadowNode = static_cast<CraigsListContainerShadowNode&>(shadowNode);
    auto layoutMetrics = layoutableShadowNode.calculateLayoutMetrics();

    for (std::size_t index = layoutMetrics.visibleStartIndex; index < layoutMetrics.visibleEndIndex; ++index) {
      auto& childNode = *layoutableShadowNode.getLayoutableChildNodes()[index];
      auto& layoutableChildShadowNode = static_cast<CraigsListItemShadowNode&>(childNode);

      auto nextChildNode = layoutableChildShadowNode.clone({
        ShadowNodeFragment::propsPlaceholder(),
        ShadowNodeFragment::childrenPlaceholder(),
        ShadowNodeFragment::statePlaceholder()
      });
      layoutableShadowNode.replaceChild(childNode, nextChildNode);
      auto& nextLayoutableChildShadowNode = static_cast<CraigsListItemShadowNode&>(*nextChildNode);

      nextLayoutableChildShadowNode.adjustLayout(
        index >= layoutMetrics.visibleStartIndex && index <= layoutMetrics.visibleEndIndex
      );
    }
    
    layoutableShadowNode.dirtyLayout();

    ConcreteComponentDescriptor::adopt(shadowNode);
  }
};

void RNCraigsListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
