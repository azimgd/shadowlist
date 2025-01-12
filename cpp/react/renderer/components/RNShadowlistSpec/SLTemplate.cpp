#include "SLTemplate.h"
#include "SLRuntimeManager.h"
#include <react/renderer/components/text/RawTextShadowNode.h>
#include <react/renderer/components/image/ImageShadowNode.h>

namespace facebook::react {

int nextFamilyTag = -2;

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -std::numeric_limits<int>::min();
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

static void updateRawTextProps(const SLContainerProps::SLContainerDataItem &elementData, const std::shared_ptr<ShadowNode> &nextShadowNode, const ShadowNode::Shared &shadowNode) {
  if (shadowNode->getComponentName() != std::string("RawText")) {
    return;
  }

  RawTextShadowNode::ConcreteProps* updatedProps = const_cast<RawTextShadowNode::ConcreteProps*>(
    static_cast<const RawTextShadowNode::ConcreteProps*>(nextShadowNode->getProps().get()));
  auto path = SLKeyExtractor::extractKey(updatedProps->text);
  updatedProps->text = SLContainerProps::getElementValueByPath(elementData, path);
}

static void updateImageProps(const SLContainerProps::SLContainerDataItem &elementData, const std::shared_ptr<ShadowNode> &nextShadowNode, const ShadowNode::Shared &shadowNode) {
  if (shadowNode->getComponentName() != std::string("Image")) {
    return;
  }

  ImageShadowNode::ConcreteProps* updatedProps = const_cast<ImageShadowNode::ConcreteProps*>(
    static_cast<const ImageShadowNode::ConcreteProps*>(nextShadowNode->getProps().get()));
  auto path = SLKeyExtractor::extractKey(updatedProps->sources[0].uri);
  updatedProps->sources[0].uri = SLContainerProps::getElementValueByPath(elementData, path);
}

ShadowNode::Unshared SLTemplate::cloneShadowNodeTree(const int& elementDataIndex, const SLContainerProps::SLContainerDataItem &elementData, const ShadowNode::Shared& shadowNode)
{
  auto const &componentDescriptor = shadowNode->getComponentDescriptor();
  PropsParserContext propsParserContext{shadowNode->getSurfaceId(), *componentDescriptor.getContextContainer().get()};

  nextFamilyTag = adjustFamilyTag(nextFamilyTag);
  SLRuntimeManager::getInstance().addTag(nextFamilyTag, elementDataIndex);

  InstanceHandle::Shared instanceHandle = std::make_shared<const InstanceHandle>(
    *SLRuntimeManager::getInstance().getRuntime(),
    shadowNode->getInstanceHandle(*SLRuntimeManager::getInstance().getRuntime()),
    nextFamilyTag);

  auto const family = componentDescriptor.createFamily({nextFamilyTag, shadowNode->getSurfaceId(), instanceHandle});

  #ifdef ANDROID
  auto const nextProps = componentDescriptor.cloneProps(propsParserContext, shadowNode->getProps(), RawProps(shadowNode->getProps()->rawProps));
  #else
  auto const nextProps = componentDescriptor.cloneProps(propsParserContext, shadowNode->getProps(), {});
  #endif

  auto const nextState = componentDescriptor.createInitialState(nextProps, family);
  auto const nextShadowNode = componentDescriptor.createShadowNode({nextProps, ShadowNodeFragment::childrenPlaceholder(), nextState}, family);

  updateRawTextProps(elementData, nextShadowNode, shadowNode);
  updateImageProps(elementData, nextShadowNode, shadowNode);

  nextShadowNode->setMounted(true);

  for (const auto &childShadowNode : shadowNode->getChildren()) {
    auto const clonedChildShadowNode = cloneShadowNodeTree(elementDataIndex, elementData, childShadowNode);
    componentDescriptor.appendChild(nextShadowNode, clonedChildShadowNode);
  }

  return nextShadowNode;
}

}
