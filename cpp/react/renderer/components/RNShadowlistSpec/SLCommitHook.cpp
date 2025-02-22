#include "SLCommitHook.h"

using namespace facebook::react;

namespace azimgd::shadowlist {

using namespace facebook;
using namespace facebook::react;

class SLYogaLayoutableShadowNode : public facebook::react::YogaLayoutableShadowNode {
  public:
  facebook::yoga::Node& getYogaNode() {
    return yogaNode_;
  }
};

SLCommitHook::SLCommitHook(const std::shared_ptr<UIManager> &uiManager) : uiManager_(uiManager){
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
