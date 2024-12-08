#include <react/renderer/components/text/RawTextShadowNode.h>
#include "json.hpp"
#include "SLCommitHook.h"
#include "SLContainerShadowNode.h"
#include "SLElementShadowNode.h"


using namespace facebook::react;

namespace facebook::react {

int nextFamilyTag = -2;

class KeyExtractor {
public:
  static std::optional<std::string> extractKey(const std::string& input) {
    if (input.length() < 4) {
      return std::nullopt;
    }

    if (input.substr(0, 2) != "{{" || input.substr(input.length() - 2) != "}}") {
      return std::nullopt;
    }

    std::string key = input.substr(2, input.length() - 4);

    if (key.find('{') != std::string::npos || key.find('}') != std::string::npos) {
      return std::nullopt;
    }

    if (key.empty()) {
      return std::nullopt;
    }

    return key;
  }

  static std::vector<std::string> extractAllKeys(const std::string& input) {
    std::vector<std::string> keys;
    size_t pos = 0;
    
    while (pos < input.length()) {
      size_t start = input.find("{{", pos);
      if (start == std::string::npos) break;

      size_t end = input.find("}}", start);
      if (end == std::string::npos) break;

      std::string potential_key = input.substr(start, end - start + 2);
      auto key = extractKey(potential_key);

      if (key) {
        keys.push_back(*key);
      }

      pos = end + 2;
    }

    return keys;
  }
};

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -2e9;
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

ShadowNode::Shared cloneShadowNodeTree(jsi::Runtime *runtime, nlohmann::json* elementData, const ShadowNode::Shared& shadowNode)
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
      if (!KeyExtractor::extractKey(interpolatedProps->text).has_value()) {
        throw false;
      }
    
      auto elementDataPointer = nlohmann::json::json_pointer(
        "/" + KeyExtractor::extractKey(interpolatedProps->text).value()
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

class SLYogaLayoutableShadowNode : public facebook::react::YogaLayoutableShadowNode {
  public:
  facebook::yoga::Node& getYogaNode() {
    return yogaNode_;
  }
};

SLCommitHook::SLCommitHook(
  const std::shared_ptr<UIManager> &uiManager,
  jsi::Runtime *runtime) :
    uiManager_(uiManager),
    runtime_(runtime) {
  uiManager_->registerCommitHook(*this);
}

SLCommitHook::~SLCommitHook() noexcept {
  uiManager_->unregisterCommitHook(*this);
}

void SLCommitHook::commitHookWasRegistered(const UIManager& uiManager) noexcept {
}

void SLCommitHook::commitHookWasUnregistered(const UIManager& uiManager) noexcept {
}

RootShadowNode::Unshared SLCommitHook::shadowTreeWillCommit(
  ShadowTree const &,
  RootShadowNode::Shared const &oldRootShadowNode,
  RootShadowNode::Unshared const &newRootShadowNode) noexcept {

  auto rootShadowNode = newRootShadowNode->ShadowNode::clone(ShadowNodeFragment{});
  auto rootShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(
    *std::make_shared<const ShadowNode::ListOfShared>());

  if (!elementNodes_.size() || !containerNodes_.size()) {
    return std::static_pointer_cast<RootShadowNode>(rootShadowNode);
  }

  const auto nextRootShadowNode = rootShadowNode->cloneTree(
    containerNodes_.begin()->second->getFamily(),
    [this, rootShadowNodeChildren](const ShadowNode& oldShadowNode) {

    auto containerShadowNodeProps = const_cast<SLContainerShadowNode::ConcreteProps*>(
      static_cast<const SLContainerShadowNode::ConcreteProps*>(oldShadowNode.getProps().get())
    );

    auto containerData = std::make_shared<nlohmann::json>(nlohmann::json::parse(containerShadowNodeProps->data));

    for (const auto& elementNode : elementNodes_) {
      auto elementShadowNodeProps = const_cast<SLElementShadowNode::ConcreteProps*>(
        static_cast<const SLElementShadowNode::ConcreteProps*>(elementNode.second->getProps().get())
      );

      if ((*containerData).is_array() && elementShadowNodeProps->uniqueId == std::string("ListChildrenComponentUniqueId")) {
        for (int i = 0; i < (*containerData).size(); ++i) {
          auto elementDataPointer = nlohmann::json::json_pointer("/" + std::to_string(i));
          auto* elementData = &(*containerData)[elementDataPointer];
          rootShadowNodeChildren->push_back(cloneShadowNodeTree(runtime_, elementData, elementNode.second));
        }
      } else {
        // rootShadowNodeChildren->push_back(cloneShadowNodeTree(runtime_, elementData, elementNode.second));
      }
    }

    return oldShadowNode.clone({.state = oldShadowNode.getState(), .children = rootShadowNodeChildren});
  });

  if (nextRootShadowNode) {
    rootShadowNode = nextRootShadowNode;
  }

  return std::static_pointer_cast<RootShadowNode>(rootShadowNode);
}

void SLCommitHook::registerContainerNode(ShadowNode::Shared node) {
  containerNodes_[node->getTag()] = node;
}

void SLCommitHook::unregisterContainerNode(ShadowNode::Shared node) {
  containerNodes_.erase(node->getTag());
}

void SLCommitHook::registerElementNode(ShadowNode::Shared node) {
  elementNodes_[node->getTag()] = node;
}

void SLCommitHook::unregisterElementNode(ShadowNode::Shared node) {
  elementNodes_.erase(node->getTag());
}

std::unordered_map<Tag, ShadowNode::Shared> SLCommitHook::getContainerNodes() {
  return containerNodes_;
}

std::unordered_map<Tag, ShadowNode::Shared> SLCommitHook::getElementNodes() {
  return elementNodes_;
}

}
