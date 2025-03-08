#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#import <React/RCTRootView.h>
#import <ReactCommon/RuntimeExecutor.h>

#include <react/renderer/mounting/stubs.h>
#include <react/renderer/mounting/Differentiator.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>

#include "ShadowlistTreeHelpers.hpp"

using namespace facebook::react;
using namespace azimgd::shadowlist;

@interface ShadowlistExampleTests : XCTestCase
@end

@implementation ShadowlistExampleTests

- (void)testRendersWelcomeScreen
{
  auto layoutContext = LayoutContext{};
  auto layoutConstraints = LayoutConstraints{
    {1000, 2000}
  };
  
  RuntimeExecutor runtimeExecutor = [](const std::function<void(jsi::Runtime&)>&) {};
  auto providerRegistry = std::make_shared<ComponentDescriptorProviderRegistry>();
  auto componentDescriptorRegistry = providerRegistry->createComponentDescriptorRegistry({});
  providerRegistry->add(concreteComponentDescriptorProvider<RootComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContainerComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContentComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());

  const std::string data = R"([
    {
      "__shadowlist_template_id": "TemplateOneComponentUniqueId",
      "uri": "http://example.com",
      "title": "template one title"
    }
  ])";

  /*
   * SLElement
   */
  auto headerComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto headerShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 100,
    .componentDescriptor = headerComponentDescriptor,
    .size = {500, 1000},
    .uniqueId = "ListHeaderComponentUniqueId"
  });

  /*
   * SLElement
   */
  auto footerComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto footerShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 102,
    .componentDescriptor = footerComponentDescriptor,
    .size = {500, 1000},
    .uniqueId = "ListFooterComponentUniqueId"
  });

  /*
   * SLElement
   */
  auto emptyComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto emptyShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 104,
    .componentDescriptor = emptyComponentDescriptor,
    .size = {500, 1000},
    .uniqueId = "ListEmptyComponentUniqueId"
  });

  /*
   * SLElement
   */
  auto dynamicComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto dynamicShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 106,
    .componentDescriptor = dynamicComponentDescriptor,
    .size = {500, 1000},
    .uniqueId = "ListDynamicComponentUniqueId"
  });

  /*
   * SLElement
   */
  auto templateOneComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto templateOneShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 108,
    .componentDescriptor = templateOneComponentDescriptor,
    .size = {500, 1000},
    .uniqueId = "TemplateOneComponentUniqueId"
  });

  /*
   * SLContent
   */
  auto contentComponentDescriptor = SLContentComponentDescriptor({{}, {}, nullptr});
  auto contentShadowNode = generateContentShadowNode<SLContentShadowNode>({
    .elementTag = 110,
    .componentDescriptor = contentComponentDescriptor,
    .size = {500, 1000},
  });

  /*
   * SLContainer
   */
  auto containerComponentDescriptor = SLContainerComponentDescriptor({{}, {}, nullptr});
  auto containerShadowNode = generateContainerShadowNode<SLContainerShadowNode>({
    .elementTag = 4,
    .componentDescriptor = containerComponentDescriptor,
    .size = {500, 1000},
    data
  });

  /*
   * Root
   */
  auto rootComponentDescriptor = RootComponentDescriptor({{}, {}, nullptr});
  auto rootShadowNode = generateRootShadowNode<RootShadowNode>({
    .elementTag = 2,
    .componentDescriptor = rootComponentDescriptor,
    .size = {500, 1000},
  });

  /*
   *
   */
  containerShadowNode->appendChild(contentShadowNode);
  containerShadowNode->appendChild(dynamicShadowNode);
  containerShadowNode->appendChild(headerShadowNode);
  containerShadowNode->appendChild(templateOneShadowNode);
  containerShadowNode->appendChild(emptyShadowNode);
  containerShadowNode->appendChild(footerShadowNode);

  auto rootShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(ShadowNode::ListOfShared{containerShadowNode});
  auto rootShadowNodeCloned = std::static_pointer_cast<const RootShadowNode>(
    rootShadowNode->ShadowNode::clone(ShadowNodeFragment{ShadowNodeFragment::propsPlaceholder(), rootShadowNodeChildren})
  );

  // Building an initial view hierarchy.
  auto viewTree = buildStubViewTreeWithoutUsingDifferentiator(*rootShadowNode);
  auto mutations = calculateShadowViewMutations(*rootShadowNode, *rootShadowNodeCloned);
  viewTree.mutate(mutations);

  std::vector<const LayoutableShadowNode*> affectedLayoutableNodes{};
  affectedLayoutableNodes.reserve(2048);
  std::const_pointer_cast<RootShadowNode>(rootShadowNodeCloned)->layoutIfNeeded(&affectedLayoutableNodes);

  /*
   *
   */
  rootShadowNodeCloned->cloneTree(containerShadowNode->getFamily(), [&](const ShadowNode& oldShadowNode) {
    auto& stateData = static_cast<const SLContainerShadowNode::ConcreteState&>(*oldShadowNode.getState()).getData();
    XCTAssertEqual(stateData.scrollContent.height, 3000, "stateData.scrollContent.height");
    XCTAssertEqual(stateData.scrollPosition.y, 0, "stateData.scrollPosition.y");
    
    return oldShadowNode.clone({});
  });
}

@end
