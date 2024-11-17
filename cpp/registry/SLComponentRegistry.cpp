#include "SLComponentRegistry.h"

int SLComponent::getIndex() const {
  return index;
}

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

void SLComponentRegistry::registerComponent(std::string uniqueId, int index) {
  components.emplace(index, SLComponent{uniqueId, index});
}

void SLComponentRegistry::unregisterComponent(std::string uniqueId, int index) {
  auto componentIter = components.find(index);
  if (componentIter != components.end()) {
    notifyObservers({componentIter->second}, false);
    components.erase(componentIter);
  }
}

void SLComponentRegistry::mountRange(int visibleStartIndex, int visibleEndIndex) {
  std::vector<int> indices(visibleEndIndex - visibleStartIndex);
  std::iota(indices.begin(), indices.end(), visibleStartIndex);
  mount(indices);
}

void SLComponentRegistry::mount(const std::vector<int> &indices) {
  std::unordered_set<int> componentSet(indices.begin(), indices.end());
  for (auto &componentPair : components) {
    SLComponent &component = componentPair.second;
    int index = component.getIndex();
    bool nextVisible = componentSet.count(index) > 0;
    if (component.getVisible() != nextVisible) {
      component.setVisible(nextVisible);
      notifyObservers({component}, nextVisible);
    }
  }
}

void SLComponentRegistry::unmount(const std::vector<int> &indices) {
  for (int index : indices) {
    auto componentIter = components.find(index);
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
      observerFunction(component.getUniqueId(), component.getIndex(), isVisible);
    }
  }
}

void SLComponentRegistry::updateVisibility(const std::vector<int> &indices, bool visible) {
  std::unordered_set<SLComponent> updatedComponents;

  for (int index : indices) {
    auto componentIter = components.find(index);
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
