#include <react/renderer/components/text/RawTextShadowNode.h>
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

static SLContainerShadowNode::ConcreteProps* getSLContainerShadowNodeProps(const ShadowNode& containerShadowNode) {
  return const_cast<SLContainerShadowNode::ConcreteProps*>(
    static_cast<const SLContainerShadowNode::ConcreteProps*>(containerShadowNode.getProps().get())
  );
}

static SLContainerShadowNode::ConcreteState getSLContainerShadowNodeState(const ShadowNode& containerShadowNode) {
  return static_cast<const SLContainerShadowNode::ConcreteState&>(*containerShadowNode.getState());
}

static SLElementShadowNode::ConcreteProps* getSLElementShadowNodeProps(const ShadowNode& elementShadowNode) {
  return const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(elementShadowNode.getProps().get())
  );
}

static void updateElementShadowNodeStyle(ShadowNode::Unshared *elementShadowNode, Point elementShadowNodePosition) {
  const auto layoutableNode = std::static_pointer_cast<SLYogaLayoutableShadowNode>(*elementShadowNode);

  auto style = layoutableNode->getYogaNode().style();
  style.setPositionType(yoga::PositionType::Relative);
  layoutableNode->getYogaNode().setStyle(style);
  layoutableNode->getYogaNode().setDirty(true);
}

RootShadowNode::Unshared SLCommitHook::shadowTreeWillCommit(
  ShadowTree const &,
  RootShadowNode::Shared const &oldRootShadowNode,
  RootShadowNode::Unshared const &newRootShadowNode) noexcept {

  auto rootShadowNode = newRootShadowNode->ShadowNode::clone(ShadowNodeFragment{});

  /**
   * Iterate through SLContainer instances
   */
  for (const auto& containerNode : containerNodes_) {
    const auto nextRootShadowNode = rootShadowNode->cloneTree(containerNode.second->getFamily(), [this](const ShadowNode& containerShadowNode) {
      auto containerShadowNodeCloned = containerShadowNode.clone({});
      auto containerShadowNodeProps = getSLContainerShadowNodeProps(containerShadowNode);
      auto containerShadowNodeState = getSLContainerShadowNodeState(containerShadowNode);
      auto containerShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(*ShadowNode::emptySharedShadowNodeSharedList());

      /**
       * Iterate through SLElement instances
       */
      for (const auto& elementNode : elementNodes_) {
        auto elementShadowNodeProps = getSLElementShadowNodeProps(*elementNode.second);

        if (elementShadowNodeProps->uniqueId == std::string("ListChildrenComponentUniqueId")) {
          for (int i = 0; i < containerShadowNodeProps->data.size(); ++i) {
            auto* elementData = containerShadowNodeProps->getDataItem(i);
            auto elementShadowNodeCloned = SLTemplate::cloneShadowNodeTree(elementData, elementNode.second);

            updateElementShadowNodeStyle(&elementShadowNodeCloned, {});
            containerShadowNodeChildren->push_back(elementShadowNodeCloned);
          }
        } else {
          auto elementShadowNodeCloned = elementNode.second->clone({});
  
          updateElementShadowNodeStyle(&elementShadowNodeCloned, {});
          containerShadowNodeChildren->push_back(elementShadowNodeCloned);
        }
      }

      return containerShadowNodeCloned->clone({ .children = containerShadowNodeChildren });
    });

    if (nextRootShadowNode) {
      rootShadowNode = nextRootShadowNode;
    }
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
