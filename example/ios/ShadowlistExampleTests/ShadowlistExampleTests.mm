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
#import "SLContentShadowNode.h"
#import "SLContentProps.h"
#import "SLContentComponentDescriptor.h"

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
  auto layoutContext = LayoutContext{};
  auto layoutConstraints = LayoutConstraints{
    {1000, 1000}
  };

  /*
   * SLElement
   */
  auto headerElementTag = 100;
  auto headerContextContainer = std::make_shared<const ContextContainer>();
  auto headerEventDispatcher = EventDispatcher::Shared{};
  auto headerDescriptorParams = ComponentDescriptorParameters{headerEventDispatcher, headerContextContainer, nullptr};

  auto headerComponentDescriptor = SLElementComponentDescriptor(headerDescriptorParams);
  auto headerFamily = headerComponentDescriptor.createFamily({headerElementTag, 1, nullptr});
  
  auto headerProps = std::make_shared<const SLElementProps>();
  auto headerState = headerComponentDescriptor.createInitialState(headerProps, headerFamily);
  auto headerFragment = ShadowNodeFragment{headerProps, ShadowNodeFragment::childrenPlaceholder(), headerState};

  SLElementShadowNode::ConcreteProps* headerPropsUpdated = const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(headerProps.get()));
  headerPropsUpdated->uniqueId = "ListHeaderComponentUniqueId";
  
  auto headerShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(headerComponentDescriptor.createShadowNode(headerFragment, headerFamily))
  );

  auto headerLayoutMetrics = EmptyLayoutMetrics;
  headerLayoutMetrics.frame.origin = {500, 250};
  headerLayoutMetrics.frame.size = {500, 1000};
  headerShadowNode->setLayoutMetrics(headerLayoutMetrics);
  headerShadowNode->layoutTree(layoutContext, layoutConstraints);
  
  /*
   * SLElement
   */
  auto footerElementTag = 102;
  auto footerContextContainer = std::make_shared<const ContextContainer>();
  auto footerEventDispatcher = EventDispatcher::Shared{};
  auto footerDescriptorParams = ComponentDescriptorParameters{footerEventDispatcher, footerContextContainer, nullptr};

  auto footerComponentDescriptor = SLElementComponentDescriptor(footerDescriptorParams);
  auto footerFamily = footerComponentDescriptor.createFamily({footerElementTag, 1, nullptr});

  auto footerProps = std::make_shared<const SLElementProps>();
  auto footerState = footerComponentDescriptor.createInitialState(footerProps, footerFamily);
  auto footerFragment = ShadowNodeFragment{footerProps, ShadowNodeFragment::childrenPlaceholder(), footerState};

  SLElementShadowNode::ConcreteProps* footerPropsUpdated = const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(footerProps.get()));
  footerPropsUpdated->uniqueId = "ListFooterComponentUniqueId";

  auto footerShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(footerComponentDescriptor.createShadowNode(footerFragment, footerFamily))
  );
  
  auto footerLayoutMetrics = EmptyLayoutMetrics;
  footerLayoutMetrics.frame.origin = {500, 250};
  footerLayoutMetrics.frame.size = {500, 1000};
  footerShadowNode->setLayoutMetrics(footerLayoutMetrics);
  footerShadowNode->layoutTree(layoutContext, layoutConstraints);

  /*
   * SLElement
   */
  auto emptyElementTag = 104;
  auto emptyContextContainer = std::make_shared<const ContextContainer>();
  auto emptyEventDispatcher = EventDispatcher::Shared{};
  auto emptyDescriptorParams = ComponentDescriptorParameters{emptyEventDispatcher, emptyContextContainer, nullptr};

  auto emptyComponentDescriptor = SLElementComponentDescriptor(emptyDescriptorParams);
  auto emptyFamily = emptyComponentDescriptor.createFamily({emptyElementTag, 1, nullptr});

  auto emptyProps = std::make_shared<const SLElementProps>();
  auto emptyState = emptyComponentDescriptor.createInitialState(emptyProps, emptyFamily);
  auto emptyFragment = ShadowNodeFragment{emptyProps, ShadowNodeFragment::childrenPlaceholder(), emptyState};

  SLElementShadowNode::ConcreteProps* emptyPropsUpdated = const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(emptyProps.get()));
  emptyPropsUpdated->uniqueId = "ListEmptyComponentUniqueId";

  auto emptyShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(emptyComponentDescriptor.createShadowNode(emptyFragment, emptyFamily))
  );

  /*
   * SLElement
   */
  auto dynamicElementTag = 106;
  auto dynamicContextContainer = std::make_shared<const ContextContainer>();
  auto dynamicEventDispatcher = EventDispatcher::Shared{};
  auto dynamicDescriptorParams = ComponentDescriptorParameters{dynamicEventDispatcher, dynamicContextContainer, nullptr};

  auto dynamicComponentDescriptor = SLElementComponentDescriptor(dynamicDescriptorParams);
  auto dynamicFamily = dynamicComponentDescriptor.createFamily({dynamicElementTag, 1, nullptr});

  auto dynamicProps = std::make_shared<const SLElementProps>();
  auto dynamicState = dynamicComponentDescriptor.createInitialState(dynamicProps, dynamicFamily);
  auto dynamicFragment = ShadowNodeFragment{dynamicProps, ShadowNodeFragment::childrenPlaceholder(), dynamicState};

  SLElementShadowNode::ConcreteProps* dynamicPropsUpdated = const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(dynamicProps.get()));
  dynamicPropsUpdated->uniqueId = "ListDynamicComponentUniqueId";

  auto dynamicShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(dynamicComponentDescriptor.createShadowNode(dynamicFragment, dynamicFamily))
  );

  /*
   * SLElement
   */
  auto templateOneElementTag = 108;
  auto templateOneContextContainer = std::make_shared<const ContextContainer>();
  auto templateOneEventDispatcher = EventDispatcher::Shared{};
  auto templateOneDescriptorParams = ComponentDescriptorParameters{templateOneEventDispatcher, templateOneContextContainer, nullptr};

  auto templateOneComponentDescriptor = SLElementComponentDescriptor(templateOneDescriptorParams);
  auto templateOneFamily = templateOneComponentDescriptor.createFamily({templateOneElementTag, 1, nullptr});

  auto templateOneProps = std::make_shared<const SLElementProps>();
  auto templateOneState = templateOneComponentDescriptor.createInitialState(templateOneProps, templateOneFamily);
  auto templateOneFragment = ShadowNodeFragment{templateOneProps, ShadowNodeFragment::childrenPlaceholder(), templateOneState};

  SLElementShadowNode::ConcreteProps* templateOnePropsUpdated = const_cast<SLElementShadowNode::ConcreteProps*>(
    static_cast<const SLElementShadowNode::ConcreteProps*>(templateOneProps.get()));
  dynamicPropsUpdated->uniqueId = "TemplateOneComponentUniqueId";
  
  auto templateOneShadowNode = std::const_pointer_cast<SLElementShadowNode>(
    std::static_pointer_cast<const SLElementShadowNode>(templateOneComponentDescriptor.createShadowNode(templateOneFragment, templateOneFamily))
  );
  
  /*
   * SLContent
   */
  auto contentElementTag = 110;
  auto contentContextContainer = std::make_shared<const ContextContainer>();
  auto contentEventDispatcher = EventDispatcher::Shared{};
  auto contentDescriptorParams = ComponentDescriptorParameters{contentEventDispatcher, contentContextContainer, nullptr};

  auto contentComponentDescriptor = SLContentComponentDescriptor(contentDescriptorParams);
  auto contentFamily = contentComponentDescriptor.createFamily({contentElementTag, 1, nullptr});

  auto contentProps = std::make_shared<const SLContentProps>();
  auto contentState = contentComponentDescriptor.createInitialState(contentProps, contentFamily);
  auto contentFragment = ShadowNodeFragment{contentProps, ShadowNodeFragment::childrenPlaceholder(), contentState};

  auto contentShadowNode = std::const_pointer_cast<SLContentShadowNode>(
    std::static_pointer_cast<const SLContentShadowNode>(contentComponentDescriptor.createShadowNode(contentFragment, contentFamily))
  );
  
  /*
   * SLContainer
   */
  auto parentElementTag = 4;
  auto parentContextContainer = std::make_shared<const ContextContainer>();
  auto parentEventDispatcher = EventDispatcher::Shared{};
  auto parentDescriptorParams = ComponentDescriptorParameters{parentEventDispatcher, parentContextContainer, nullptr};

  auto parentComponentDescriptor = SLContainerComponentDescriptor(parentDescriptorParams);
  auto parentFamily = parentComponentDescriptor.createFamily({parentElementTag, 1, nullptr});
  
  auto parentProps = std::make_shared<const SLContainerProps>();
  auto parentState = parentComponentDescriptor.createInitialState(parentProps, parentFamily);
  auto parentFragment = ShadowNodeFragment{parentProps, ShadowNodeFragment::childrenPlaceholder(), parentState};

  SLContainerShadowNode::ConcreteProps* parentPropsUpdated = const_cast<SLContainerShadowNode::ConcreteProps*>(
    static_cast<const SLContainerShadowNode::ConcreteProps*>(parentProps.get()));
  parentPropsUpdated->data = "[{\"__shadowlist_template_id\":\"TemplateOneComponentUniqueId\",\"uri\":\"http://example.com\",\"title\":\"template one title\"}]";

  auto parentShadowNode = std::const_pointer_cast<SLContainerShadowNode>(
    std::static_pointer_cast<const SLContainerShadowNode>(parentComponentDescriptor.createShadowNode(parentFragment, parentFamily))
  );

  /*
   * Root
   */
  auto rootElementTag = 2;
  auto rootContextContainer = std::make_shared<const ContextContainer>();
  auto rootEventDispatcher = EventDispatcher::Shared{};
  auto rootDescriptorParams = ComponentDescriptorParameters{rootEventDispatcher, rootContextContainer, nullptr};

  auto rootComponentDescriptor = RootComponentDescriptor(rootDescriptorParams);
  auto rootFamily = rootComponentDescriptor.createFamily({rootElementTag, 1, nullptr});
  
  auto rootProps = std::make_shared<const RootProps>();
  auto rootState = rootComponentDescriptor.createInitialState(rootProps, rootFamily);
  auto rootFragment = ShadowNodeFragment{rootProps, ShadowNodeFragment::childrenPlaceholder(), rootState};

  auto rootShadowNode = std::const_pointer_cast<RootShadowNode>(
    std::static_pointer_cast<const RootShadowNode>(rootComponentDescriptor.createShadowNode(rootFragment, rootFamily))
  );

  /*
   * Layout phase
   */
  RuntimeExecutor runtimeExecutor = [](const std::function<void(jsi::Runtime&)>&) {};
  auto providerRegistry = std::make_shared<ComponentDescriptorProviderRegistry>();
  auto componentDescriptorRegistry = providerRegistry->createComponentDescriptorRegistry(parentDescriptorParams);
  providerRegistry->add(concreteComponentDescriptorProvider<RootComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContainerComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLContentComponentDescriptor>());
  providerRegistry->add(concreteComponentDescriptorProvider<SLElementComponentDescriptor>());

  /*
   *
   */
  parentShadowNode->appendChild(contentShadowNode);
  parentShadowNode->appendChild(dynamicShadowNode);
  parentShadowNode->appendChild(headerShadowNode);
  parentShadowNode->appendChild(templateOneShadowNode);
  parentShadowNode->appendChild(emptyShadowNode);
  parentShadowNode->appendChild(footerShadowNode);

  auto rootShadowNodeChildren = std::make_shared<ShadowNode::ListOfShared>(ShadowNode::ListOfShared{parentShadowNode});
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
  auto parentStateData = std::static_pointer_cast<SLContainerShadowNode::ConcreteState const>(parentState)->getData();
  XCTAssertEqual(rootShadowNodeCloned->getLayoutMetrics().frame.size.height, 200, "layoutmetrics size test");
}

@end
