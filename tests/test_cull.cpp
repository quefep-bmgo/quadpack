#include "brute.hpp"
#include "cull.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <vector>

namespace {

int failures = 0;

using quadpack::detail::BitAxis;
using quadpack::detail::build_occupancy;
using quadpack::detail::cull_in_plane;
using quadpack::detail::Grid;

Grid grid_for(std::size_t e) {
    Grid g{};
    for (int a = 0; a < 3; ++a) {
        g.extent[a] = e;
        g.padded[a] = e + 2;
    }
    return g;
}

// Lays a local extent into a padded buffer with an air halo, which is the shape
// a caller hands the library. The bit axis is the contiguous one, so the test has
// to lay the buffer out differently for each of the three.
std::vector<std::uint8_t> pad(const quadpack::testing::Brute& b, int bit) {
    const std::size_t e = b.extent();
    const std::size_t p = e + 2;

    // Matches build_occupancy: bit axis stride 1, inner axis stride p, outer
    // axis stride p * p.
    int outer = bit == 0 ? 1 : 0;
    int inner = bit == 2 ? 1 : 2;
    std::size_t stride[3];
    stride[bit] = 1;
    stride[inner] = p;
    stride[outer] = p * p;

    std::vector<std::uint8_t> out(p * p * p, 0);
    std::size_t local[3];
    for (std::size_t k = 0; k < e; ++k) {
        local[bit] = k;
        for (std::size_t i = 0; i < e; ++i) {
            local[outer] = i;
            for (std::size_t j = 0; j < e; ++j) {
                local[inner] = j;
                std::size_t idx = 0;
                for (int a = 0; a < 3; ++a) {
                    idx += (local[a] + 1) * stride[a];
                }
                out[idx] = b.solid(static_cast<long>(local[0]), static_cast<long>(local[1]),
                                   static_cast<long>(local[2]))
                               ? 1
                               : 0;
            }
        }
    }
    return out;
}

void check(quadpack::testing::Brute& brute, int bit, int face_axis, bool positive) {
    const std::size_t e = brute.extent();
    const int row_axis = quadpack::testing::Brute::third(bit, face_axis);

    auto occupancy =
        build_occupancy(pad(brute, bit).data(), grid_for(e), static_cast<BitAxis>(bit), 0);
    auto got = cull_in_plane(occupancy, grid_for(e), static_cast<BitAxis>(bit), face_axis, positive);

    if (got.size() != e * e) {
        if (failures < 20) {
            std::printf("FAIL row count e=%zu bit=%d face=%d pos=%d\n", e, bit, face_axis,
                        positive ? 1 : 0);
        }
        ++failures;
        return;
    }

    for (std::size_t plane = 0; plane < e; ++plane) {
        for (std::size_t row = 0; row < e; ++row) {
            const std::uint64_t want = brute.visible_mask(bit, face_axis, positive, plane, row);
            const std::uint64_t have = got[plane * e + row];
            if (have != want) {
                if (failures < 20) {
                    std::printf("FAIL row e=%zu bit=%d face=%d pos=%d at %zu,%zu "
                                "have=%016llx want=%016llx\n",
                                e, bit, face_axis, positive ? 1 : 0, plane, row,
                                static_cast<unsigned long long>(have),
                                static_cast<unsigned long long>(want));
                }
                ++failures;
                return;
            }
        }
    }
    (void)row_axis;
}

}  // namespace

int run_cull_tests() {
    // Every density at every extent, both signs, on all six axis pairs.
    unsigned seed = 12345u;
    for (std::size_t e = 1; e <= 6; ++e) {
        for (int bit = 0; bit < 3; ++bit) {
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                if (face_axis == bit) {
                    continue;
                }
                for (bool positive : { true, false }) {
                    for (int density : { 0, 25, 50, 75, 100 }) {
                        quadpack::testing::Brute brute(e);
                        for (std::size_t x = 0; x < e; ++x) {
                            for (std::size_t y = 0; y < e; ++y) {
                                for (std::size_t z = 0; z < e; ++z) {
                                    seed = seed * 1103515245u + 12345u;
                                    const auto roll = static_cast<unsigned>((seed >> 16) % 100u);
                                    brute.set(x, y, z, roll < static_cast<unsigned>(density));
                                }
                            }
                        }
                        check(brute, bit, face_axis, positive);
                    }
                }
            }
        }
    }

    // A solid chunk inside a solid halo shows nothing, because every face has a
    // solid neighbour. The halo is what makes that true.
    for (std::size_t e = 1; e <= 4; ++e) {
        for (int bit = 0; bit < 3; ++bit) {
            quadpack::testing::Brute solid(e);
            solid.fill(true);
            std::vector<std::uint8_t> full = pad(solid, bit);
            std::fill(full.begin(), full.end(), std::uint8_t{ 1 });
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                if (face_axis == bit) {
                    continue;
                }
                for (bool positive : { true, false }) {
                    auto got = cull_in_plane(build_occupancy(full.data(), grid_for(e),
                                                             static_cast<BitAxis>(bit), 0),
                                             grid_for(e), static_cast<BitAxis>(bit), face_axis,
                                             positive);
                    for (std::size_t i = 0; i < got.size(); ++i) {
                        if (got[i] != 0) {
                            if (failures < 20) {
                                std::printf("FAIL solid halo leaked e=%zu bit=%d face=%d pos=%d\n", e,
                                            bit, face_axis, positive ? 1 : 0);
                            }
                            ++failures;
                            break;
                        }
                    }
                }
            }
        }
    }

    // A single voxel in an otherwise empty chunk has exactly one face per
    // direction, which is a fixed answer to check against.
    for (std::size_t e = 1; e <= 4; ++e) {
        for (int bit = 0; bit < 3; ++bit) {
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                if (face_axis == bit) {
                    continue;
                }
                for (bool positive : { true, false }) {
                    quadpack::testing::Brute one(e);
                    one.set(0, 0, 0, true);
                    check(one, bit, face_axis, positive);

                    auto got = cull_in_plane(build_occupancy(pad(one, bit).data(), grid_for(e),
                                                             static_cast<BitAxis>(bit), 0),
                                             grid_for(e), static_cast<BitAxis>(bit), face_axis,
                                             positive);
                    unsigned faces = 0;
                    for (std::uint64_t w : got) {
                        faces += static_cast<unsigned>(std::popcount(w));
                    }
                    if (faces != 1) {
                        if (failures < 20) {
                            std::printf("FAIL one voxel e=%zu bit=%d face=%d pos=%d faces=%u\n", e,
                                        bit, face_axis, positive ? 1 : 0, faces);
                        }
                        ++failures;
                    }
                }
            }
        }
    }

    // Every solid cell with air across at least one face contributes exactly one
    // face, so the total over all six directions is a count the grid itself
    // fixes. Checking it across directions catches an axis mix-up that a
    // per-row comparison can hide, since two wrong directions can cancel.
    for (std::size_t e = 1; e <= 5; ++e) {
        for (int density : { 25, 50, 75 }) {
            unsigned s = 777u + static_cast<unsigned>(e) * 31u + static_cast<unsigned>(density);
            quadpack::testing::Brute grid(e);
            for (std::size_t x = 0; x < e; ++x) {
                for (std::size_t y = 0; y < e; ++y) {
                    for (std::size_t z = 0; z < e; ++z) {
                        s = s * 1103515245u + 12345u;
                        grid.set(x, y, z,
                                 (s >> 16) % 100u < static_cast<unsigned>(density));
                    }
                }
            }

            std::size_t want = 0;
            for (std::size_t x = 0; x < e; ++x) {
                for (std::size_t y = 0; y < e; ++y) {
                    for (std::size_t z = 0; z < e; ++z) {
                        const auto lx = static_cast<long>(x);
                        const auto ly = static_cast<long>(y);
                        const auto lz = static_cast<long>(z);
                        if (!grid.solid(lx, ly, lz)) {
                            continue;
                        }
                        const long across[6][3] = { { lx + 1, ly, lz }, { lx - 1, ly, lz },
                                                    { lx, ly + 1, lz }, { lx, ly - 1, lz },
                                                    { lx, ly, lz + 1 }, { lx, ly, lz - 1 } };
                        for (const auto& nb : across) {
                            if (!grid.solid(nb[0], nb[1], nb[2])) {
                                ++want;
                            }
                        }
                    }
                }
            }

            std::size_t have = 0;
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                // Any bit axis other than the face axis works for an in-plane
                // direction, so pick one and take each direction exactly once.
                const int bit = (face_axis + 1) % 3;
                auto occupancy = build_occupancy(pad(grid, bit).data(), grid_for(e),
                                                 static_cast<BitAxis>(bit), 0);
                for (bool positive : { true, false }) {
                    auto rows = cull_in_plane(occupancy, grid_for(e),
                                              static_cast<BitAxis>(bit), face_axis, positive);
                    for (std::uint64_t w : rows) {
                        have += static_cast<std::size_t>(std::popcount(w));
                    }
                }
            }

            if (have != want) {
                if (failures < 20) {
                    std::printf("FAIL face total e=%zu density=%d have=%zu want=%zu\n", e, density,
                                have, want);
                }
                ++failures;
            }
        }
    }

    return failures;
}