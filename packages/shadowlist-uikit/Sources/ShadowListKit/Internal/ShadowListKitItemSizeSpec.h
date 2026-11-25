#pragma once
#import <ShadowListKit/ShadowListKitItemSizeSpec.h>
#include <shadowlist-core/host/RowSizeSpec.hpp>

azimgd::shadowlist::RowSizeSpec ShadowListKitReadItemSizeSpec(ShadowListKitItemSizeSpec* spec);
azimgd::shadowlist::Size ShadowListKitMeasureItemSizeSpec(const azimgd::shadowlist::RowSizeSpec& spec, double width);
