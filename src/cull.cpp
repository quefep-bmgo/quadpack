#include "cull.hpp"

#include <cassert>

namespace quadpack::detail {

namespace {

// The two axes that are not the bit axis, in ascending order.
std::size_t other_axis(BitAxis bit, int which) {
    const int b = static_cast<int>(bit);
    if (which == 0) {
        return b == 0 ? 1u : 0u;
    }
    return b == 2 ? 1u : 2u;
}

}  // namespace

std::vector<std::uint64_t> build_occupancy(const std::uint8_t* occupancy, const Grid& g, BitAxis bit,
                                           std::uint8_t air) {
    const std::size_t b = static_cast<std::size_t>(bit);
    const std::size_t a0 = other_axis(bit, 0);
    const std::size_t a1 = other_axis(bit, 1);

    std::vector<std::uint64_t> out(g.padded[a0] * g.padded[a1], 0);

    for (std::size_t i = 0; i < g.padded[a0]; ++i) {
        for (std::size_t j = 0; j < g.padded[a1]; ++j) {
            const std::size_t base = (i * g.padded[a1] + j) * g.padded[b];
            std::uint64_t word = 0;
            for (std::size_t k = 0; k < g.padded[b]; ++k) {
                if (occupancy[base + k] != air) {
                    word |= std::uint64_t{ 1 } << k;
                }
            }
            out[i * g.padded[a1] + j] = word;
        }
    }
    return out;
}

std::vector<std::uint64_t> cull_in_plane(const std::vector<std::uint64_t>& occupancy, const Grid& g,
                                         BitAxis bit, int face_axis, bool positive) {
    const std::size_t fa = static_cast<std::size_t>(face_axis);
    assert(static_cast<std::size_t>(bit) != fa && face_axis >= 0 && face_axis < 3);

    const std::size_t row_axis = other_axis(bit, 0) == fa ? other_axis(bit, 1) : other_axis(bit, 0);
    const std::size_t n_bit = g.extent[static_cast<std::size_t>(bit)];
    const std::uint64_t bit_mask = (std::uint64_t{ 1 } << n_bit) - 1;
    const long step = positive ? 1 : -1;

    // build_occupancy always writes a column at ia0 * padded[a1] + ia1, so the
    // stride is padded[a1] whichever axis the face axis happens to be. Only the
    // order of the two terms changes.
    const bool plane_is_outer = other_axis(bit, 0) == fa;
    const std::size_t stride = g.padded[other_axis(bit, 1)];

    std::vector<std::uint64_t> out(g.extent[fa] * g.extent[row_axis], 0);

    for (std::size_t plane = 1; plane <= g.extent[fa]; ++plane) {
        // The neighbour index stays inside the padded grid, so a face on the
        // chunk's outer surface reads the caller's halo. What that halo holds is
        // the caller's decision, and air is what keeps the outer faces.
        const std::size_t nb = static_cast<std::size_t>(static_cast<long>(plane) + step);

        for (std::size_t row = 1; row <= g.extent[row_axis]; ++row) {
            const std::size_t idx =
                plane_is_outer ? plane * stride + row : row * stride + plane;
            const std::size_t nidx =
                plane_is_outer ? nb * stride + row : row * stride + nb;

            // A face survives where its own voxel is solid and the one across it
            // is not. Shifting right by one moves the bit from padded index to
            // local index, which is what leaves bit 0 clear of the low halo.
            const std::uint64_t m = (occupancy[idx] & ~occupancy[nidx]) >> 1;

            out[(plane - 1) * g.extent[row_axis] + (row - 1)] = m & bit_mask;
        }
    }
    return out;
}

}  // namespace quadpack::detail