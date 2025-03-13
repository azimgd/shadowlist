#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>
#import <React/RCTRootView.h>

#include <react/renderer/mounting/stubs.h>
#include <react/renderer/mounting/Differentiator.h>
#include <react/renderer/components/root/RootComponentDescriptor.h>
#include <react/renderer/components/view/ViewComponentDescriptor.h>

#include "ShadowlistTreeHelpers.hpp"
#include "SLRuntimeManager.h"

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

  auto providerRegistry = std::make_shared<ComponentDescriptorProviderRegistry>();
  auto componentDescriptorRegistry = providerRegistry->createComponentDescriptorRegistry({});
  providerRegistry->add(concreteComponentDescriptorProvider<RootComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContainerComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContentComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());

  nlohmann::json parsed = nlohmann::json::array();
  std::vector<std::string> uniqueIds;
  for (int i = 1; i <= 10; ++i) {
    nlohmann::json item = {
        {"__shadowlist_template_id", "TemplateOneComponentUniqueId"},
        {"uri", "http://example" + std::to_string(i) + ".com"},
        {"title", "template one title " + std::to_string(i)}
    };
    parsed.push_back(item);
    uniqueIds.push_back(std::to_string(i));
  }

  /*
   * SLElement
   */
  auto headerComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto headerProps = std::make_shared<typename SLElementShadowNode::ConcreteProps>();
  
  headerProps->uniqueId = "ListHeaderComponentUniqueId";

  auto headerShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 100,
    .componentDescriptor = headerComponentDescriptor,
    .size = {500, 1000},
    .props = headerProps
  });

  /*
   * SLElement
   */
  auto footerComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto footerProps = std::make_shared<typename SLElementShadowNode::ConcreteProps>();

  footerProps->uniqueId = "ListFooterComponentUniqueId";

  auto footerShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 102,
    .componentDescriptor = footerComponentDescriptor,
    .size = {500, 1000},
    .props = footerProps
  });

  /*
   * SLElement
   */
  auto emptyComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto emptyProps = std::make_shared<typename SLElementShadowNode::ConcreteProps>();

  emptyProps->uniqueId = "ListEmptyComponentUniqueId";

  auto emptyShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 104,
    .componentDescriptor = emptyComponentDescriptor,
    .size = {500, 1000},
    .props = emptyProps
  });

  /*
   * SLElement
   */
  auto dynamicComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto dynamicProps = std::make_shared<typename SLElementShadowNode::ConcreteProps>();

  dynamicProps->uniqueId = "ListDynamicComponentUniqueId";

  auto dynamicShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 106,
    .componentDescriptor = dynamicComponentDescriptor,
    .size = {500, 1000},
    .props = dynamicProps
  });

  /*
   * SLElement
   */
  auto templateOneComponentDescriptor = SLElementComponentDescriptor({{}, {}, nullptr});
  auto templateOneProps = std::make_shared<typename SLElementShadowNode::ConcreteProps>();

  templateOneProps->uniqueId = "TemplateOneComponentUniqueId";

  auto templateOneShadowNode = generateElementShadowNode<SLElementShadowNode>({
    .elementTag = 108,
    .componentDescriptor = templateOneComponentDescriptor,
    .size = {500, 1000},
    .props = templateOneProps
  });

  /*
   * SLContent
   */
  auto contentComponentDescriptor = SLContentComponentDescriptor({{}, {}, nullptr});
  auto contentProps = std::make_shared<typename SLContentShadowNode::ConcreteProps>();

  auto contentShadowNode = generateContentShadowNode<SLContentShadowNode>({
    .elementTag = 110,
    .componentDescriptor = contentComponentDescriptor,
    .size = {500, 1000},
    .props = contentProps
  });

  /*
   * SLContainer
   */
  auto containerComponentDescriptor = SLContainerComponentDescriptor({{}, {}, nullptr});
  auto containerProps = std::make_shared<typename SLContainerShadowNode::ConcreteProps>();
  
  auto containerShadowNode = generateContainerShadowNode<SLContainerShadowNode>({
    .elementTag = 4,
    .componentDescriptor = containerComponentDescriptor,
    .size = {500, 1000},
    .props = containerProps
  });

  /*
   * Root
   */
  auto rootComponentDescriptor = RootComponentDescriptor({{}, {}, nullptr});
  auto rootProps = std::make_shared<typename RootShadowNode::ConcreteProps>();

  auto rootShadowNode = generateRootShadowNode<RootShadowNode>({
    .elementTag = 2,
    .componentDescriptor = rootComponentDescriptor,
    .size = {500, 1000},
    .props = rootProps
  });

  /*
   * TESTCASE: default
   */
  auto containerShadowNodeCloned = std::const_pointer_cast<SLContainerShadowNode>(
    std::static_pointer_cast<const SLContainerShadowNode>(containerShadowNode->clone({})));

  containerShadowNodeCloned->appendChild(contentShadowNode);
  containerShadowNodeCloned->appendChild(dynamicShadowNode);
  containerShadowNodeCloned->appendChild(headerShadowNode);
  containerShadowNodeCloned->appendChild(templateOneShadowNode);
  containerShadowNodeCloned->appendChild(emptyShadowNode);
  containerShadowNodeCloned->appendChild(footerShadowNode);

  containerProps->parsed = parsed;
  containerProps->uniqueIds = uniqueIds;
  containerProps->inverted = false;

  containerShadowNodeCloned->layout(layoutContext);

  layout(rootShadowNode, containerShadowNodeCloned, [&](const ShadowNode& oldShadowNode) {
    auto& stateData = static_cast<const SLContainerShadowNode::ConcreteState&>(*oldShadowNode.getState()).getData();
    XCTAssertEqual(stateData.scrollContent.height, 3000, "stateData.scrollContent.height");
    XCTAssertEqual(stateData.scrollPosition.y, 0, "stateData.scrollPosition.y");
    
    return oldShadowNode.clone({});
  });
  
  /*
   * TESTCASE: inverted
   */
  auto containerShadowNodeClonedInverted = std::const_pointer_cast<SLContainerShadowNode>(
    std::static_pointer_cast<const SLContainerShadowNode>(containerShadowNode->clone({})));

  containerShadowNodeClonedInverted->appendChild(contentShadowNode);
  containerShadowNodeClonedInverted->appendChild(dynamicShadowNode);
  containerShadowNodeClonedInverted->appendChild(headerShadowNode);
  containerShadowNodeClonedInverted->appendChild(templateOneShadowNode);
  containerShadowNodeClonedInverted->appendChild(emptyShadowNode);
  containerShadowNodeClonedInverted->appendChild(footerShadowNode);

  containerProps->parsed = parsed;
  containerProps->uniqueIds = uniqueIds;
  containerProps->inverted = true;

  containerShadowNodeClonedInverted->layout(layoutContext);

  layout(rootShadowNode, containerShadowNodeClonedInverted, [&](const ShadowNode& oldShadowNode) {
    auto& stateData = static_cast<const SLContainerShadowNode::ConcreteState&>(*oldShadowNode.getState()).getData();
    XCTAssertEqual(stateData.scrollContent.height, 3000, "stateData.scrollContent.height");
    XCTAssertEqual(stateData.scrollPosition.y, 1000, "stateData.scrollPosition.y");
    
    return oldShadowNode.clone({});
  });

}

@end
