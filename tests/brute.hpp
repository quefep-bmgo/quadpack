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

    bool visible_at(long x, long y, long z, int face_axis, bool positive) const {
        if (!solid(x, y, z)) {
            return false;
        }
        long n[3] = { x, y, z };
        n[face_axis] += positive ? 1 : -1;
        return !solid(n[0], n[1], n[2]);
    }

    static int third(int a, int b) {
        for (int i = 0; i < 3; ++i) {
            if (i != a && i != b) {
                return i;
            }
        }
        return -1;
    }

  private:
    std::size_t extent_;
    std::vector<std::uint8_t> cells_;
};

}  // namespace quadpack::testing