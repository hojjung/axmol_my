# Math and container dependencies

These source snapshots are required parts of this Axmol fork. They are built
from source on every supported target and are not optional backends.

| Dependency | Upstream | Snapshot |
| --- | --- | --- |
| Corrade | <https://github.com/mosra/corrade> | 2025 source snapshot; Git tree `8e668690caa4a812f27ba0a9c029f6274e1f6f34` |
| Magnum | <https://github.com/mosra/magnum> | 2025 source snapshot; Git tree `671f04ac8cabfee4c448b952696228770656c6ec` |
| EnTT | <https://github.com/skypjack/entt> | 3.16.0; Git tree `9cf122b2a7f4129b4dc21be7ce8a36ed45843a39` |

Corrade and Magnum are a matched snapshot and must be updated together. The
upstream license files remain in each dependency directory:

- `corrade/COPYING` and `corrade/COPYING-examples`
- `magnum/COPYING`
- `entt/LICENSE`
