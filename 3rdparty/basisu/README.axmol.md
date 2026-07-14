# Basis Universal runtime subset

- Upstream: https://github.com/BinomialLLC/basis_universal
- Pinned commit: `58e3afbabae592e97e6a736e0908c03bc7a4dd4f`
- Version: v2.1 plus the 2026-02-28 KTX2 DFD/KVD 64-bit bounds fix
- Included: decoder/transcoder sources and the single-file Zstandard decoder
- Excluded: encoder, CLI, examples, WebGL wrappers, and test assets

Local upstream guard patch:

- `basisu_transcoder.cpp` wraps the XUASTC arithmetic globals and private
  slice helpers in `BASISD_SUPPORT_XUASTC`. Upstream v2.1 leaves those two
  implementation blocks outside the feature guard, so the documented
  decoder-only `BASISD_SUPPORT_XUASTC=0` configuration does not compile.
- UASTC LDR capability reporting and dispatch are guarded by the same
  `BASISD_SUPPORT_*` switches used by ETC1S. Upstream v2.1 otherwise retains
  calls to disabled BC7/PVRTC/EAC targets, which keeps their implementations
  in size-optimized WebAssembly links and reports unavailable outputs as
  supported.

This directory is runtime-only. Host asset encoding belongs to `axasset` and
must not be linked into Axmol or WebAssembly products.
