#import "Internal/ShadowListKitMenuController.h"

@implementation ShadowListKitMenuController {
  __weak id<ShadowListKitMenuHost> _host;
  UIContextMenuInteraction *_interaction;
  UIEditMenuInteraction *_editMenu;
  UIMenu *_pendingMenu;
  __weak UIView *_pendingContent;
}

- (instancetype)initWithHost:(id<ShadowListKitMenuHost>)host
{
  if (self = [super init]) { _host = host; [self refresh]; }
  return self;
}

- (void)refresh
{
  id<ShadowListKitMenuHost> host = _host;
  if (host.menusEnabled && !_interaction) {
    _interaction = [[UIContextMenuInteraction alloc] initWithDelegate:self];
    _editMenu = [[UIEditMenuInteraction alloc] initWithDelegate:self];
    [host.menuRootView addInteraction:_interaction];
    [host.menuRootView addInteraction:_editMenu];
  } else if (!host.menusEnabled && _interaction) {
    [self reset];
    [host.menuRootView removeInteraction:_interaction];
    [host.menuRootView removeInteraction:_editMenu];
    _interaction = nil;
    _editMenu = nil;
  }
}

- (void)reset
{
  [_interaction dismissMenu];
  [_editMenu dismissMenu];
  _pendingMenu = nil;
  _pendingContent = nil;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                    configurationForMenuAtLocation:(CGPoint)location
{
  id<ShadowListKitMenuHost> host = _host;
  UIView *content = [host menuContentAtPoint:location];
  if (!content || ![host canShowMenuForContent:content] || ![host isMenuContentValid:content]) return nil;
  UIMenu *menu = [host menuForContent:content];
  if (!menu.children.count || ![host isMenuContentValid:content]) return nil;
  return [UIContextMenuConfiguration configurationWithIdentifier:[host menuIdentifierForContent:content]
    previewProvider:nil actionProvider:^UIMenu *(NSArray<UIMenuElement *> *) { return menu; }];
}

- (UITargetedPreview *)previewForConfiguration:(UIContextMenuConfiguration *)configuration
{
  UIView *content = [_host menuContentForIdentifier:configuration.identifier];
  return content && [_host isMenuContentValid:content] ? [[UITargetedPreview alloc] initWithView:content] : nil;
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                            configuration:(UIContextMenuConfiguration *)configuration
    highlightPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
  return [self previewForConfiguration:configuration];
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction
                            configuration:(UIContextMenuConfiguration *)configuration
    dismissalPreviewForItemWithIdentifier:(id<NSCopying>)identifier
{
  return [self previewForConfiguration:configuration];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction
  willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration
                         animator:(id<UIContextMenuInteractionAnimating>)animator
{
  UIView *content = [_host menuContentForIdentifier:configuration.identifier];
  if (interaction == _interaction && content && [_host isMenuContentValid:content]) [_host contextMenuWillShowForContent:content];
}

- (BOOL)showMenuForContent:(UIView *)content atPoint:(CGPoint)point
{
  if (!_editMenu || ![_host isMenuContentValid:content]) return NO;
  UIMenu *menu = [_host menuForContent:content];
  if (!menu.children.count || ![_host isMenuContentValid:content]) return NO;
  _pendingMenu = menu;
  _pendingContent = content;
  [_editMenu presentEditMenuWithConfiguration:[UIEditMenuConfiguration configurationWithIdentifier:nil sourcePoint:point]];
  return YES;
}

- (UIMenu *)editMenuInteraction:(UIEditMenuInteraction *)interaction
          menuForConfiguration:(UIEditMenuConfiguration *)configuration
              suggestedActions:(NSArray<UIMenuElement *> *)suggestedActions
{
  UIMenu *menu = _pendingMenu;
  _pendingMenu = nil;
  return menu;
}

- (void)editMenuInteraction:(UIEditMenuInteraction *)interaction
  willPresentMenuForConfiguration:(UIEditMenuConfiguration *)configuration
                         animator:(id<UIEditMenuInteractionAnimating>)animator
{
  UIView *content = _pendingContent;
  if (interaction == _editMenu && content && [_host isMenuContentValid:content]) [_host contextMenuWillShowForContent:content];
}

@end
