#include "greedy_2d.hpp"

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

int failures = 0;

void fail(const char* what, unsigned long long seed_value, unsigned long long a, unsigned long long b) {
    if (failures < 10) {
        std::printf("FAIL %s  n=%llu  a=%llu  b=%llu\n", what, seed_value, a, b);
    }
    ++failures;
}

// An independent implementation of the same specification, written against a
// bool grid so it shares no code path with the bit version. If the two disagree
// on any region the bit manipulation has a bug.
std::size_t reference_quad_count(std::vector<char> cells, std::size_t rows, std::size_t cols) {
    std::size_t count = 0;
    const auto at = [&](std::size_t x, std::size_t y) { return cells[y * cols + x] != 0; };
    const auto clear = [&](std::size_t x, std::size_t y) { cells[y * cols + x] = 0; };

    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < cols; ++x) {
            if (!at(x, y)) {
                continue;
            }
            ++count;
            std::size_t w = 1;
            while (x + w < cols && at(x + w, y)) {
                ++w;
            }
            std::size_t h = 1;
            for (;;) {
                if (y + h >= rows) {
                    break;
                }
                bool all = true;
                for (std::size_t k = 0; k < w; ++k) {
                    if (!at(x + k, y + h)) {
                        all = false;
                        break;
                    }
                }
                if (!all) {
                    break;
                }
                ++h;
            }
            for (std::size_t j = 0; j < h; ++j) {
                for (std::size_t i = 0; i < w; ++i) {
                    clear(x + i, y + j);
                }
            }
        }
    }
    return count;
}

// Every set cell is covered exactly once.
void check_exact_cover(const std::vector<std::uint64_t>& rows, std::size_t cols,
                       const std::vector<quadpack::detail::Quad2D>& quads, std::size_t rows_n) {
    std::vector<std::uint64_t> cover(rows_n, 0);
    for (const auto& q : quads) {
        for (std::uint32_t j = 0; j < q.h; ++j) {
            const std::size_t y = q.y + j;
            if (y >= rows_n || q.x + q.w > cols) {
                fail("quad out of range", rows_n, q.x, q.y);
                return;
            }
            const std::uint64_t span = ((std::uint64_t{ 1 } << q.w) - 1) << q.x;
            if ((cover[y] & span) != 0) {
                fail("quad overlaps another", rows_n, q.x, q.y);
                return;
            }
            cover[y] |= span;
        }
    }
    const std::uint64_t col_mask = cols == 0 ? 0 : (std::uint64_t{ 1 } << cols) - 1;
    for (std::size_t y = 0; y < rows_n; ++y) {
        if ((cover[y] & col_mask) != (rows[y] & col_mask)) {
            fail("coverage differs from the mask", rows_n, y, 0);
            return;
        }
    }
}

void sweep(std::size_t rows_n, std::size_t cols) {
    const std::uint64_t cells = static_cast<std::uint64_t>(rows_n) * cols;
    for (std::uint64_t bits = 0; bits < (std::uint64_t{ 1 } << cells); ++bits) {
        std::vector<std::uint64_t> rows(rows_n, 0);
        std::vector<char> grid(cells, 0);
        for (std::size_t i = 0; i < cells; ++i) {
            if ((bits >> i) & 1) {
                rows[i / cols] |= std::uint64_t{ 1 } << (i % cols);
                grid[i] = 1;
            }
        }

        const auto quads = quadpack::detail::greedy_2d(rows, cols);

        check_exact_cover(rows, cols, quads, rows_n);

        if (quads.size() != reference_quad_count(grid, rows_n, cols)) {
            fail("quad count differs from the reference", bits, quads.size(), 0);
        }
    }
}

void test_worked_example() {
    const std::vector<std::uint64_t> rows = { 0b0011, 0b0001, 0b1111 };
    const auto quads = quadpack::detail::greedy_2d(rows, 4);

    struct Want {
        std::uint32_t x, y, w, h;
    };
    const Want want[] = { { 0, 0, 2, 1 }, { 0, 1, 1, 2 }, { 1, 2, 3, 1 } };

    if (quads.size() != 3) {
        fail("worked example quad count", 3, quads.size(), 0);
        return;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        if (quads[i].x != want[i].x || quads[i].y != want[i].y || quads[i].w != want[i].w
            || quads[i].h != want[i].h) {
            fail("worked example quad", i, quads[i].x, quads[i].y);
            return;
        }
    }
}

void test_degenerate() {
    if (!quadpack::detail::greedy_2d({}, 4).empty()) {
        fail("no rows should give no quads", 0, 0, 0);
    }
    if (!quadpack::detail::greedy_2d({ 0b1111 }, 0).empty()) {
        fail("zero columns should give no quads", 0, 0, 0);
    }
    // Border bits above n_cols must be ignored, which is what the cull leaves.
    const auto q = quadpack::detail::greedy_2d({ ~std::uint64_t{ 0 } }, 4);
    if (q.size() != 1 || q[0].w != 4 || q[0].h != 1) {
        fail("border bits leaked into the result", 0, q.size(), 0);
    }
}

}  // namespace

int run_cull_tests();

int main() {
    test_worked_example();
    test_degenerate();
    sweep(3, 3);
    sweep(4, 4);
    sweep(3, 6);
    sweep(5, 5);

    failures += run_cull_tests();
    if (failures == 0) {
        std::printf("cull_in_plane ok\n");
    }

    if (failures != 0) {
        std::printf("%d failures\n", failures);
        return EXIT_FAILURE;
    }
    std::printf("greedy_2d ok\n");
    return EXIT_SUCCESS;
}