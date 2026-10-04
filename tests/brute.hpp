#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace quadpack::testing {

// A dense 3D grid of cells with no bit manipulation in it. Used as the oracle
// for the cull, which is only trustworthy if something else computes the
// answer a completely different way.
class Brute {
  public:
    explicit Brute(std::size_t extent) : extent_(extent) {
        cells_.assign(extent * extent * extent, 0);
    }

    std::size_t extent() const { return extent_; }

    void set(std::size_t x, std::size_t y, std::size_t z, bool v) {
        cells_[(x * extent_ + y) * extent_ + z] = v ? 1 : 0;
    }

    void fill(bool v) {
        for (auto& c : cells_) {
            c = v ? 1 : 0;
        }
    }

    // Out of bounds counts as air, which is the same assumption the library
    // makes when it reads the caller's halo.
    bool solid(long x, long y, long z) const {
        if (x < 0 || y < 0 || z < 0) {
            return false;
        }
        const auto i = static_cast<std::size_t>(x);
        const auto j = static_cast<std::size_t>(y);
        const auto k = static_cast<std::size_t>(z);
        if (i >= extent_ || j >= extent_ || k >= extent_) {
            return false;
        }
        return cells_[(i * extent_ + j) * extent_ + k] != 0;
    }

    // The visible faces of one in-plane slice, as a row of bits. plane is along
    // face_axis, row is along the remaining axis, and the bits run along bit.
    std::uint64_t visible_mask(int bit, int face_axis, bool positive, std::size_t plane,
                               std::size_t row) const {
        const auto row_axis = static_cast<std::size_t>(third(bit, face_axis));
        std::uint64_t w = 0;
        for (std::size_t k = 0; k < extent_; ++k) {
            if (visible(bit, face_axis, positive, plane, row_axis, row, k)) {
                w |= std::uint64_t{ 1 } << k;
            }
        }
        return w;
    }

    bool visible(int bit, int face_axis, bool positive, std::size_t plane, std::size_t row_axis,
                 std::size_t row, std::size_t k) const {
        long p[3] = { 0, 0, 0 };
        p[face_axis] = static_cast<long>(plane);
        p[row_axis] = static_cast<long>(row);
        p[bit] = static_cast<long>(k);

        if (!solid(p[0], p[1], p[2])) {
            return false;
        }
        p[face_axis] += positive ? 1 : -1;
        return !solid(p[0], p[1], p[2]);
    }

    static int third(int bit, int face_axis) {
        for (int a = 0; a < 3; ++a) {
            if (a != bit && a != face_axis) {
                return a;
            }
        }
        return -1;
    }

  private:
    std::size_t extent_;
    std::vector<std::uint8_t> cells_;
};

}  // namespace quadpack::testing