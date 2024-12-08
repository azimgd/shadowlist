#include "SLTemplate.h"

namespace facebook::react {

int nextFamilyTag = -2;

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -2e9;
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

ShadowNode::Shared SLTemplate::cloneShadowNodeTree(jsi::Runtime *runtime, nlohmann::json* elementData, const ShadowNode::Shared& shadowNode)
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

  RawTextShadowNode::ConcreteProps* interpolatedProps;
  if (shadowNode->getComponentName() == std::string("RawText")) {
    const Props::Shared& nextprops = nextShadowNode->getProps();
    interpolatedProps = const_cast<RawTextShadowNode::ConcreteProps*>(
      static_cast<const RawTextShadowNode::ConcreteProps*>(nextprops.get())
    );

    try {
      if (!SLKeyExtractor::extractKey(interpolatedProps->text).has_value()) {
        throw false;
      }

      auto elementDataPointer = nlohmann::json::json_pointer(
        "/" + SLKeyExtractor::extractKey(interpolatedProps->text).value()
      );

      if ((*elementData).contains(elementDataPointer) && (*elementData)[elementDataPointer].is_string()) {
        interpolatedProps->text = (*elementData)[elementDataPointer].get<std::string>();
      }
    } catch (...) {}
  }

  for (const auto &childShadowNode : shadowNode->getChildren()) {
    auto const clonedChildShadowNode = cloneShadowNodeTree(runtime, elementData, childShadowNode);
    componentDescriptor.appendChild(nextShadowNode, clonedChildShadowNode);
  }

  return nextShadowNode;
}

}
