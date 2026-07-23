# Current-State Audit

This audit records the repository state before the multi-sample refactor. It separates observed
facts from unverified claims so later phases have a reproducible starting point.

## Audit Context

| Field | Observed value |
|---|---|
| Record date | 2026-07-23 |
| Git revision | `44577008dfcdcbcad68cfb39ccfc38bdd87165f5` |
| Worktree | Dirty: Phase 0 artifacts and the external Sponza directory are uncommitted |
| OS | Linux Mint 22.2, kernel 6.8.0-88-generic, x86_64 |
| CPU | Intel Core i7-8700K, 6 cores / 12 threads |
| RAM | 33,589,395,456 bytes |
| GPU | NVIDIA GeForce RTX 3060, 12,288 MiB |
| Driver | NVIDIA 570.153.02 |
| Compiler | GCC 13.3.0 |
| CMake | 3.28.3 |

The original audit did not capture runtime OpenGL strings. The Phase 0 baseline now records an
NVIDIA GeForce RTX 3060, OpenGL `4.3.0 NVIDIA 570.153.02`, and GLSL
`4.30 NVIDIA via Cg compiler`.

## Build and Source Findings

1. `CMakeLists.txt` defines only `game_engine`. That executable contains ray marching plus GLAD,
   shader, image, and camera sources. Sponza and the other legacy programs are not normal targets.
2. Nine files in `src/` define their own `main()`. They duplicate GLFW context setup, callbacks,
   input, camera, frame timing, asset lookup, and primitive rendering.
3. `src/sponza.cpp` has 1,054 lines. `include/load_gltf.h` has 1,023 lines and combines TinyGLTF
   parsing, decoded image retention, OpenGL upload, material/light conversion, hierarchy handling,
   and drawing behind one loader-owned interface.
4. Resource discovery depends on the process working directory. Ray marching and Sponza use the
   parent of `std::filesystem::current_path()` rather than a configured asset root.
5. CMake copies the complete `resources/` directory during configure. With the restored external
   Sponza archive, that can copy 6,714,394,828 bytes into every new build tree before compilation.
6. `external/imgui` is an empty gitlink and no `.gitmodules` file defines how to populate it.
   `src/main.cpp` therefore cannot be treated as a reproducible maintained target.
7. `src/hdr.cpp` declares `attachments` twice in the same function scope, at lines 168 and 207.
8. The current target has no explicit C++ standard, warning policy, CTest tests, CI configuration,
   fixed-frame execution, shader validation, or machine-readable benchmark mode.
9. GLM, GLAD, TinyGLTF, stb, and JSON implementation code is mixed with project headers under
   `include/`, obscuring project API boundaries.
10. Existing comments are inconsistent: some describe obsolete code or syntax, while ownership,
    render-pass inputs/outputs, binding contracts, coordinate spaces, and failure behavior are
    generally undocumented.

Phase 0 did not perform the architecture refactor, but it established C++17, added a temporary
`legacy_sponza` target, removed configure-time resource copying, added default/opt-in CTest, and
introduced deterministic baseline instrumentation. Phases 1-10 still own the structural findings.

## Known Correctness and Lifetime Risks

These findings are expected failures until a later phase closes them with tests:

- The original Sponza SSAO noise upload read beyond a 16-element `vec3` source while declaring a
  5x5 RGBA texture. Phase 0 corrected this memory-unsafe upload to 25 deterministic `vec4` values;
  the still-clear SSAO result is a separate rendering defect.
- Sponza allocates size-dependent G-buffer and SSAO attachments at the initial 800 by 600 size.
  The resize callback changes only the viewport/global dimensions, not those attachments.
- `GLTFLoader` retains its TinyGLTF model and decoded payload after GPU upload, and most owning GL
  handles are copyable public integers without automatic destruction.
- The glTF loader binds material textures even for passes that need only position/depth data.
- Lights with authored intensity below `0.001` are changed to intensity `1.0`, so zero-intensity
  glTF lights do not retain their authored semantics.
- No automated checks currently cover selected-scene handling, full matrix/TRS composition,
  normalized/strided/sparse accessors, non-indexed primitives, alpha modes, sampler state, color
  space, texture-coordinate selection, or transformed lights.
- Baseline mode now captures and reconciles structured GL-debug output. Live-object accounting and
  proof of correct destruction remain absent and are Phase 3 scope.

## Sponza Input Contract

The selected input is Intel's 2022 Sponza Base Scene, publisher content ID `830833`. Provenance,
license, local hashes, and the local glTF closure fingerprint are in `assets/manifest.json`.

Observed local facts:

| Property | Value |
|---|---:|
| External asset root | `resources/main-sponza/` |
| Complete extracted root | 154 files, 6,714,394,828 bytes |
| glTF entrypoint | `main_sponza/NewSponza_Main_glTF_003.gltf` |
| glTF entrypoint SHA-256 | `e04c4c540c74bdddcbd3f590a85c14119bfd5839702246a23b3a737c0cce2400` |
| Referenced URI closure | 74 files, 2,171,830,243 bytes |
| Closure SHA-256 | `2d6102dc617e6e5cfed37d5fe23674d518cfb6d1a6b2fbd86084010ce7246645` |
| Scene shape | 1 scene, 155 nodes, 115 meshes, 405 primitives, 28 materials, 72 images |
| License evidence | `main_sponza/credits_license.txt`, CC-BY-4.0 |
| Official download | `https://cdrdv2.intel.com/v1/dl/getContent/830833?fileName=main_sponza.zip` |
| Archive | `main_sponza.zip`, 3,987,608,266 bytes |
| Archive SHA-256 | `b8bb853884ab1566b3beb35666bd09882a4e0dc16661e4684e103792cf0229b9` |
| Archive MD5/ETag | `8c9c6c6f7d8bd75f15737379be181ba3` |
| Publisher metadata | Last modified 2025-05-29; S3 version `x5c5AxBlNIiVX0Zee9WfMSm0ODBGuILi` |

Intel does not publish the archive SHA-256. The manifest value is explicitly project-derived from
`/home/antash/Downloads/main_sponza.zip` on 2026-07-23, not publisher-issued. The archive's exact
size, SHA-256, and MD5 were independently recomputed locally before use.

The tool can fetch, checksum, safely extract, install, and verify into an empty external root:

```bash
python3 tools/assets/manage_assets.py fetch \
  --asset intel-sponza-base \
  --root /path/to/empty/external/root \
  --accept-license

python3 tools/assets/manage_assets.py verify --asset intel-sponza-base
```

The final modular installer completed an offline integration install from the pinned local archive
in 39.43 seconds with 161,352 KiB maximum RSS. Its staging verification reported the final
post-rename root and passed all closure and structural checks. The temporary installed copy was
then removed; neither the source
archive nor `resources/main-sponza/` was modified. A live network fetch was not performed, so
publisher availability and current HTTP response headers remain external dependencies.

The repository-local external root is ignored by Git and is never modified by the verifier.
The latest successful local verification output is retained at
`artifacts/verification/phase-0/sponza-asset-verify.json`.

## Deterministic CI Fixture

`tests/fixtures/gltf/phase0_scene.gltf` is a redistribution-safe, locally-authored fixture with:

- one indexed triangle with position, normal, and UV data;
- a non-default metallic-roughness material;
- a nested translated mesh node with expected world bounds; and
- a separate `KHR_lights_punctual` directional light node.

It is validated without the production loader. Expected positions, normals, UVs, indices, local
bounds, and transformed bounds are decoded from the embedded bytes rather than copied from accessor
metadata. Negative tests cover malformed containers, declared buffer bounds, sparse references,
punctual-light references, profile mismatches, checksums, unsafe archive names, ZIP traversal,
symlinks, size limits, hierarchy cycles, missing buffers, and failed-install cleanup. The latest
fixture result is retained at
`artifacts/verification/phase-0/fixture-verify.json`.

## Baseline Measurements

The two legacy executables now expose an opt-in deterministic baseline mode driven by
`tools/benchmarks/capture_legacy.py`. No-argument execution remains interactive. The runner records
startup spans, exact first-frame completion, retained CPU-memory scope, peak RSS, every raw CPU/GPU
frame sample, synchronous OpenGL diagnostics, and hashed numeric readbacks. The offline validator
recomputes frame summaries and readback statistics from the raw evidence, and independently
classifies the byte-counted and hashed GL-debug JSONL records, rather than relying on screenshots
or producer summaries.

Five current-contract captures for each sample were produced after the schema began binding and
reconciling GL-debug JSONL. Every record was independently validated against its raw artifacts
before and after deterministic-artifact deduplication. Cross-run results are:

| Measurement | Observed value |
|---|---:|
| First completed frame, median / p95 | 40,756 / 40,779 ms |
| glTF load/upload span, median / p95 | 38,787 / 39,051 ms |
| Peak RSS, median / p95 | 10,536,452,096 / 10,538,360,832 bytes |
| Retained CPU lower bound | 5,131,527,552 bytes |
| Per-run CPU-frame median, cross-run median | 2.591 ms |
| Per-run GPU-frame median, cross-run median | 43.821 ms |

The raw final-color and shadow-depth diagnostics were nonuniform. The view-position, view-normal,
and SSAO readbacks were entirely zero, establishing a legacy rendering defect through buffer
readback rather than visual comparison. The capture contained no OpenGL errors or non-finite
values; it retained 11 medium-severity NVIDIA shader-recompilation performance messages.

The accepted records and deduplicated raw artifacts are retained in
`artifacts/verification/phase-0/baseline-bundle.tar.gz`. The earlier partial Sponza loader record
remains at
`artifacts/verification/phase-0/sponza-legacy-loader.partial.json` as historical diagnostic
evidence; it does not satisfy or contribute samples to the complete-baseline gate.

The historical 78-second Sponza claim remains **unverified** because no matching asset, revision,
machine, command, raw log, or cache state was retained.

## Fixed Validation Inputs

`validation/acceptance.json` pins fixture hashes, camera states, resolutions, seed, tolerances,
repetitions, memory ceilings, and provisional pass/DDGI budgets. The legacy ray-marching projection
planes are `0.1`/`100.0` because `src/ray_marching.cpp` hardcodes them; Sponza uses the shared
camera defaults `0.5`/`60.0`.

Threshold rationale and change rules are documented in `validation/README.md`. These values define
future checks; they are not evidence that current rendering passes meet the budgets.

## Reproduction Commands

Fast, offline Phase 0 checks:

```bash
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/assets -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/benchmarks -p 'test_*.py' -v
python3 tools/assets/manage_assets.py verify-gltf \
  tests/fixtures/gltf/phase0_scene.gltf \
  --expect tests/fixtures/gltf/phase0_scene.expected.json
python3 tools/benchmarks/validate_benchmark.py \
  validation/schemas/benchmark.schema.json \
  artifacts/verification/phase-0/sponza-legacy-loader.partial.json
```

The full local closure integration check streams and hashes about 2.1 GiB:

```bash
python3 tools/assets/manage_assets.py verify --asset intel-sponza-base
```

Offline archive install and post-install verification:

```bash
python3 tools/assets/manage_assets.py install \
  --asset intel-sponza-base \
  --archive /home/antash/Downloads/main_sponza.zip \
  --root /tmp/mini-engine-sponza-install-test \
  --accept-license
python3 tools/assets/manage_assets.py verify \
  --asset intel-sponza-base \
  --root /tmp/mini-engine-sponza-install-test
```

A fresh Release configure/build with
`-DENGINE_SPONZA_ASSET_ROOT=/home/antash/workspace/engine/resources/main-sponza` succeeded. The
`asset|fixture|schema|baseline` CTest selection passed 4/4 tests, including the opt-in full closure
integration test. Without that explicit CMake cache path, only the ignored multi-gigabyte
integration test is omitted; fast asset, fixture, and schema tests remain registered.

## Phase 0 Conclusions

The complete Phase 0 record is `docs/verification/phase-0.md`. The checksum-gated literal fetch,
atomic install, separate verification, five-run baseline set, durable evidence, and independent
validation are complete.

Remaining limitations are inputs to later phases, not missing Phase 0 measurements:

1. The fetch used the exact checksum-verified local archive cache. Live publisher endpoint
   availability and current HTTP headers were not tested.
2. The accepted runs declare `cold_cache=false`; Phase 6 must add controlled cold/warm startup
   methodology before claiming an optimization.
3. Sponza's clear G-buffer/SSAO attachments remain a Phase 5 correctness failure.
4. Retained CPU staging remains 5,131,527,552 bytes and must be reduced in Phase 6.
