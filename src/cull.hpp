#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace quadpack::detail {

// The meshed extent, and the padded extent the caller must supply.
struct Grid {
    std::size_t extent[3];
    std::size_t padded[3];
};

// Which axis the caller's array is contiguous along. The cull packs 64 voxels
// of this axis into one word, so the caller's stride has to match or the library
// pays a transpose.
enum class BitAxis : std::uint8_t { x = 0, y = 1, z = 2 };

// Packs the caller's occupancy into one word per column along the bit axis.
// A column is addressed by its two other axes, and the layout follows the axis
// order: for bit axis z the word at index y * padded[x] + x holds one column of
// z.
std::vector<std::uint64_t> build_occupancy(const std::uint8_t* occupancy, const Grid& g,
                                           BitAxis bit, std::uint8_t air);

// Visible faces on one of the four directions perpendicular to the bit axis.
// out[(plane * extent[row_axis]) + row] holds one row, bit x set meaning the
// face at local bit position x is visible.
//
// Bits at or above extent[bit] are never set. The shift that converts padded
// positions to local ones drops the low halo, and the mask drops the high one.
std::vector<std::uint64_t> cull_in_plane(const std::vector<std::uint64_t>& occupancy, const Grid& g,
                                         BitAxis bit, int face_axis, bool positive);

}  // namespace quadpack::detail