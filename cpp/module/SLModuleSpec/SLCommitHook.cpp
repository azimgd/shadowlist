#include <react/renderer/components/text/RawTextShadowNode.h>
#include "json.hpp"
#include "SLCommitHook.h"
#include "SLContainerShadowNode.h"
#include "SLElementShadowNode.h"
#include "SLTemplate.h"

using namespace facebook::react;

namespace facebook::react {

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
          rootShadowNodeChildren->push_back(
            SLTemplate::cloneShadowNodeTree(runtime_, elementData, elementNode.second)
          );
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
