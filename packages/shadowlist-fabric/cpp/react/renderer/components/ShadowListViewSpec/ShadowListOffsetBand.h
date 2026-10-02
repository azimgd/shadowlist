#pragma once

#include "ShadowListViewShadowNode.h"
#include "ShadowListViewState.h"

#include <shadowlist-core/Container.hpp>

namespace facebook::react {

/*
 * The offset band to publish for this list, see azimgd::shadowlist::publishedOffsetBand.
 * Empty whenever SHADOWLIST_SCROLL_BAND is off. The caller holds the core lock.
 */
inline azimgd::shadowlist::OffsetBand shadowListOffsetBand(const ShadowListViewShadowNode& listShadowNode) {
  const auto& core = listShadowNode.getContainerManager();
  const auto& geometry = listShadowNode.getGeometryCache();
  if (!core || !geometry) {
    return {};
  }
  const auto& props = listShadowNode.getConcreteProps();
  bool measuringSizeSpecs = !props.elementsSizeSpecs.empty() && !geometry->sizeSpecs.finished(listShadowNode.getProps());
  return azimgd::shadowlist::publishedOffsetBand(
    *core, shadowListScrollBandEnabled(), !geometry->concealedRows.empty(), measuringSizeSpecs);
}

/*
 * Whether the state already carries this band. Every empty band is the same default value.
 * They compare equal too.
 */
inline bool shadowListOffsetBandPublished(const ShadowListViewState& stateData, const azimgd::shadowlist::OffsetBand& band) {
  return azimgd::shadowlist::offsetBandPublished(stateData.offsetBandLow_, stateData.offsetBandHigh_, band);
}

}
