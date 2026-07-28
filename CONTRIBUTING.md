# Contributing to Mirror Bridge

Thanks for your interest! Mirror Bridge is young and contributions of every
size are welcome — from typo fixes to new language backends.

## Quick orientation

- **[ARCHITECTURE.md](ARCHITECTURE.md)** — how the system fits together
- **[AGENTS.md](AGENTS.md)** — operating manual (written for AI agents, equally useful for humans)
- **[docs/guides/contributing.md](docs/guides/contributing.md)** — detailed development guide
- **[ROADMAP.md](ROADMAP.md)** — where the project is heading and what's up for grabs

## Development setup

You need a C++26 reflection compiler. The dev container has everything:

```bash
./start_dev_container.sh     # option 1: pull the pre-built image
```

Or natively with stock GCC 16.1+ (`g++ -std=c++26 -freflection`).

## Running tests

Two harnesses, both must stay green:

```bash
# Fast, trustworthy signal — exactly the modules CMake builds
cmake --preset default && cmake --build --preset default && ctest --preset default

# Full CLI-driven suite (what CI runs)
./tests/run_all_tests.sh
```

If you add a feature, add a test. Python tests live in `tests/` as
`test_*.py` next to a `*_binding.cpp`; register the module in
`tests/CMakeLists.txt` so ctest picks it up.

## Pull requests

1. Fork, branch from `main`, make your change.
2. Run both test harnesses locally — the dev container is the reference
   environment ([installation guide](docs/getting-started/installation.md)).
3. If you touched `core/`, `python/`, `lua/`, or `javascript/` headers,
   regenerate the single headers: `./amalgamate.sh`.
4. If you changed user-visible behavior, add a line to `CHANGELOG.md`
   under `[Unreleased]`.
5. Open the PR; CI must pass and one maintainer must approve.

## Good first contributions

Issues labeled [`good first issue`](https://github.com/FranciscoThiesen/mirror_bridge/labels/good%20first%20issue)
are scoped to be doable without deep reflection knowledge. Typical shapes:
a new type converter, a missing test, a docs fix, a CLI ergonomics tweak.

## Code style

- C++26, concepts over SFINAE, simplicity over cleverness
- Comments explain *why*, not *what* — no restating the code
- Match the surrounding code's density and naming

## Questions

Open a [discussion](https://github.com/FranciscoThiesen/mirror_bridge/discussions)
or an issue — there are no bad questions while the docs are still growing.
