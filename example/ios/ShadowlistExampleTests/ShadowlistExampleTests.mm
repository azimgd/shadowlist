#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import <React/RCTLog.h>
#import <React/RCTRootView.h>

#import "SLContainerShadowNode.h"
#import "SLContainerProps.h"
#import "SLContainerComponentDescriptor.h"
#import "SLElementShadowNode.h"
#import "SLElementProps.h"
#import "SLElementComponentDescriptor.h"

#include <react/renderer/mounting/stubs.h>
#include <react/renderer/mounting/Differentiator.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>

#include <ReactCommon/RuntimeExecutor.h>

using namespace facebook::react;
using namespace azimgd::shadowlist;

@interface ShadowlistExampleTests : XCTestCase
@end

@implementation ShadowlistExampleTests

- (void)testRendersWelcomeScreen
{
  /*
   * SLElement
   */
  auto childContextContainer = std::make_shared<const ContextContainer>();
  auto childEventDispatcher = EventDispatcher::Shared{};
  auto childDescriptorParams = ComponentDescriptorParameters{childEventDispatcher, childContextContainer, nullptr};

  auto childComponentDescriptor = SLElementComponentDescriptor(childDescriptorParams);
  auto childFamily = childComponentDescriptor.createFamily(ShadowNodeFamilyFragment{100, 1, nullptr});
  
  auto childProps = RootShadowNode::defaultSharedProps();
  auto childState = childComponentDescriptor.createInitialState(childProps, childFamily);
  auto childFragment = ShadowNodeFragment{childProps};

  auto childShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(childComponentDescriptor.createShadowNode(childFragment, childFamily))
  );

  /*
   * SLContainer
   */
  auto parentContextContainer = std::make_shared<const ContextContainer>();
  auto parentEventDispatcher = EventDispatcher::Shared{};
  auto parentDescriptorParams = ComponentDescriptorParameters{parentEventDispatcher, parentContextContainer, nullptr};

  auto parentComponentDescriptor = SLContainerComponentDescriptor(parentDescriptorParams);
  auto parentFamily = parentComponentDescriptor.createFamily(ShadowNodeFamilyFragment{4, 1, nullptr});
  
  auto parentProps = RootShadowNode::defaultSharedProps();
  auto parentState = parentComponentDescriptor.createInitialState(parentProps, parentFamily);
  auto parentFragment = ShadowNodeFragment{parentProps};

  auto parentShadowNode = std::const_pointer_cast<SLContainerShadowNode>(
    std::static_pointer_cast<const SLContainerShadowNode>(parentComponentDescriptor.createShadowNode(parentFragment, parentFamily))
  );

  /*
   * Root
   */
  auto rootContextContainer = std::make_shared<const ContextContainer>();
  auto rootEventDispatcher = EventDispatcher::Shared{};
  auto rootDescriptorParams = ComponentDescriptorParameters{rootEventDispatcher, rootContextContainer, nullptr};

  auto rootComponentDescriptor = RootComponentDescriptor(rootDescriptorParams);
  auto rootFamily = rootComponentDescriptor.createFamily(ShadowNodeFamilyFragment{2, 1, nullptr});
  
  auto rootProps = RootShadowNode::defaultSharedProps();
  auto rootState = rootComponentDescriptor.createInitialState(rootProps, rootFamily);
  auto rootFragment = ShadowNodeFragment{rootProps};

  auto rootShadowNode = std::const_pointer_cast<RootShadowNode>(
    std::static_pointer_cast<const RootShadowNode>(rootComponentDescriptor.createShadowNode(rootFragment, rootFamily))
  );

  /*
   * children
   */
  auto parentShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>();
  
  auto layoutMetrics = EmptyLayoutMetrics;
  layoutMetrics.frame.origin = {500, 250};
  layoutMetrics.frame.size = {500, 1000};

  auto elementShadowNodeLayoutable = std::static_pointer_cast<YogaLayoutableShadowNode>(childShadowNode);
  elementShadowNodeLayoutable->setLayoutMetrics(layoutMetrics);
  
  parentShadowNodeChildren->push_back(elementShadowNodeLayoutable);
  auto relativeLayoutMetrics = LayoutableShadowNode::computeRelativeLayoutMetrics(childShadowNode->getFamily(), *parentShadowNode, {});
  
  XCTAssertEqual(relativeLayoutMetrics.frame.size.height, 200, "layoutmetrics size test");
  
  /*
   * Layout phase
   */
  RuntimeExecutor runtimeExecutor = [](const std::function<void(jsi::Runtime&)>&) {};
  auto providerRegistry = std::make_shared<ComponentDescriptorProviderRegistry>();
  auto componentDescriptorRegistry = providerRegistry->createComponentDescriptorRegistry(parentDescriptorParams);
  providerRegistry->add(concreteComponentDescriptorProvider<RootComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContainerComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());

  /*
   *
   */
  auto rootShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(ShadowNode::ListOfShared{parentShadowNode});

  auto currentRootNode = std::static_pointer_cast<const RootShadowNode>(
    rootShadowNode->ShadowNode::clone(ShadowNodeFragment{
      ShadowNodeFragment::propsPlaceholder(),
      rootShadowNodeChildren
    })
  );

  // Building an initial view hierarchy.
  auto viewTree = buildStubViewTreeWithoutUsingDifferentiator(*rootShadowNode);
  viewTree.mutate(calculateShadowViewMutations(*rootShadowNode, *currentRootNode));
  
  std::vector<const LayoutableShadowNode*> affectedLayoutableNodes{};
  affectedLayoutableNodes.reserve(2048);

  std::const_pointer_cast<RootShadowNode>(currentRootNode)->layoutIfNeeded(&affectedLayoutableNodes);
}

@end
