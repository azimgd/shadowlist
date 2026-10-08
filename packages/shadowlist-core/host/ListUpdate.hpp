#pragma once

#include <shadowlist-core/Constants.hpp>

#include <cstddef>
#include <vector>

namespace azimgd::shadowlist {

/*
 * Data changes a native list reads from its data source before they reach the core. Both kits
 * use these, which keeps one rule for every change.
 */

/*
 * Positions of rows inserted into the new data: sorted, each one once, and inside the new
 * data. A position past the end goes at the end: the k-th insert of the change lands at most at
 * previousCount + k. Hosts read the new keys at these positions.
 */
std::vector<std::size_t> insertionPositions(std::vector<std::size_t> indices, std::size_t previousCount);

/*
 * Positions of rows deleted from the old data: sorted, each one once. Positions past the end
 * name no row and are dropped.
 */
std::vector<std::size_t> deletionPositions(std::vector<std::size_t> indices, std::size_t previousCount);

}
