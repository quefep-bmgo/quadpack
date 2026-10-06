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
using quadpack::detail::cull_normal;
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
    const int outer = bit == 0 ? 1 : 0;
    const int inner = bit == 2 ? 1 : 2;
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

// Fills expected[plane * e + row] bit k from the oracle. The three axis
// arguments say which axis each index runs along, which is what differs between
// the in-plane and normal cases.
void fill_expected(const quadpack::testing::Brute& brute, int plane_axis, int row_axis,
                   int bit_axis, bool positive, std::vector<std::uint64_t>& expected) {
    const std::size_t e = brute.extent();
    for (std::size_t plane = 0; plane < e; ++plane) {
        for (std::size_t row = 0; row < e; ++row) {
            std::uint64_t w = 0;
            for (std::size_t k = 0; k < e; ++k) {
                long c[3] = { 0, 0, 0 };
                c[plane_axis] = static_cast<long>(plane);
                c[row_axis] = static_cast<long>(row);
                c[bit_axis] = static_cast<long>(k);
                if (brute.visible_at(c[0], c[1], c[2], plane_axis, positive)) {
                    w |= std::uint64_t{ 1 } << k;
                }
            }
            expected[plane * e + row] = w;
        }
    }
}

void compare(const std::vector<std::uint64_t>& got, const std::vector<std::uint64_t>& want,
             const char* what, std::size_t e, int bit, int face_axis, bool positive) {
    if (got.size() != want.size()) {
        if (failures < 20) {
            std::printf("FAIL %s e=%zu bit=%d face=%d pos=%d size %zu vs %zu\n", what, e, bit,
                        face_axis, positive ? 1 : 0, got.size(), want.size());
        }
        ++failures;
        return;
    }
    for (std::size_t i = 0; i < got.size(); ++i) {
        if (got[i] != want[i]) {
            if (failures < 20) {
                std::printf("FAIL %s e=%zu bit=%d face=%d pos=%d at %zu have=%016llx "
                            "want=%016llx\n",
                            what, e, bit, face_axis, positive ? 1 : 0, i,
                            static_cast<unsigned long long>(got[i]),
                            static_cast<unsigned long long>(want[i]));
            }
            ++failures;
            return;
        }
    }
}

void check_in_plane(quadpack::testing::Brute& brute, int bit, int face_axis, bool positive) {
    const std::size_t e = brute.extent();
    auto occupancy =
        build_occupancy(pad(brute, bit).data(), grid_for(e), static_cast<BitAxis>(bit), 0);
    auto got = cull_in_plane(occupancy, grid_for(e), static_cast<BitAxis>(bit), face_axis, positive);

    const int row_axis = quadpack::testing::Brute::third(bit, face_axis);
    std::vector<std::uint64_t> want(e * e, 0);
    fill_expected(brute, face_axis, row_axis, bit, positive, want);
    compare(got, want, "in_plane", e, bit, face_axis, positive);
}

void check_normal(quadpack::testing::Brute& brute, int bit, bool positive) {
    const std::size_t e = brute.extent();
    auto occupancy =
        build_occupancy(pad(brute, bit).data(), grid_for(e), static_cast<BitAxis>(bit), 0);
    auto got = cull_normal(occupancy, grid_for(e), static_cast<BitAxis>(bit), positive);

    const int a0 = bit == 0 ? 1 : 0;
    const int a1 = bit == 2 ? 1 : 2;
    std::vector<std::uint64_t> want(e * e, 0);
    fill_expected(brute, bit, a0, a1, positive, want);
    compare(got, want, "normal", e, bit, bit, positive);
}

}  // namespace

int run_cull_tests() {
    // Every density at every extent, both signs, on all six axis pairs.
    unsigned seed = 12345u;
    for (std::size_t e = 1; e <= 6; ++e) {
        for (int bit = 0; bit < 3; ++bit) {
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                if (face_axis != bit) {
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
                            check_in_plane(brute, bit, face_axis, positive);
                        }
                    }
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
                        check_normal(brute, bit, positive);
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
            auto occupancy =
                build_occupancy(full.data(), grid_for(e), static_cast<BitAxis>(bit), 0);
            for (int face_axis = 0; face_axis < 3; ++face_axis) {
                if (face_axis != bit) {
                    for (bool positive : { true, false }) {
                        auto got = cull_in_plane(occupancy, grid_for(e),
                                                 static_cast<BitAxis>(bit), face_axis, positive);
                        for (std::uint64_t w : got) {
                            if (w != 0) {
                                if (failures < 20) {
                                    std::printf("FAIL solid halo in_plane e=%zu bit=%d face=%d "
                                                "pos=%d\n",
                                                e, bit, face_axis, positive ? 1 : 0);
                                }
                                ++failures;
                                break;
                            }
                        }
                    }
                }
                for (bool positive : { true, false }) {
                    auto got = cull_normal(occupancy, grid_for(e), static_cast<BitAxis>(bit),
                                           positive);
                    for (std::uint64_t w : got) {
                        if (w != 0) {
                            if (failures < 20) {
                                std::printf("FAIL solid halo normal e=%zu bit=%d pos=%d\n", e, bit,
                                            positive ? 1 : 0);
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
                if (face_axis != bit) {
                    for (bool positive : { true, false }) {
                        quadpack::testing::Brute one(e);
                        one.set(0, 0, 0, true);
                        check_in_plane(one, bit, face_axis, positive);

                        auto got = cull_in_plane(build_occupancy(pad(one, bit).data(),
                                                                 grid_for(e),
                                                                 static_cast<BitAxis>(bit), 0),
                                                 grid_for(e), static_cast<BitAxis>(bit), face_axis,
                                                 positive);
                        unsigned faces = 0;
                        for (std::uint64_t w : got) {
                            faces += static_cast<unsigned>(std::popcount(w));
                        }
                        if (faces != 1) {
                            if (failures < 20) {
                                std::printf("FAIL one voxel in_plane e=%zu bit=%d face=%d pos=%d "
                                            "faces=%u\n",
                                            e, bit, face_axis, positive ? 1 : 0, faces);
                            }
                            ++failures;
                        }
                    }
                }
                for (bool positive : { true, false }) {
                    quadpack::testing::Brute one(e);
                    one.set(0, 0, 0, true);
                    check_normal(one, bit, positive);

                    auto got = cull_normal(build_occupancy(pad(one, bit).data(), grid_for(e),
                                                           static_cast<BitAxis>(bit), 0),
                                           grid_for(e), static_cast<BitAxis>(bit), positive);
                    unsigned faces = 0;
                    for (std::uint64_t w : got) {
                        faces += static_cast<unsigned>(std::popcount(w));
                    }
                    if (faces != 1) {
                        if (failures < 20) {
                            std::printf("FAIL one voxel normal e=%zu bit=%d pos=%d faces=%u\n", e,
                                        bit, positive ? 1 : 0, faces);
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
                        grid.set(x, y, z, (s >> 16) % 100u < static_cast<unsigned>(density));
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

            // Each cull covers all six directions on its own, so each total
            // should match the grid-implied count. Checking them separately
            // catches an axis mix-up that a per-row comparison can hide.
            for (int which = 0; which < 2; ++which) {
                std::size_t have = 0;
                for (int face_axis = 0; face_axis < 3; ++face_axis) {
                    const int bit = which == 0 ? (face_axis + 1) % 3 : face_axis;
                    auto occupancy = build_occupancy(pad(grid, bit).data(), grid_for(e),
                                                     static_cast<BitAxis>(bit), 0);
                    for (bool positive : { true, false }) {
                        auto rows = which == 0
                                        ? cull_in_plane(occupancy, grid_for(e),
                                                        static_cast<BitAxis>(bit), face_axis,
                                                        positive)
                                        : cull_normal(occupancy, grid_for(e),
                                                      static_cast<BitAxis>(bit), positive);
                        for (std::uint64_t w : rows) {
                            have += static_cast<std::size_t>(std::popcount(w));
                        }
                    }
                }

                if (have != want) {
                    if (failures < 20) {
                        std::printf("FAIL face total e=%zu density=%d %s have=%zu want=%zu\n", e,
                                    density, which == 0 ? "in_plane" : "normal", have, want);
                    }
                    ++failures;
                }
            }
        }
    }

    return failures;
}