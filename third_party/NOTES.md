# Prior art

What other projects do, and where they are wrong. This file is the reason
several decisions in this repo look the way they do, so when one of them looks
odd, read the entry here first and then argue with it.

Add to it when you read something. Delete an entry when it stops being true,
which is the same as saying the world changed or we were wrong.

## The two things we must credit

**Greedy meshing** is Mikola Lysenko's, from June and July 2012 on 0fps. The
total order on quads, the comparison pseudocode, the sweep-line formulation, and
the 8x approximation bound are all his. The blog posts also carry a comment
thread where he corrects the post twice, so read the errata.

- <https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/>
- <https://0fps.net/2012/07/07/meshing-minecraft-part-2/>

**The bitwise variant** is Erik Johansson's `binary-greedy-meshing`, MIT
licensed. The occupancy column mask, the six face masks, culling 64 faces per
word, and the 8 bytes per quad packing are his.

- <https://github.com/cgerikj/binary-greedy-meshing>
- v2 landed June 2024. Nothing merged since March 2025. Two stale open items.

## What the reference gets wrong

Read before copying. All of this is in the v2 `src/mesher.h`.

**The output buffer never grows from its default.** The growth is a doubling
resize guarded by a capacity field. With the capacity at zero the guard is true
on the first iteration, the resize is a no-op, and the multiply leaves it at
zero. The caller gets a silent out-of-bounds write, and nothing in the header
says a capacity has to be set first.

**The reported count is one too many.** `vertexCount = vertexI + 1` at the end
`mesh`. Harmless, and exactly the kind of thing a test that only checks
`count > 0` lets through.

**Chunk size is a compile-time constant.** `CS = 62` with no way to change it.
The 62 is not arbitrary, it is 64 minus a one voxel padding ring, but a library
should not make your world fit its constant.

**Input order is ZXY and the header says so once, in a comment.** A caller who
gets this wrong pays for it in a transpose they did not know they were paying.
See the vengi entry below.

**There is a preprocessor branch in the innermost loop.** `#ifdef _MSC_VER` for
`_BitScanForward64` versus `__builtin_ctzll`. `std::countr_zero` has been in
`<bit>` since C++20 and removes the branch.

**There are no tests.** The repository has none. The behaviour is only
demonstrated by a demo that prints timings.

**The benchmark reports no quad count.** The README gives a range, 50 to 200
microseconds on a Ryzen 3800X, with no terrain, no chunk size beyond 64 cubed,
and no compiler flags. A quad count is the cheapest useful sanity metric and it
is the one thing missing.

## The traps that are not that project's fault

**Memory layout matters, and we misread the evidence once.** `vengi` vendors
the reference and hit this. Commit `fcdebcf`, February 2026, is titled "removed
huge performance killer in using the binary mesher", and it deleted their XYZ to
ZXY reorder, `prepareChunk`. A deleted function is proof the copy existed and
was believed expensive.

The commit message carries a benchmark table, 6.44 ms `BeforeBinary` against
1.68 ms `AfterBinary`, and we originally read that as 3.8x attributable to
removing the transpose. **That was wrong and we had it written down in three
files before we checked.** `BeforeBinary` and `AfterBinary` are the pre-binary
and post-binary surface extractors, two different algorithms, not one algorithm
with and without a copy. There is also a `BinaryPrepareChunk` benchmark sitting
unused in their tree, which is the row that would have answered the question,
and it has no published output anywhere we could find.

So the transpose cost is real and the magnitude is unknown. Not one number for
it until we produce our own row.

**The face masks do not fit in cache.** Six directions of 62 by 62 64-bit masks
is about 180 KiB. A 32 KiB L1 does not hold that, so the merge reads from L2
or worse. `streaming-binary-greedy-meshing` keeps two rolling 11 KiB slices
instead and reports 2.67x over the whole array version, with a brute force
reference and a validator to prove equivalence. It is C++17, MIT, and it needs
AVX2 with BMI and BMI2, so it is not portable to every machine we care about.

**Ambient occlusion invalidates merging.** AO is a per-vertex quantity, so two
faces may only merge when all four corners agree. The reference v1 branch
handles it by comparing the eight neighbours around each candidate merge
position. That is the most expensive thing in the mesher by a wide margin. It
is also why the v2 rewrite dropped AO, since v1 is where the baked AO lives.

**Turbo is not the governor.** Nobody in this space reports median and spread.
Several report a single mean from a single long run, which averages across the
frequency drift curve and corresponds to no real operating point.

## The state of the C and C++ world

It is emptier than you would expect.

**There is no popular standalone C or C++ greedy meshing library.** GitHub's own
search for `greedy meshing` in C++ returns 17 repositories. The top one has 22
stars and is a game engine, not a library. The best known implementations are
JavaScript from 2013 and Rust.

| Project | Language | Licence | Stars | State |
|---|---|---|---|---|
| [godot_voxel](https://github.com/Zylann/godot_voxel) | C++ | MIT | 3917 | alive, an engine module |
| [vengi](https://github.com/vengi-voxel/vengi) | C and C++ | MIT | 1419 | alive, vendors the mesher |
| [binary-greedy-meshing](https://github.com/cgerikj/binary-greedy-meshing) | C | MIT | 318 | dormant since March 2025 |
| [meshing](https://github.com/vercidium-patreon/meshing) | C# | MIT | 669 | dormant since August 2024 |
| [binary_greedy_mesher_demo](https://github.com/TanTanDev/binary_greedy_mesher_demo) | Rust and Bevy | MIT or Apache | 389 | dormant since June 2024 |
| [binary-greedy-meshing](https://github.com/Inspirateur/binary-greedy-meshing) | Rust | MIT | 30 | alive, best API in the family |
| [streaming-binary-greedy-meshing](https://github.com/charagarlnad/streaming-binary-greedy-meshing) | C++17 | MIT | 0 | one day of work, experimental |

**The highest starred greedy meshing repo in the world is in C#.** That is the
opening.

**`binary-greedy-meshing` is taken as a name.** It exists on GitHub and on
crates.io, so we do not use it. `greedy-mesher` and `voxel-mesher` are taken on
npm. `cubemap` is 408 repositories in the unrelated skybox sense.

## Four more worth knowing about

**`block-mesh` is the incumbent API, not the C reference.** Rust, MIT, about
137,000 downloads a month, by the author of Bonsai. Nobody benchmarks against
`binary-greedy-meshing` except its own ports. They benchmark against this. Its
shape is `visible_block_faces` for speed and `greedy_quads` for quality, with a
`MergeStrategy` trait, an `AxisPermutation` type, and a `MergeVoxel` trait that
splits "can this voxel be seen" from "what value must match to merge". That last
split is the better idea in the whole space and we should take it.

**`finnbear/voxmesh` is new and worth reading.** Created February 2026, Rust,
MIT or Apache, one star and 51 commits. Block shapes including slabs and
billboards, three culling modes, smooth lighting, AO, and it has tests and
benches. It is the only library in the space I found that ships a test suite, so
read it before designing ours.

**`PRIArobotics/binary-greedy-meshing-py`** is a Python fork of the v1 branch
with voxel types, baked light and AO. Its benchmark table on a Ryzen 3800X is
worth having as a worst-case reference: 3D hills 1.14 ms for 46,798 vertices,
white noise 33.3 ms for 2.1 million, 3D checkerboard 36.8 ms for 4.3 million.
Checkerboard and white noise are the adversarial cases for greedy merging,
because no two faces ever merge. Any benchmark suite we write needs both.

**A daily.dev post claims 40x over a naive implementation** by reducing voxel
sampling from 1.1 million to 39,000 operations per chunk. Treat it as a blog
claim with no terrain, no chunk size and no code. The number is not usable.

## API ideas worth taking from elsewhere

**Two entry points that name the tradeoff.** The Rust port has `mesh`, which
builds the occupancy mask for you, and `fast_mesh`, which takes a mask you
already maintain. It reports the second at about 4x the first. Making that
explicit beats hiding it.

**An explicit padding contract.** `godot_voxel` exposes `get_minimum_padding()`
and `get_maximum_padding()` on its mesher. That states what neighbour data is
required instead of leaving it implied by a magic constant.

**A sealed mesher holding reusable scratch.** Both the C reference and the Rust
port converge on this independently. It is the only reason a `clear()` call
needs to exist at all.

## Measurement notes

**Use paired interleaved runs.** nanobench's changelog documents a test case
where two identical workloads came out 38 percent apart on a CI runner, each
reporting half a percent error. The spread inside one benchmark is not the
spread between two of them. Comparing two separate invocations measures the
machine's drift as much as the code.

**Warm up before timing, and reuse buffers.** A fresh heap allocation faults its
pages in on first write, and a core that was idle has not ramped its clock. Both
land entirely in the first sample.

**Never time under a sanitizer.** ASan is roughly 2x and it replaces the
allocator. Correctness under sanitizers, timing without them, two separate CI
jobs.

**Never time a single chunk operation.** Windows QPC is about 30 ns against an
invariant TSC and up to a microsecond against a motherboard timer. Time a batch
and divide.

**Measure the clock, do not trust it.** Some systems report 1 ns resolution and
tick at 30 us.

## Traps in our own tooling that are worth writing down

**`std::uniform_int_distribution` is not portable between standard libraries.**
The standard does not specify the algorithm, so libstdc++ and libc++ return
different values from the same engine and the same seed. A test suite using it
passes on Linux and fails on macOS with no code change. `std::mt19937_64` is
specified, so use that and draw bounded values by hand.

**libc++ lags libstdc++ on C++20 library features.** A Linux-only CI matrix
gives false confidence on exactly the features that differ. The bit counting
functions are level at version 9 everywhere. `std::bit_cast` needs libstdc++ 11
but libc++ 14, and `std::source_location` needs libc++ 16. If punning is the
only use, `std::memcpy` into a local has no version floor and compiles to the
same instruction.

**MSVC has no UBSan, no TSan and no MSan.** Not reduced support. Absent. Plan
CI matrix around it rather than assuming parity.

**MinGW ships no sanitizer runtimes.** WSL2 with the distro GCC is the local
path for ASan, UBSan and TSan on Windows.

**Leak detection is broken on arm64 macOS.** `llvm-project#131676`, and
`macos-latest` is now an M1. Keep leak detection on for Linux, where this is
mostly memory ownership code and the checker is worth having.

**Windows and macOS filesystems are case-insensitive.** Two files that differ
only in case will pass on both and fail on Linux. One shell step catches the
whole class, see the CI notes.
