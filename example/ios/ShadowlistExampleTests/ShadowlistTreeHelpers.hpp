#import "SLContainerComponentDescriptor.h"
#import "SLElementComponentDescriptor.h"
#import "SLContentComponentDescriptor.h"

using namespace facebook::react;
using namespace azimgd::shadowlist;

/*
 *
 */
template <typename ShadowNodeT>
struct GenerateElementShadowNodeParams {
  Tag elementTag;
  ComponentDescriptor& componentDescriptor;
  react::Size size;
  std::string uniqueId;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateElementShadowNode(const GenerateElementShadowNodeParams<ShadowNodeT>& params) {
  using ConcretePropsT = typename ShadowNodeT::ConcreteProps;

  auto props = std::make_shared<ConcretePropsT>();
  props->uniqueId = params.uniqueId;

  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(props, family);
  auto fragment = ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state};

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
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateContentShadowNode(const GenerateContentShadowNodeParams<ShadowNodeT>& params) {
  using ConcretePropsT = typename ShadowNodeT::ConcreteProps;

  auto props = std::make_shared<ConcretePropsT>();

  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(props, family);
  auto fragment = ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state};

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
  std::string data;
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateContainerShadowNode(const GenerateContainerShadowNodeParams<ShadowNodeT>& params) {
  using ConcretePropsT = typename ShadowNodeT::ConcreteProps;

  auto props = std::make_shared<ConcretePropsT>();
  props->data = params.data;

  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(props, family);
  auto fragment = ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state};

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
};

template <typename ShadowNodeT>
static std::shared_ptr<ShadowNodeT> generateRootShadowNode(const GenerateRootShadowNodeParams<ShadowNodeT>& params) {
  using ConcretePropsT = typename ShadowNodeT::ConcreteProps;

  auto props = std::make_shared<ConcretePropsT>();

  auto family = params.componentDescriptor.createFamily({params.elementTag, 1, nullptr});
  auto state = params.componentDescriptor.createInitialState(props, family);
  auto fragment = ShadowNodeFragment{props, ShadowNodeFragment::childrenPlaceholder(), state};

  auto shadowNode = params.componentDescriptor.createShadowNode(fragment, family);
  auto shadowNodeCasted = std::const_pointer_cast<ShadowNodeT>(
    std::static_pointer_cast<const ShadowNodeT>(shadowNode)
  );

  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.size = params.size;
  shadowNodeCasted->setLayoutMetrics(layoutMetrics);

  return shadowNodeCasted;
}
