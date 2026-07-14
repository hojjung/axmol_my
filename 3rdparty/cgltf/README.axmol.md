# cgltf runtime parser

- Upstream: https://github.com/jkuhlmann/cgltf
- Pin: `v1.15` (`360db1a95480fe102ae9c69b27c5d101167ff5ba`)
- License: MIT, see `LICENSE`
- `cgltf.h` SHA-256:
  `e378a21c084bf1f288bb799de827bb26906efb024255f1ecf1705ea13f11c6ec`

Only the parser is vendored. The writer and any host conversion dependencies
must not be linked into Axmol or WebAssembly runtime targets.
