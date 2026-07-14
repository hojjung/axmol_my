# meshoptimizer runtime decoder

This directory contains only the runtime files required to decode
`EXT_meshopt_compression` payloads.

- Upstream: https://github.com/zeux/meshoptimizer
- Pin: `v1.1` (`dc9d09ed83e1004aef47a1c3c597e0ec64848a37`)
- License: MIT, see `LICENSE.md`
- Source provenance: imported byte-identical to the copy tracked by ExaEngine
  on 2026-07-14, then guarded with `MESHOPTIMIZER_DECODER_ONLY`. The guards
  remove public encoder entry points from runtime objects without changing
  decoder code.

Encoder, optimizer, gltfpack, JavaScript and test sources are intentionally
not part of the engine target. Asset encoding belongs to the host-only
`axasset` pipeline.
