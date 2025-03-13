#import "SLContainerComponentDescriptor.h"
#import "SLElementComponentDescriptor.h"
#import "SLContentComponentDescriptor.h"

using namespace facebook::react;
using namespace azimgd::shadowlist;

void layout(
  const std::shared_ptr<RootShadowNode>& rootShadowNode,
  const std::shared_ptr<ShadowNode>& containerShadowNode,
  std::function<std::shared_ptr<ShadowNode>(const ShadowNode&)> callback
) {
  auto rootShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(ShadowNode::ListOfShared{containerShadowNode});
  auto rootShadowNodeCloned = std::static_pointer_cast<const RootShadowNode>(
    rootShadowNode->ShadowNode::clone({ShadowNodeFragment::propsPlaceholder(), rootShadowNodeChildren})
  );

  auto viewTree = buildStubViewTreeWithoutUsingDifferentiator(*rootShadowNode);
  auto mutations = calculateShadowViewMutations(*rootShadowNode, *rootShadowNodeCloned);
  viewTree.mutate(mutations);

  std::vector<const LayoutableShadowNode*> affectedLayoutableNodes{};
  affectedLayoutableNodes.reserve(2048);

  std::const_pointer_cast<RootShadowNode>(rootShadowNodeCloned)->layoutIfNeeded(&affectedLayoutableNodes);
  rootShadowNodeCloned->cloneTree(containerShadowNode->getFamily(), callback);
}

/*
 *
 */
template <typename ShadowNodeT>
struct GenerateElementShadowNodeParams {
  Tag elementTag;
  ComponentDescriptor& componentDescriptor;
  react::Size size;
  std::shared_ptr<typename ShadowNodeT::ConcreteProps> props;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateElementShadowNode(const GenerateElementShadowNodeParams<ShadowNodeT>& params) {
  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(params.props, family);
  auto fragment = ShadowNodeFragment{params.props, ShadowNodeFragment::childrenPlaceholder(), state};

  auto shadowNode = params.componentDescriptor.createShadowNode(fragment, family);
  auto shadowNodeCasted = std::const_pointer_cast<ShadowNodeT>(
    std::static_pointer_cast<const ShadowNodeT>(shadowNode)
  );

  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.size = params.size;
  shadowNodeCasted->setLayoutMetrics(layoutMetrics);

  return shadowNodeCasted;
}

/*
 *
 */
template <typename ShadowNodeT>
struct GenerateContentShadowNodeParams {
  Tag elementTag;
  ComponentDescriptor& componentDescriptor;
  react::Size size;
  std::shared_ptr<typename ShadowNodeT::ConcreteProps> props;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateContentShadowNode(const GenerateContentShadowNodeParams<ShadowNodeT>& params) {
  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(params.props, family);
  auto fragment = ShadowNodeFragment{params.props, ShadowNodeFragment::childrenPlaceholder(), state};

  auto shadowNode = params.componentDescriptor.createShadowNode(fragment, family);
  auto shadowNodeCasted = std::const_pointer_cast<ShadowNodeT>(
    std::static_pointer_cast<const ShadowNodeT>(shadowNode)
  );

  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.size = params.size;
  shadowNodeCasted->setLayoutMetrics(layoutMetrics);

  return shadowNodeCasted;
}

/*
 *
 */
template <typename ShadowNodeT>
struct GenerateContainerShadowNodeParams {
  Tag elementTag;
  ComponentDescriptor& componentDescriptor;
  react::Size size;
  std::shared_ptr<typename ShadowNodeT::ConcreteProps> props;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateContainerShadowNode(const GenerateContainerShadowNodeParams<ShadowNodeT>& params) {
  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(params.props, family);
  auto fragment = ShadowNodeFragment{params.props, ShadowNodeFragment::childrenPlaceholder(), state};

  auto shadowNode = params.componentDescriptor.createShadowNode(fragment, family);
  auto shadowNodeCasted = std::const_pointer_cast<ShadowNodeT>(
    std::static_pointer_cast<const ShadowNodeT>(shadowNode)
  );

  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.size = params.size;
  shadowNodeCasted->setLayoutMetrics(layoutMetrics);

  return shadowNodeCasted;
}
 
/*
 *
 */
template <typename ShadowNodeT>
struct GenerateRootShadowNodeParams {
  Tag elementTag;
  ComponentDescriptor& componentDescriptor;
  react::Size size;
  std::shared_ptr<typename ShadowNodeT::ConcreteProps> props;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateRootShadowNode(const GenerateRootShadowNodeParams<ShadowNodeT>& params) {
  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(params.props, family);
  auto fragment = ShadowNodeFragment{params.props, ShadowNodeFragment::childrenPlaceholder(), state};

  auto shadowNode = params.componentDescriptor.createShadowNode(fragment, family);
  auto shadowNodeCasted = std::const_pointer_cast<ShadowNodeT>(
    std::static_pointer_cast<const ShadowNodeT>(shadowNode)
  );

  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.size = params.size;
  shadowNodeCasted->setLayoutMetrics(layoutMetrics);

  return shadowNodeCasted;
}
