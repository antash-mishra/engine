# Mini-Engine

Mini-Engine is an OpenGL 4.3 rendering sandbox being reorganized around reusable engine libraries
and one multi-sample launcher. The maintained path currently contains the ray-marching sample.
Sponza and the older rendering experiments remain isolated compatibility code until their renderer,
asset pipeline, and tests are migrated through the phased plan.

## Current Structure

The default build produces:

- `engine_platform`: GLFW and native OpenGL context lifetime
- `engine_render`: maintained GLAD owner and shared rendering primitives
- `engine_assets`: deterministic asset-root resolution and validation
- `engine_scene`: renderer-independent scene state
- `engine_samples`: sample registry and sample implementations
- `render_samples`: command-line launcher

See [`docs/architecture.md`](docs/architecture.md) for ownership and dependency rules and
[`docs/adding-a-sample.md`](docs/adding-a-sample.md) for the extension workflow. The execution
sequence and phase gates are in
[`docs/plans/03-master-execution-plan.md`](docs/plans/03-master-execution-plan.md).

## Prerequisites

The verified Ubuntu 24.04 package resolution is recorded in `dependencies.lock.json`:

```bash
sudo apt-get update
sudo apt-get install --yes \
  $(python3 tools/build/verify_dependencies.py apt-arguments)
```

The maintained targets use CMake 3.23 or newer, Ninja, GCC-compatible C++17, GLFW 3.3 or newer,
OpenGL, and the vendored GLAD/GLM headers. Assimp and Draco are required only by opt-in legacy
targets.

## Build and Test

```bash
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
```

Use `release` for an optimized maintained build and `sanitizers` for AddressSanitizer plus
UndefinedBehaviorSanitizer:

```bash
cmake --preset release
cmake --build --preset release --parallel

cmake --preset sanitizers
cmake --build --preset sanitizers --parallel
ctest --preset sanitizers --output-on-failure
```

Run the maintained launcher:

```bash
build/dev/render_samples --list-samples
build/dev/render_samples --sample ray-marching --frames 10
```

`--frames` makes automated runs finite and deterministic. Use `--help` for the complete CLI.
Assets resolve from `--asset-root`, then `ENGINE_ASSET_ROOT`, then `assets/` beside the executable;
the process working directory is never an asset-root fallback.

## Legacy Compatibility

The old ray-marching and Sponza entrypoints are excluded from the default target graph. Build them
only while comparing migration behavior:

```bash
cmake -S . -B build/legacy -G Ninja \
  -DENGINE_BUILD_LEGACY_SAMPLES=ON \
  -DENGINE_SPONZA_ASSET_ROOT="$PWD/resources/main-sponza"
cmake --build build/legacy --target game_engine legacy_sponza
```

The Intel Sponza archive is external and is never copied into a build directory. Its pinned
manifest, validation commands, measured loading baseline, and license evidence are documented in
[`tools/assets/README.md`](tools/assets/README.md) and
[`docs/verification/phase-0.md`](docs/verification/phase-0.md).

## Status

The existing Sponza path is not yet a maintained sample and its Phase 0 readbacks identified clear
G-buffer/SSAO correctness failures. Loading also retains several gigabytes of CPU data and takes
tens of seconds on the reference system. These are explicit Phase 4 through Phase 6 work, not
claims of current functionality. DDGI is planned only after the shared runtime, GPU ownership,
scene pipeline, and Sponza correctness gates pass.

Rendering features are accepted with deterministic fixtures, analytic or CPU references, GPU
readback, invariants, ablation, temporal checks, and performance telemetry. Screenshots are
supplementary evidence rather than the correctness oracle.
