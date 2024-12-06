#pragma once

#include <react/renderer/uimanager/UIManager.h>
#include <react/renderer/uimanager/UIManagerCommitHook.h>

using namespace facebook::react;

namespace facebook::react {

class SLCommitHook : public UIManagerCommitHook {
public:
  SLCommitHook(const std::shared_ptr<UIManager> &uiManager, jsi::Runtime *runtime);

  ~SLCommitHook() noexcept override;

  void commitHookWasRegistered(UIManager const &) noexcept override;
  void commitHookWasUnregistered(UIManager const &) noexcept override;

  RootShadowNode::Unshared shadowTreeWillCommit(
    ShadowTree const &shadowTree,
    RootShadowNode::Shared const &oldRootShadowNode,
    RootShadowNode::Unshared const &newRootShadowNode)
    noexcept override;
    
  void registerContainerNode(ShadowNode::Shared node);
  void unregisterContainerNode(ShadowNode::Shared node);
  void registerElementNode(ShadowNode::Shared node);
  void unregisterElementNode(ShadowNode::Shared node);
  std::unordered_map<Tag, ShadowNode::Shared> getContainerNodes();
  std::unordered_map<Tag, ShadowNode::Shared> getElementNodes();

private:
  std::shared_ptr<UIManager> uiManager_;
  jsi::Runtime *runtime_;
  std::unordered_map<Tag, ShadowNode::Shared> containerNodes_;
  std::unordered_map<Tag, ShadowNode::Shared> elementNodes_;
};

}
