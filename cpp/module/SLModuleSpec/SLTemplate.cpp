#include "SLTemplate.h"
#include <react/renderer/components/text/RawTextShadowNode.h>
#include <react/renderer/components/image/ImageShadowNode.h>

namespace facebook::react {

int nextFamilyTag = -2;

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -2e9;
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

static void updateRawTextProps(SLContainerProps::SLContainerDataItem *elementData, const std::shared_ptr<ShadowNode> &nextShadowNode, const ShadowNode::Shared &shadowNode) {
  if (shadowNode->getComponentName() != std::string("RawText")) {
    return;
  }

  RawTextShadowNode::ConcreteProps* updatedProps = const_cast<RawTextShadowNode::ConcreteProps*>(
    static_cast<const RawTextShadowNode::ConcreteProps*>(nextShadowNode->getProps().get()));
  updatedProps->text = SLContainerProps::getDataItemContent(elementData, updatedProps->text);
}

static void updateImageProps(SLContainerProps::SLContainerDataItem *elementData, const std::shared_ptr<ShadowNode> &nextShadowNode, const ShadowNode::Shared &shadowNode) {
  if (shadowNode->getComponentName() != std::string("Image")) {
    return;
  }

  ImageShadowNode::ConcreteProps* updatedProps = const_cast<ImageShadowNode::ConcreteProps*>(
    static_cast<const ImageShadowNode::ConcreteProps*>(nextShadowNode->getProps().get()));
  updatedProps->sources[0].uri = SLContainerProps::getDataItemContent(elementData, updatedProps->sources[0].uri);
}

ShadowNode::Shared SLTemplate::cloneShadowNodeTree(jsi::Runtime *runtime, SLContainerProps::SLContainerDataItem* elementData, const ShadowNode::Shared& shadowNode)
{
  auto const &componentDescriptor = shadowNode->getComponentDescriptor();
  PropsParserContext propsParserContext{shadowNode->getSurfaceId(), *componentDescriptor.getContextContainer().get()};

  nextFamilyTag = adjustFamilyTag(nextFamilyTag);

  InstanceHandle::Shared instanceHandle = std::make_shared<const InstanceHandle>(
    *runtime,
    shadowNode->getInstanceHandle(*runtime),
    shadowNode->getTag());
  auto const fragment = ShadowNodeFamilyFragment{nextFamilyTag, shadowNode->getSurfaceId(), instanceHandle};
  auto const family = componentDescriptor.createFamily(fragment);
  auto const props = componentDescriptor.cloneProps(propsParserContext, shadowNode->getProps(), {});
  auto const state = componentDescriptor.createInitialState(props, family);
  auto const nextShadowNode = componentDescriptor.createShadowNode(
    ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state}, family);

  updateRawTextProps(elementData, nextShadowNode, shadowNode);
  updateImageProps(elementData, nextShadowNode, shadowNode);

  for (const auto &childShadowNode : shadowNode->getChildren()) {
    auto const clonedChildShadowNode = cloneShadowNodeTree(runtime, elementData, childShadowNode);
    componentDescriptor.appendChild(nextShadowNode, clonedChildShadowNode);
  }

  return nextShadowNode;
}

}
