#include "CraigsListItemShadowNode.h"
#include <iostream>

namespace facebook::react {

extern const char CraigsListItemComponentName[] = "CraigsListItem";

void CraigsListItemShadowNode::layout(LayoutContext layoutContext) {
  ensureUnsealed();
  ConcreteShadowNode::layout(layoutContext);
}

void CraigsListItemShadowNode::adjustLayout(bool visible) {
  ensureUnsealed();
    
  auto style = yogaNode_.style();

  if (visible) {
    style.setPadding(yoga::Edge::Left, yoga::StyleLength::points(100));
  } else {
    style.setPadding(yoga::Edge::Left, yoga::StyleLength::points(0));
  }

  yogaNode_.setStyle(style);
  yogaNode_.setDirty(true);
}

}
