#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace quadpack::detail {

struct Quad2D {
    std::uint32_t x;
    std::uint32_t y;
    std::uint32_t w;
    std::uint32_t h;
};

// rows[y] holds one row of the mask, bit x set meaning the face at (x, y) is
// visible. Bits at or above n_cols are ignored, because the cull leaves the two
// border bits of the bit axis set.
//
// Width is taken before height. Lysenko's order compares w before h, so the
// least element at a corner is the widest run, and taking height first gives a
// different partition with more quads on roughly five percent of regions.
std::vector<Quad2D> greedy_2d(const std::vector<std::uint64_t>& rows, std::size_t n_cols);

}  // namespace quadpack::detail