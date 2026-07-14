# axasset

`axasset` is a native, host-only offline compiler. It converts FBX, OBJ, and
other Assimp-supported sources to an intermediate GLB, then runs the official
`gltfpack` executable with these default product settings:

```text
-cc -tc -kn -km
```

- `-cc`: `EXT_meshopt_compression`
- `-tc`: ETC1S Basis Universal supercompression in a KTX2 container
- `-kn -km`: preserve named scene nodes, meshes, and materials used by gameplay

Neither Assimp nor a texture encoder is linked into Axmol, WebAssembly, or the
mobile runtime. `AX_BUILD_AXASSET` defaults to `OFF`, and cross-compiling with it
enabled is a configure error.

Assimp-embedded textures that gltfpack cannot encode directly, including PSD,
are decoded and normalized to a temporary lossless PNG in the host process.
Decode or normalization failure aborts the conversion; axasset never labels
unconverted source bytes as `KHR_texture_basisu`.

## Build

Install a native Assimp package that includes the glTF2 exporter, and obtain a
native `gltfpack` built with texture-compression support. Then configure a
separate host build:

```sh
cmake -S . -B build-axasset -G Ninja \
  -DAX_BUILD_TESTS=OFF \
  -DAX_BUILD_AXASSET=ON \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/assimp/install \
  -DAX_GLTFPACK_EXECUTABLE=/absolute/path/to/gltfpack
cmake --build build-axasset --target axasset axasset-help-smoke
```

If gltfpack is not fixed at configure time, select it with `--gltfpack`, the
`AX_GLTFPACK` environment variable, or `PATH`, in that order.

## Convert

```sh
build-axasset/tools/axasset/axasset \
  --gltfpack /absolute/path/to/gltfpack \
  --output /absolute/path/to/Bat.glb \
  /absolute/path/to/Bat.FBX
```

ETC1S remains the default for compact albedo delivery. Use `--uastc` only when
the whole conversion should use gltfpack's `-tu` mode:

```sh
build-axasset/tools/axasset/axasset \
  --uastc \
  --gltfpack /absolute/path/to/gltfpack \
  --output /absolute/path/to/Bat-uastc.glb \
  /absolute/path/to/Bat.FBX
```

`--uastc` applies to every texture handled by this single gltfpack invocation.
It is not a per-semantic normal/mask switch. A mixed ETC1S-albedo/UASTC-normal
pipeline requires a separate semantic-aware preprocessing stage and is not
claimed by this tool.

The output directory must already exist. Existing outputs are rejected unless
`--force` is supplied. Replacement is staged in the output directory and the
old file is restored if the final rename fails. `gltfpack` is launched directly
with an argument vector; no command shell parses spaces or metacharacters in
paths. Its non-zero exit status is propagated by `axasset`.

Use `--keep-intermediate` only for importer/exporter diagnosis. Temporary files
are removed on every normal path.

## Asset and codec boundary

`CapsuleMonsterChess` Unit01 FBX/PSD files and JMO/AC shader code are licensed
reference assets. Never add them, derived textures, or converted GLBs to this
repository. They may be used only from their existing local path for manual A/B
validation. CI fixtures must be procedural or permissively licensed.

The shipped runtime contains only parsers and decoders. Basis Universal v2.1 is
the pinned runtime transcoder boundary; the encoder remains in the external
host `gltfpack` tool. Do not copy an older BasisU encoder into the runtime or
make `axasset` a dependency of any engine target.

Official references:

- <https://github.com/assimp/assimp>
- <https://github.com/zeux/meshoptimizer/blob/master/gltf/README.md>
