#include "SLComponentRegistry.h"

int SLComponent::getComponentId() const {
  return componentId;
}

bool SLComponent::getVisible() const {
  return isVisible;
}

void SLComponent::setVisible(bool visible) {
  isVisible = visible;
}

SLComponentRegistry::SLComponentRegistry(int initialNumToRender) 
  : initialNumToRender(initialNumToRender) {}

void SLComponentRegistry::registerComponent(int componentId) {
  components.emplace(componentId, SLComponent{componentId});
}

void SLComponentRegistry::unregisterComponent(int componentId) {
  auto componentIter = components.find(componentId);
  if (componentIter != components.end()) {
    notifyObservers({componentIter->second.getComponentId()}, false);
    components.erase(componentIter);
  }
}

void SLComponentRegistry::mountRange(int visibleStartIndex, int visibleEndIndex) {
  std::vector<int> indices(visibleEndIndex - visibleStartIndex);
  std::iota(indices.begin(), indices.end(), visibleStartIndex);
  mount(indices);
}

void SLComponentRegistry::mount(const std::vector<int>& indices) {
  std::unordered_set<int> componentSet(indices.begin(), indices.end());
  for (auto& componentPair : components) {
    SLComponent& component = componentPair.second;
    int componentId = component.getComponentId();
    bool nextVisible = componentSet.count(componentId) > 0;
    if (component.getVisible() != nextVisible) {
      component.setVisible(nextVisible);
      notifyObservers({componentId}, nextVisible);
    }
  }
}

void SLComponentRegistry::unmount(const std::vector<int>& indices) {
  for (int componentId : indices) {
    auto componentIter = components.find(componentId);
    if (componentIter != components.end()) {
      componentIter->second.setVisible(false);
      notifyObservers({componentIter->first}, false);
    }
  }
}

void SLComponentRegistry::mountObserver(const SLObserver &observer) {
  observers.push_back(observer);
}

void SLComponentRegistry::unmountObserver(const SLObserver &observer) {
  observers.erase(
    std::remove_if(observers.begin(), observers.end(),
      [&observer](const SLObserver& obs) {
        return obs.target<void(int, bool)>() == observer.target<void(int, bool)>();
      }),
    observers.end());
}

void SLComponentRegistry::notifyObservers(const std::unordered_set<int>& updatedComponents, bool isVisible) {
  for (int componentId : updatedComponents) {
    for (const auto& observerFunction : observers) {
      observerFunction(componentId, isVisible);
    }
  }
}

void SLComponentRegistry::updateVisibility(const std::vector<int>& indices, bool visible) {
  std::unordered_set<int> updatedComponents;
  for (int componentId : indices) {
    auto componentIter = components.find(componentId);
    if (componentIter != components.end()) {
      SLComponent& component = componentIter->second;
      if (component.getVisible() != visible) {
        component.setVisible(visible);
        updatedComponents.insert(component.getComponentId());
      }
    }
  }
  notifyObservers(updatedComponents, visible);
}
