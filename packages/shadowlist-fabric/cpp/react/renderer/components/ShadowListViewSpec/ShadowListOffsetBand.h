#pragma once

#include "ShadowListViewShadowNode.h"
#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>

namespace facebook::react {

/*
 * The offset band to publish for this list, see azimgd::shadowlist::OffsetBand. The core
 * decides from its own state. On top of that, rows hidden until the host echoes a
 * correction and text size predictions still being measured both need more commits, so
 * the band stays empty and the host sends every frame until they are done.
 * Empty whenever SHADOWLIST_SCROLL_BAND is off. The caller holds the core lock.
 */
inline azimgd::shadowlist::OffsetBand shadowListOffsetBand(const ShadowListViewShadowNode& listShadowNode) {
  const auto& core = listShadowNode.getContainerManager();
  const auto& geometry = listShadowNode.getGeometryCache();
  if (!shadowListScrollBandEnabled() || !core || !geometry) {
    return {};
  }
  if (!geometry->concealedRows.empty()) {
    return {};
  }
  const auto& props = listShadowNode.getConcreteProps();
  bool measuringSizeSpecs = !props.elementsSizeSpecs.empty() &&
    !(geometry->sizeSpecsDone && geometry->sizeSpecsProps == listShadowNode.getProps());
  if (measuringSizeSpecs) {
    return {};
  }
  return core->computeOffsetBand();
}

/*
 * Whether the state already carries this band. Every empty band is the same default value,
 * so they compare equal too.
 */
inline bool shadowListOffsetBandPublished(const ShadowListViewState& stateData, const azimgd::shadowlist::OffsetBand& band) {
  return stateData.offsetBandLow_ == band.low && stateData.offsetBandHigh_ == band.high;
}

}
