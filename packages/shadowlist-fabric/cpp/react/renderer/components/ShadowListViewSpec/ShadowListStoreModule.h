#pragma once

namespace facebook::react {

/*
 * Registers the ShadowListStore C++ TurboModule, see ShadowListStoreJsi.h. Must run before JS
 * first asks for it: iOS and macOS call it from ShadowListView's +load, Android when its library
 * loads. Repeated calls do nothing.
 */
void registerShadowListStoreModule();

}
