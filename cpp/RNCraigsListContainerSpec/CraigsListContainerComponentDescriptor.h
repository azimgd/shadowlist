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
    auto& stateData = static_cast<const CraigsListContainerShadowNode::ConcreteState&>(*shadowNode.getState()).getData();
    auto layoutMetrics = stateData.calculateLayoutMetrics();
    auto layoutableShadowNodeChildren = layoutableShadowNode.getLayoutableChildNodes();

    for (std::size_t index = 0; index < layoutableShadowNodeChildren.size(); ++index) {
      auto& childNode = *layoutableShadowNodeChildren[index];
      auto& layoutableChildShadowNode = static_cast<CraigsListItemShadowNode&>(childNode);

      /**
       * Cloning each node to conform sealed node pattern
       */
      auto nextChildNode = layoutableChildShadowNode.clone({
        ShadowNodeFragment::propsPlaceholder(),
        ShadowNodeFragment::childrenPlaceholder(),
        ShadowNodeFragment::statePlaceholder()
      });
      layoutableShadowNode.replaceChild(childNode, nextChildNode);
      auto& nextLayoutableChildShadowNode = static_cast<CraigsListItemShadowNode&>(*nextChildNode);

      /**
       * Determine child visibility, applies given offset to both directions
       */
      auto offset = 10;
      size_t visibleStartIndex = std::max(0, layoutMetrics.visibleStartIndex - offset);
      size_t visibleEndIndex = std::min(layoutableShadowNodeChildren.size(), size_t(layoutMetrics.visibleEndIndex + offset));
  
      /**
       * Apply new layout based on child visibility
       */
      nextLayoutableChildShadowNode.adjustLayout(
        index >= visibleStartIndex && index <= visibleEndIndex
      );
    }
    
    layoutableShadowNode.dirtyLayout();
    ConcreteComponentDescriptor::adopt(shadowNode);
  }
};

void RNCraigsListContainerSpec_registerComponentDescriptorsFromCodegen(
  std::shared_ptr<const ComponentDescriptorProviderRegistry> registry);

}
