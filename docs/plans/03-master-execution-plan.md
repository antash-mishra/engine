# Master Execution Plan

## Objective

Refactor the repository from a disordered collection of standalone OpenGL programs into a proper,
maintainable multi-sample rendering codebase. The primary deliverable is a shared sample launcher,
clear engine modules, explicit CPU/GPU ownership, reliable asset handling, tests, and a documented
path for adding new rendering samples without duplicating application infrastructure.

Sponza is the first complex migration and performance case. DDGI is a later rendering feature that
must use the shared framework; neither is allowed to introduce Sponza-specific behavior into the
engine architecture.

Detailed design references:

- [`01-codebase-refactor-plan.md`](01-codebase-refactor-plan.md)
- [`02-sponza-performance-ddgi-plan.md`](02-sponza-performance-ddgi-plan.md)
- [`04-phase-goals-and-verification.md`](04-phase-goals-and-verification.md) (authoritative phase
  status and completion evidence)

## Priority and Scope

The work is ordered around the codebase refactor:

1. Establish the build, module, launcher, sample, runtime, ownership, and asset boundaries.
2. Prove those boundaries by migrating multiple existing samples incrementally.
3. Migrate Sponza through the same public sample and scene interfaces, then fix and optimize it.
4. Implement DDGI as a reusable renderer capability only after the shared foundations are stable.
5. Remove the duplicated legacy entry points only after their replacements are verified.

The project is not considered properly refactored merely because Sponza or DDGI works. A basic new
sample must require only a sample implementation, its shaders/assets, and one registry entry; it
must not require changes to the application, platform runtime, or unrelated samples.

## Execution Rules

1. Work in small vertical changes. Each pull request must build, have a focused verification
   command, and leave the current default sample runnable.
2. Preserve output before improving it. Capture fixed-camera images and benchmark data before
   changing Sponza materials, passes, or assets.
3. Optimize measured spans only. Every loading optimization must show before/after stage timings,
   bytes, and relevant visual checks.
4. Keep OpenGL on the render thread. Worker jobs may read, parse, validate, decode, and prepare
   staging data only.
5. Make ownership move-only before adding destructors. Never add RAII deletion to a copyable GL
   handle type.
6. Do not carry false compatibility. A legacy sample is either migrated and tested, retained behind
   an explicit legacy option, or retired with a short record.
7. DDGI does not begin until transforms, normals, color space, render-target resize, and Sponza
   baseline tests are trustworthy.
8. Update README/docs in the same change that alters a user-facing command or capability.
9. Treat readability as a tested deliverable. Migrated public APIs, render passes, shader bindings,
   ownership rules, and complex algorithms must carry accurate contract-level comments, while
   routine code must remain understandable through naming and focused functions.

## Phase Overview

| Phase | Outcome | Depends on |
|---|---|---|
| 0. Baseline and inputs | Reproducible facts, Sponza asset contract, benchmark schema | None |
| 1. Build foundation | Reliable targets, dependencies, tests, sample launcher | Phase 0 |
| 2. Shared runtime | Common window/input/timing/resize/path behavior | Phase 1 |
| 3. GPU ownership | Move-only GL resources and validated shader/render targets | Phase 2 |
| 4. Scene pipeline | Testable glTF import, staged upload, pass-specific draw data | Phase 3 |
| 5. Sponza migration | Existing renderer runs in new framework with correctness fixes | Phase 4 |
| 6. Loading and frame optimization | Responsive, measured startup and bounded frame cost | Phase 5 |
| 7. Multi-sample framework completion | At least three maintained samples prove the refactor | Phases 3-5 |
| 8. DDGI capability | Reusable, validated software BVH tracing and probe storage | Phases 5-7 |
| 9. DDGI delivery | Stable single-bounce dynamic diffuse GI using shared engine interfaces | Phase 8 |
| 10. Consolidation | CI, docs, legacy removal, final performance report | Phases 7 and 9 |

## Phase 0: Baseline and Inputs

### Work

- Add `docs/audit/current-state.md` with build/source/asset findings and expected failures.
- Select the exact Sponza distribution and record URL, version, license, checksum, and unpacked size
  in `assets/manifest.json`.
- Add a fetch/verify tool and keep downloaded large assets outside Git.
- Create or select a tiny glTF fixture that covers mesh, PBR material, hierarchy, and light loading.
- Define startup benchmark JSON schema and fixed-camera visual capture settings.
- Capture current ray-marching output. Restore the historical Sponza setup on a reference machine
  long enough to capture load stage timings, first-frame time, memory, frame timings, and images.
- Mark the historical 78-second load claim as verified or unverified for that asset/machine.

### Exit Gate

- The ray-marching target builds with the documented command.
- The Sponza input can be fetched and checksum-verified, or an explicit blocker is recorded.
- Baseline artifacts identify git revision, hardware, driver, build type, asset profile, and settings.
- No optimization work starts without a baseline for the span it changes.

## Phase 1: Build Foundation

### Work

- Create `engine_platform`, `engine_render`, `engine_assets`, `engine_scene`, and `render_samples`
  CMake targets, initially with minimal sources.
- Set C++17, explicit OpenGL/GLFW/GLM dependencies, warnings, compile commands, Debug/Release
  behavior, and CTest.
- Add a pinned dependency manifest. Remove the broken ImGui gitlink or replace it with a valid
  optional package.
- Add `--list-samples`, `--sample`, `--asset-root`, `--width`, `--height`, and `--frames`.
- Register ray marching as the first sample without changing its shader behavior.
- Add a CI build job and one command-line/asset-path unit test.
- Add initial `docs/architecture.md` and `docs/adding-a-sample.md`; evolve them with later module
  and scene-pipeline changes.

### Exit Gate

```text
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
build/dev/render_samples --list-samples
build/dev/render_samples --sample ray-marching --frames 10
```

Equivalent final preset names may differ, but all commands must work from a clean checkout and be
documented.

## Phase 2: Shared Runtime

### Work

- Move GLFW/context lifecycle, callbacks, input states, timing, FPS, camera controller, and resize
  delivery into `Application`.
- Resolve assets from CLI/environment/executable location, never `current_path().parent_path()`.
- Install an OpenGL debug callback and collect error counts by sample/pass.
- Fix camera `LEFT`/`RIGHT` semantics and bindings with unit tests.
- Add minimized-window handling and clamp abnormally large frame deltas.
- Port a second small sample to prove that runtime code is not ray-marching-specific.

### Exit Gate

- Two samples share the runtime and contain no GLFW setup or global time/input state.
- Both run from repository root and build directory.
- Repeated resize, minimize/restore, focus, and mouse-capture smoke checks pass.

## Phase 3: GPU Ownership and Render Targets

### Work

- Add move-only `ShaderProgram`, `Buffer`, `VertexArray`, `Texture`, `Sampler`, `Framebuffer`,
  `RenderTarget`, and `GpuTimer` types.
- Make shader file, compilation, and linking errors fail sample initialization.
- Cache shader uniform locations and delete intermediate shader objects.
- Add debug labels and a live-object counter in debug builds.
- Port shared cube/quad/sphere primitives.
- Port one framebuffer-based legacy sample and make its attachments resize-safe.

### Exit Gate

- Migrated code has no raw owning GL handle globals and no manual cleanup block.
- Creating and destroying each migrated sample 20 times produces zero live-object and GL-error
  counts.
- A deliberate invalid shader and incomplete framebuffer fail with actionable messages.

## Phase 4: Scene and Asset Pipeline

### Work

- Move TinyGLTF/stb implementation macros into dedicated translation units.
- Implement namespaced `ImportedScene`, `GltfImporter`, `GpuScene`, and `RenderView`.
- Add typed, bounds-checked, strided, normalized, sparse, indexed, and non-indexed accessor support.
- Implement matrix/TRS hierarchy, default scene selection, bounds, materials, alpha modes, samplers,
  texture coordinate selection, emissive, and punctual lights.
- Separate image storage from sampler objects.
- Pack compatible primitives into shared GPU buffers and create opaque/masked/transparent draw lists.
- Add importer fixture tests and one upload/render/release integration test.

### Exit Gate

- Importer tests run without OpenGL.
- The CI fixture imports with expected counts, bounds, transforms, materials, and lights.
- CPU import and GPU upload are separate calls, and upload releases staging memory when configured.
- Depth, G-buffer, and PBR passes can consume draw records without calling a loader-owned render
  function.

## Phase 5: Sponza Migration and Correctness

### Work

- Create `SponzaSample` and explicit depth, G-buffer/SSAO, PBR, sky, and final-output passes.
- Reproduce the fixed-camera legacy output first.
- Resolve all P0 issues from the Sponza issue register.
- Fix size-dependent targets, SSAO screen coordinates/noise storage, hierarchy/light transforms,
  selected scene, material defaults, sampler wrap, alpha handling, tangent fallback, and color space.
- Fit shadow bounds from scene bounds and use explicit pass GL state.
- Add direct-only, IBL-only, SSAO-only, shadow-only, and combined baselines.
- Replace inaccurate Sponza documentation with tested capability statements.

### Exit Gate

- `render_samples --sample sponza --asset-profile ci --frames 10` is a required smoke test.
- Full Sponza runs on the reference machine with zero GL debug errors.
- Resize and all fixed-camera visual checks pass.
- No P0/P1 correctness issue remains open unless it has an approved replacement and regression test.

## Phase 6: Sponza Loading and Frame Optimization

### Work Order

1. Add complete startup spans, memory/count telemetry, and GPU pass timers.
2. Release TinyGLTF source/decoded payloads and CPU mesh staging after upload.
3. Cache precomputed IBL outputs.
4. Add deterministic `ci`, `dev`, and `full` processed asset profiles with mip chains and appropriate
   texture compression.
5. Move file IO, parsing, validation, and image decode to a bounded worker pool. Keep uploads queued
   on the render thread and show responsive progress.
6. Pack GPU buffers, deduplicate images, use sampler objects, and eliminate material work from depth
   and position/normal-only passes.
7. Add frustum culling and opaque material sorting.
8. Evaluate multi-draw, LOD, and occlusion culling only if profiler data still misses budgets.

### Exit Gate

- Development and full profiles meet the agreed cold/warm first-frame budgets or have a documented
  measurement-based exception.
- Main thread remains responsive after window creation.
- Retained staging memory meets its budget.
- Before/after JSON and visual comparisons accompany every accepted optimization.
- Frame timings identify a stable non-DDGI budget at 1080p for the DDGI work to consume.

## Phase 7: Multi-Sample Completion

This phase can run alongside Phase 6 after Phase 3 is stable, without changing scene pipeline APIs
that Phase 4 still owns.

### Work

- Migrate clustered deferred rendering because it exercises OpenGL 4.3 compute and SSBO paths
  needed by DDGI.
- Migrate PBR/IBL next, sharing the corrected lighting/color-space code with Sponza.
- Migrate or merge HDR/bloom, terrain, and basic samples based on distinct educational value.
- Fix the `hdr.cpp` declaration conflict during migration; do not patch unused legacy code solely to
  make a false all-source build claim.
- Document retired samples and remove their stale assets/shaders only after dependency checks.

### Exit Gate

- Ray marching, Sponza, and clustered deferred samples are default-built and smoke-tested.
- Adding a minimal sample requires no platform/runtime edits.
- The README's sample list is generated from or checked against the registry.

## Phase 8: DDGI Capability

### Work

- Define DDGI settings, resource ownership, pass inputs/outputs, history reset rules, and debug
  counters without putting Sponza types in `engine_render`.
- Build/cache a static Sponza triangle BVH and upload compact nodes/triangles/material hit data.
- Implement compute-shader ray traversal and a CPU reference fixture.
- Measure rays/ms, traversal depth, and overflow rate.
- Add probe volume atlas addressing, ping-pong history, deterministic ray rotation, and probe debug
  rendering.

### Exit Gate

- GPU and CPU reference rays agree within tolerance on the test fixture.
- No traversal overflow or out-of-range access occurs.
- The reference GPU supports an update preset within the agreed DDGI budget. If it does not, record
  a decision on coarse voxel/DDA fallback before continuing.
- Probe atlas/addressing tests pass and debug visualization is usable.

## Phase 9: DDGI Delivery

### Work

- Trace a rotating subset of probe rays each frame and evaluate direct/emissive hit radiance plus
  environment misses.
- Blend irradiance and distance moments temporally.
- Sample eight surrounding probes for diffuse indirect light in the PBR path.
- Add history invalidation, visibility weighting, normal/view bias, classification, and relocation.
- Add low/medium/high budgets and direct-only, indirect-only, combined, probe, ray, atlas, and
  traversal debug modes.
- Validate static convergence, moving lights, abrupt light changes, camera motion, and long runs.

### Exit Gate

- The validation fixture shows expected color transfer.
- Sponza responds to dynamic light changes with temporally converging indirect diffuse lighting.
- Medium preset meets the agreed 1080p GPU budget.
- Output has no NaN/Inf values, unbounded energy growth, persistent severe leaks in agreed camera
  views, GL errors, or live-resource leaks.
- DDGI can be toggled and reset at runtime.

## Phase 10: Consolidation

### Work

- Run the complete benchmark/regression matrix and publish a final reference report.
- Add/finish CI jobs for build, unit, importer, shader, smoke, sanitizer, and selected visual tests.
- Remove the legacy CMake option, old loaders, duplicate vendor headers, dead sample entry points,
  stale shaders, and unused assets.
- Review public headers for ownership, const correctness, namespaces, and dependency direction.
- Update README and technique documentation from verified behavior.
- Record deferred work such as multi-bounce DDGI, moving geometry, cascaded volumes, LOD, or a render
  graph as separate follow-up issues, not hidden TODO comments.

### Final Gate

- All definitions of done in both detailed plans pass.
- Clean configure/build/test instructions work in CI and on the reference development environment.
- Three or more maintained samples prove the common framework.
- A new contributor can follow the documented startup, ownership, frame, pass, and shutdown flow,
  and can add a minimal sample by following the verified extension guide.
- Sponza has reproducible assets, measured loading, correct render behavior, and stable DDGI.
- Default build and documentation contain no references to missing executables, dependencies, or
  assets.

## Suggested Pull Request Sequence

1. Audit document, asset manifest, CI fixture, and benchmark schema
2. CMake targets, dependency manifest, presets, minimal CI, and architecture skeleton
3. Application/sample registry, adding-a-sample guide, and ray-marching migration
4. Input, camera, timing, resize, GL debug, and asset-path services
5. Move-only shader and basic GPU resource wrappers
6. Framebuffer/render-target wrappers and framebuffer sample migration
7. CPU glTF importer plus fixtures
8. GPU scene upload and pass-specific draw lists
9. Sponza migration with preserved baseline
10. Sponza P0/P1 correctness fixes and visual baselines
11. Startup telemetry, staging release, and IBL cache
12. Processed Sponza profiles and asynchronous CPU loading
13. Draw packing, culling, sorting, and measured frame optimizations
14. Clustered deferred migration and compute/SSBO test coverage
15. DDGI BVH/ray capability spike
16. Probe storage, debug views, and single-bounce integration
17. DDGI visibility, classification, relocation, and presets
18. Remaining sample decisions, legacy removal, CI completion, and final report

Pull requests 12 and 14 may proceed in parallel after their shared APIs in pull requests 5 through 8
are stable. DDGI pull requests must remain ordered.

## Stop and Reassess Conditions

Pause the current approach and write a short decision record if any of these occur:

- Restoring the selected Sponza asset is legally or operationally impractical.
- Processed assets cause visible quality loss outside agreed tolerances.
- Async loading requires OpenGL calls outside the render thread.
- The new renderer cannot reproduce the fixed-camera Sponza baseline before intentional fixes.
- Software BVH traversal cannot achieve a useful probe update rate within the measured budget.
- A proposed abstraction needs more code in migrated samples than the explicit implementation it
  replaces.

## Progress Tracking

Track status and independent verification evidence in
[`04-phase-goals-and-verification.md`](04-phase-goals-and-verification.md). A phase is complete only
when its exit gate and that tracker's evidence protocol pass. Keep benchmark JSON, numeric rendering
readbacks, screenshots, test commands, and design decisions attached to the relevant phase or pull
request so later performance and rendering claims remain reproducible; screenshots are supporting,
not standalone, correctness evidence.
