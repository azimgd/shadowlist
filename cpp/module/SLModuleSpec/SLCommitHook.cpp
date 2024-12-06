#include "SLCommitHook.h"
#include "SLContainerShadowNode.h"
#include "SLElementShadowNode.h"

using namespace facebook::react;

namespace facebook::react {

int nextFamilyTag = -2;

auto adjustFamilyTag = [](int tag) {
  const int MIN_TAG_VALUE = -2e9;
  const int CLAMPED_TAG = -2;
  return tag < MIN_TAG_VALUE ? CLAMPED_TAG : tag - 2;
};

ShadowNode::Shared cloneShadowNodeTree(jsi::Runtime *runtime, const ShadowNode::Shared& shadowNode)
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

  for (const auto &childShadowNode : shadowNode->getChildren()) {
    auto const clonedChildShadowNode = cloneShadowNodeTree(runtime, childShadowNode);
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

    for (const auto& elementNode : elementNodes_) {
      auto elementShadowNodeProps = const_cast<SLElementShadowNode::ConcreteProps*>(
        static_cast<const SLElementShadowNode::ConcreteProps*>(elementNode.second->getProps().get())
      );

      if (elementShadowNodeProps->uniqueId == std::string("ListChildrenComponentUniqueId")) {
        for (int i = 0; i < 100; ++i) {
          rootShadowNodeChildren->push_back(cloneShadowNodeTree(runtime_, elementNode.second));
        }
      } else {
        rootShadowNodeChildren->push_back(cloneShadowNodeTree(runtime_, elementNode.second));
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
