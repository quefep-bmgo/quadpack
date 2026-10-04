#include "greedy_2d.hpp"

#include <bit>

namespace quadpack::detail {

std::vector<Quad2D> greedy_2d(const std::vector<std::uint64_t>& rows, std::size_t n_cols) {
    std::vector<Quad2D> out;
    if (n_cols == 0 || n_cols > 62 || rows.empty()) {
        return out;
    }

    const std::uint64_t col_mask = (std::uint64_t{ 1 } << n_cols) - 1;
    std::vector<std::uint64_t> claimed(rows.size(), 0);

    for (std::size_t y = 0; y < rows.size(); ++y) {
        std::uint64_t open = rows[y] & col_mask & ~claimed[y];
        while (open != 0) {
            const auto x = static_cast<std::size_t>(std::countr_zero(open));

            std::size_t w = 1;
            while (x + w < n_cols && ((open >> (x + w)) & 1) != 0) {
                ++w;
            }

            const std::uint64_t span = ((std::uint64_t{ 1 } << w) - 1) << x;

            std::size_t h = 1;
            while (y + h < rows.size() && (rows[y + h] & span) == span
                   && (claimed[y + h] & span) == 0) {
                ++h;
            }

            for (std::size_t j = 0; j < h; ++j) {
                claimed[y + j] |= span;
            }
            open &= ~span;

            out.push_back(Quad2D{ static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y),
                                  static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h) });
        }
    }

    return out;
}

}  // namespace quadpack::detail