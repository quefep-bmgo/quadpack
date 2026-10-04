# quadpack

Greedy meshing for voxel chunks. Voxel occupancy in, merged quads out.

Not written yet. This page grows a benchmark table when there is something to
benchmark.

## Why it might be worth writing

`binary-greedy-meshing` is the reference everyone points at. It has a capacity
field that fails to grow when you leave it at the default, and both of its merge
loops implement a different order than the one Mikola Lysenko specified.
Measured over every 4 by 4 region, the two orders give different quad counts on
4.8 percent of them.

Details in `third_party/NOTES.md`.

## Usage

Nothing compiles yet. Roughly:

```cpp
#include <quadpack/quadpack.hpp>

using namespace quadpack;

Mesher<BitAxis::z> mesher;
Chunk<BitAxis::z> chunk = Chunk<BitAxis::z>::from_dense(occupancy, extent);

std::span<const Quad> quads = mesher.mesh(chunk);
```

The caller names their own contiguous axis, so there is no required memory
order. The mesher owns its scratch, so nothing allocates once it is warm.

## Prior art

Greedy meshing is Mikola Lysenko's, from his 2012 post
[Meshing in a Minecraft Game](https://0fps.net/2012/06/30/meshing-in-a-minecraft-game/).
The bitwise form is Erik Johansson's
[binary-greedy-meshing](https://github.com/cgerikj/binary-greedy-meshing), MIT.

## Building

CMake 3.28 or newer, target `quadpack::quadpack`.

## Licence

MIT. See [LICENSE](LICENSE).