#include "SLComponentRegistry.h"

std::string SLComponent::getUniqueId() const {
  return uniqueId;
}

bool SLComponent::getVisible() const {
  return isVisible;
}

void SLComponent::setVisible(bool visible) {
  isVisible = visible;
}

SLComponentRegistry::SLComponentRegistry() {}

void SLComponentRegistry::registerComponent(std::string uniqueId) {
  components.emplace(uniqueId, SLComponent{uniqueId});
}

void SLComponentRegistry::unregisterComponent(std::string uniqueId) {
  auto componentIter = components.find(uniqueId);
  if (componentIter != components.end()) {
    notifyObservers({componentIter->second}, false);
    components.erase(componentIter);
  }
}

void SLComponentRegistry::mount(const std::vector<std::string> &uniqueIds) {
  std::unordered_set<std::string> componentSet(uniqueIds.begin(), uniqueIds.end());
  for (auto &componentPair : components) {
    SLComponent &component = componentPair.second;
    bool nextVisible = componentSet.count(component.getUniqueId()) > 0;
    if (component.getVisible() != nextVisible) {
      component.setVisible(nextVisible);
      notifyObservers({component}, nextVisible);
    }
  }
}

void SLComponentRegistry::unmount(const std::vector<std::string> &uniqueIds) {
  for (std::string uniqueId : uniqueIds) {
    auto componentIter = components.find(uniqueId);
    if (componentIter != components.end()) {
      componentIter->second.setVisible(false);
      notifyObservers({componentIter->second}, false);
    }
  }
}

void SLComponentRegistry::mountObserver(const SLObserver &observer) {
  observers.push_back(observer);
}

void SLComponentRegistry::unmountObserver(const SLObserver &observer) {
  observers.erase(
    std::remove_if(observers.begin(), observers.end(),
      [&observer](const SLObserver &obs) {
        return obs.target<void(int, bool)>() == observer.target<void(int, bool)>();
      }),
    observers.end());
}

void SLComponentRegistry::notifyObservers(const std::unordered_set<SLComponent> &updatedComponents, bool isVisible) {
  for (const SLComponent &component : updatedComponents) {
    for (const auto &observerFunction : observers) {
      observerFunction(component.getUniqueId(), isVisible);
    }
  }
}

void SLComponentRegistry::updateVisibility(const std::vector<std::string> &uniqueIds, bool visible) {
  std::unordered_set<SLComponent> updatedComponents;

  for (std::string uniqueId : uniqueIds) {
    auto componentIter = components.find(uniqueId);
    if (componentIter != components.end()) {
      SLComponent &component = componentIter->second;
      if (component.getVisible() != visible) {
        component.setVisible(visible);
        updatedComponents.insert(component);
      }
    }
  }

  notifyObservers(updatedComponents, visible);
}
