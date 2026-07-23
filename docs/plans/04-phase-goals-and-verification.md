# Phase Goals and Verification

## Purpose

This file is the authoritative completion tracker for the multi-sample refactor described in
[`03-master-execution-plan.md`](03-master-execution-plan.md). It turns each phase into a falsifiable
goal with required evidence and an independent verification gate. A phase is not complete because
its implementation looks plausible, compiles once, or produces a convincing screenshot.

## Status

| Phase | Goal summary | Status | Implementer | Independent verifier | Evidence record |
|---|---|---|---|---|---|
| 0 | Establish reproducible inputs and baselines | `complete` | `/root/phase0_artifacts`, `/root/legacy_baseline_design` | `/root`, `/root/phase0_artifact_verifier`, `/root/phase0_final_verifier` | [`docs/verification/phase-0.md`](../verification/phase-0.md) |
| 1 | Establish the build and sample launcher | `in_progress` | `/root/phase1_build_launcher`, `/root` | `/root`, `/root/phase1_acceptance_design` | [`docs/verification/phase-1.md`](../verification/phase-1.md); code-controlled gates pass, commit/CI evidence pending |
| 2 | Centralize application/runtime behavior | `not_started` | Unassigned | Unassigned | Not yet created |
| 3 | Make GPU ownership and render targets explicit | `not_started` | Unassigned | Unassigned | Not yet created |
| 4 | Separate scene import, upload, and drawing | `not_started` | Unassigned | Unassigned | Not yet created |
| 5 | Migrate and correct Sponza | `not_started` | Unassigned | Unassigned | Not yet created |
| 6 | Optimize measured Sponza loading and rendering | `not_started` | Unassigned | Unassigned | Not yet created |
| 7 | Prove the architecture with multiple samples | `not_started` | Unassigned | Unassigned | Not yet created |
| 8 | Build a reusable and validated DDGI capability | `not_started` | Unassigned | Unassigned | Not yet created |
| 9 | Integrate stable single-bounce DDGI | `not_started` | Unassigned | Unassigned | Not yet created |
| 10 | Consolidate, document, and remove legacy paths | `not_started` | Unassigned | Unassigned | Not yet created |

Only this table records phase status. Detailed plans describe scope but do not imply completion.

## Verification Protocol

### Roles

- The **implementer** owns code and documentation changes for a phase.
- The **independent verifier** must be a different agent or person and must not author implementation
  changes for the phase being verified. The verifier may add or repair verification-only tooling,
  but any resulting implementation fix returns the phase to the implementer and requires a fresh
  verification run.
- The root coordinator assigns both roles, reviews evidence coverage, and is the only role that
  changes a phase to `complete`.
- A later phase may be explored early, but it cannot be marked `in_progress` until its listed
  dependencies are complete or an approved decision record explains the safe overlap.

### Required evidence record

Create one checked-in record at `docs/verification/phase-N.md` for every phase. Each record must
contain:

1. Git revision and a statement of whether the worktree was clean.
2. UTC timestamp, OS, compiler, CMake, GPU, driver, build type, and relevant asset profile.
3. Exact commands and their exit codes.
4. Links to raw test, benchmark, GL-debug, and validation artifacts under
   `artifacts/verification/phase-N/`.
5. A requirement-by-requirement result of `pass`, `fail`, or `not_run`; absence is not a pass.
6. Implementer name, verifier name, and verifier sign-off.
7. Any approved exception, including its decision record, owner, expiry/removal condition, and
   regression coverage.

Generated artifacts do not need to live in Git when their size makes that impractical, but their
schema, checksum, durable storage location, and reproduction command must be in the evidence
record.

### Universal exit rules

A phase can move to `complete` only when all of these are true:

- Every acceptance check in that phase passes at the same revision.
- Every dependency is `complete`.
- The independent verifier reruns the checks from a clean build directory. Reusing implementer
  output is supporting evidence only.
- Tests exercise the new behavior and include at least one relevant failure or negative path.
- Sanitizer, GL-debug, readback, timing, or memory output is retained where the phase requires it.
- Screenshots are supplementary evidence only. Rendering claims require deterministic fixtures,
  GPU buffer or texture readback, numeric comparisons, invariants, and feature ablation.
- New public APIs document purpose, ownership, lifetime, failure behavior, and thread restrictions.
  Render passes document inputs, outputs, formats, bindings, state assumptions, and barriers.
  Complex algorithms explain invariants and references; routine code relies on clear names and
  focused functions instead of line-by-line narration.
- `docs/architecture.md` and `docs/adding-a-sample.md` match any architecture or extension change.
- There are no unexplained test skips, GL debug errors, sanitizer findings, NaN/Inf values, or live
  GPU objects relevant to the phase.

### Parameters that must be fixed before measurement

Phase 0 must create `validation/acceptance.json`. It binds the reference fixture revisions, camera
states, resolutions, warm-up frames, sample counts, numeric tolerances, benchmark repetitions, and
performance budgets used below. A parameterized check means the named value is read from this file,
not selected after observing a candidate result. Threshold changes require a decision record and
rerunning every affected earlier baseline and later verification.

At minimum, the file must define:

- `numeric.absolute_tolerance`, `numeric.relative_tolerance`, and per-buffer overrides;
- `image_readback.max_bad_pixel_fraction` for numeric renderer comparisons;
- `timing.repetitions`, `timing.cold_start_budget_ms`, `timing.warm_start_budget_ms`, and per-pass
  GPU budgets;
- `memory.peak_rss_budget_bytes` and `memory.retained_staging_budget_bytes`;
- `ddgi.ray_hit_agreement`, `ddgi.hit_distance_tolerance`, `ddgi.irradiance_tolerance`,
  `ddgi.convergence_frame_count`, `ddgi.energy_bound`, and the low/medium/high update budgets.

Angle-bracketed values in command blocks are required parameters, not permission to improvise a
check. Uppercase values such as `<EMPTY_ASSET_ROOT>` are run-specific paths or outputs.
Lowercase command values such as `<asset-fetch-command>` must be bound to an exact repository
executable and argument prefix in the phase evidence record before implementation starts. The
verifier records the fully expanded command and must reject a phase whose command binding omits
behavior named by the acceptance check.

## Phase 0: Baseline and Inputs

**Status:** `complete`

**Goal:** Produce reproducible asset inputs, fixtures, schemas, and measurements against which later
correctness and performance claims can be tested.

**Roles:** The baseline implementer creates manifests, fixtures, tools, schemas, and captures. An
independent verifier fetches/verifies assets in a separate directory, validates schemas, and repeats
the documented baseline commands.

**Required evidence:**

- Current-state audit with source/build findings and explicit known failures.
- Sponza manifest containing provenance, license, profile contents, byte sizes, and SHA-256 values.
- Fetch/verify logs from an empty asset directory.
- A small redistribution-safe glTF fixture with asserted scene/material/light properties.
- Versioned benchmark and validation schemas plus `validation/acceptance.json`.
- Baseline JSON for ray marching and Sponza containing revision, environment, stage timings,
  first-frame time, peak RSS, retained staging bytes, frame timings, and GL-debug counts.
- Fixed-camera settings and numeric framebuffer attachment readbacks; screenshots may accompany
  them but cannot replace them.

**Acceptance checks:**

```text
cmake -S . -B build/phase-0 -DCMAKE_BUILD_TYPE=Release
cmake --build build/phase-0 --parallel
ctest --test-dir build/phase-0 --output-on-failure -R "asset|fixture|schema|baseline"
<asset-fetch-command> --manifest assets/manifest.json --root <EMPTY_ASSET_ROOT>
<asset-verify-command> --manifest assets/manifest.json --root <EMPTY_ASSET_ROOT>
<baseline-command> --sample ray-marching --config validation/acceptance.json --output <RAY_JSON>
<baseline-command> --sample sponza --asset-profile full \
  --config validation/acceptance.json --output <SPONZA_JSON>
<schema-check-command> validation/schemas/benchmark.schema.json <RAY_JSON> <SPONZA_JSON>
```

The fetch and verify commands must return zero and every fetched file must match its manifest
checksum. Both baseline JSON files must validate, contain all required environment fields, report
zero non-allowlisted GL errors, and contain finite numeric values. The historical 78-second claim
must be labeled either verified with matching evidence or unverified.

## Phase 1: Build Foundation

**Status:** `in_progress`

**Depends on:** Phase 0

**Goal:** A clean checkout builds documented engine-library targets and one launcher that discovers
and runs samples without copying the complete asset archive into the build tree.

**Roles:** The build implementer owns CMake, dependency pinning, launcher/registry scaffolding, and
initial guides. An independent verifier executes the commands in a fresh checkout or CI workspace
with an empty build directory.

**Required evidence:**

- CMake target graph for `engine_platform`, `engine_render`, `engine_assets`, `engine_scene`, and
  `render_samples`.
- Pinned dependency manifest, configure/build logs, CTest XML, and CI job URL/log.
- Launcher CLI tests including unknown sample, missing argument, invalid dimensions, and missing
  asset root.
- Architecture and adding-a-sample documents with initial public API contracts.

**Acceptance checks:**

```text
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
build/dev/render_samples --list-samples
build/dev/render_samples --sample ray-marching --frames 10
cmake --build --preset dev --target engine_platform engine_render engine_assets engine_scene
```

The listed sample must run for exactly 10 frames and exit zero. CTest must include successful CLI
negative tests. Configure/build must succeed in CI from a clean checkout. The build tree must not
contain a copied Sponza archive, and source/asset discovery must not depend on the shell's working
directory.

## Phase 2: Shared Runtime

**Status:** `not_started`

**Depends on:** Phase 1

**Goal:** Two samples use one documented application runtime for context, input, camera, timing,
resize, asset paths, and shutdown, with no sample-owned platform lifecycle.

**Roles:** The runtime implementer migrates services and two samples. An independent verifier owns
structural checks and automated interaction smoke runs from both repository and build directories.

**Required evidence:**

- Runtime unit tests for input transitions, camera directions, delta clamping, resize delivery, and
  asset-root precedence.
- Automated resize, minimize/restore, focus, and mouse-capture event logs.
- GL-debug JSON per sample and structural scan proving sample code does not initialize GLFW or own
  global input/timing state.
- Updated startup, frame, resize, and shutdown documentation.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "runtime|input|camera|asset_path"
<runtime-smoke-command> --sample ray-marching --event-script validation/runtime-events.json
<runtime-smoke-command> --sample <SECOND_SAMPLE> --event-script validation/runtime-events.json
<runtime-boundary-check-command> samples/
```

Both smoke runs must complete the configured event script from repository root and build directory,
report the expected framebuffer dimensions after every resize/restore, clamp injected long frame
deltas to the configured maximum, and report zero GL-debug errors. The boundary check must reject
direct GLFW lifecycle calls and mutable global timing/input state in migrated samples.

## Phase 3: GPU Ownership and Render Targets

**Status:** `not_started`

**Depends on:** Phase 2

**Goal:** Migrated rendering code owns GL resources through move-only, labeled, leak-counted
wrappers, and framebuffer resources resize transactionally.

**Roles:** The GPU-resource implementer builds wrappers and migrates one framebuffer sample. An
independent verifier runs compile-time ownership assertions, deliberate failure tests, repeated
lifecycle tests, and GL-debug inspection.

**Required evidence:**

- Compile-time copy/move trait tests for every wrapper.
- Invalid shader, missing shader file, link failure, and incomplete framebuffer test logs.
- Twenty-cycle create/run/destroy telemetry with per-resource live counts.
- Resize readback proving attachment dimensions and formats after every configured size.
- Public ownership/lifetime/thread contracts and render-pass binding/state comments.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "gpu_resource|shader_failure|framebuffer"
<sample-cycle-command> --sample <FRAMEBUFFER_SAMPLE> --cycles 20 \
  --event-script validation/runtime-events.json --output <LIFECYCLE_JSON>
<gpu-evidence-check-command> <LIFECYCLE_JSON>
```

Copy construction and copy assignment must be rejected for all owning wrappers; moves must transfer
the handle and leave the source empty. Every cycle must end with zero live wrapper-owned handles
and zero GL-debug errors. Invalid resources must fail initialization with a resource name and driver
log. Read-back attachment width, height, internal format, completeness, and viewport must equal the
scripted expected values after each resize.

## Phase 4: Scene and Asset Pipeline

**Status:** `not_started`

**Depends on:** Phase 3

**Goal:** glTF parsing is a bounds-checked CPU operation independent of OpenGL, while upload creates
an owned GPU scene and passes consume explicit draw records.

**Roles:** The scene-pipeline implementer owns importer, scene data, upload, and draw-list APIs. An
independent verifier runs fixture conformance, malformed-input, upload/readback, and staging-release
tests.

**Required evidence:**

- Fixture expectations for counts, bounds, hierarchy, matrix/TRS, normalized/strided/sparse
  accessors, indexed/non-indexed primitives, materials, UV selection, alpha, samplers, and lights.
- Malformed and out-of-range glTF corpus with expected typed failures.
- CPU-only importer test log proving no GL context is created.
- GPU buffer/texture readback hashes or numeric arrays matched against imported fixture data.
- Memory telemetry before import, after upload, and after configurable staging release.
- Documented `ImportedScene`, `GltfImporter`, `GpuScene`, `RenderView`, and draw-list contracts.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "gltf_import|gltf_malformed|scene_bounds"
ctest --preset dev --output-on-failure -R "gpu_scene_upload|draw_list|staging_release"
<scene-conformance-command> --fixture validation/fixtures/scene.gltf \
  --expect validation/fixtures/scene.expect.json --readback <SCENE_JSON>
<scene-evidence-check-command> <SCENE_JSON> validation/acceptance.json
```

All declared fixture properties must match. Each malformed input must fail without a crash or
out-of-bounds sanitizer finding. Uploaded vertex/index/texture data must match CPU source within
configured type-specific tolerances. Releasing staging must reduce retained staging bytes to the
configured budget without changing subsequent draw readbacks. Depth, G-buffer, and PBR tests must
obtain draw records without a loader-owned render call.

## Phase 5: Sponza Migration and Correctness

**Status:** `not_started`

**Depends on:** Phase 4

**Goal:** Sponza runs through the shared sample, scene, ownership, and pass interfaces with its known
P0/P1 correctness defects resolved by automated regression coverage.

**Roles:** The Sponza implementer owns migration and correctness fixes. An independent rendering
verifier runs sanitizer/GL-debug tests and numeric attachment/readback validation, including feature
ablation. The verifier must not approve from screenshots alone.

**Required evidence:**

- Closed issue register mapping every P0/P1 defect to a test or an approved replacement.
- Address/undefined-behavior sanitizer results covering the CI asset profile and SSAO noise upload.
- Numeric readbacks for depth, world position, world normal, material channels, SSAO, shadow,
  direct, IBL, and combined outputs at fixed cameras.
- Per-pass GL state, framebuffer format/dimension, light-transform, and material interpretation
  telemetry.
- Direct-only, IBL-only, SSAO-only, shadow-only, and combined ablation report.
- Pass comments documenting inputs, outputs, bindings, color spaces, state, and ownership.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "sponza|material|transform|ssao|shadow|color_space"
<sanitizer-smoke-command> --sample sponza --asset-profile ci --frames 10
build/dev/render_samples --sample sponza --asset-profile ci --frames 10 \
  --validation-output <SPONZA_READBACK_JSON>
<render-evidence-check-command> <SPONZA_READBACK_JSON> validation/acceptance.json
<ablation-command> --sample sponza --asset-profile ci \
  --modes direct,ibl,ssao,shadow,combined --output <ABLATION_JSON>
<ablation-evidence-check-command> <ABLATION_JSON> validation/acceptance.json
```

All readbacks must be finite, normals within their configured length tolerance, depth within the
declared range, material channels within their physical ranges, and attachment sizes/formats equal
the requested framebuffer configuration. Each ablated feature must change its designated numeric
region/statistic by at least the predeclared threshold while leaving invariant buffers within their
tolerance. Full Sponza must run on the reference machine with zero GL-debug errors. Screenshot
similarity cannot override a failed numeric check.

## Phase 6: Sponza Loading and Frame Optimization

**Status:** `not_started`

**Depends on:** Phase 5

**Goal:** Reduce measured Sponza startup, retained memory, and frame costs within predeclared budgets
without changing validated rendering semantics.

**Roles:** The performance implementer owns telemetry and optimizations. An independent verifier
runs cold/warm repetitions, validates process/GPU measurements, and repeats Phase 5 numeric
rendering checks against every asset profile.

**Required evidence:**

- Machine-readable stage timing, CPU/GPU memory, object/count, worker utilization, upload queue, and
  per-pass GPU timer data for every repetition.
- Raw before/after results for `ci`, `dev`, and `full` profiles.
- Proof that CPU workers make no GL calls and the window/event loop remains responsive.
- IBL cache miss/hit tests, staging-release evidence, and asset profile checksum manifests.
- Phase 5 numeric comparisons for each profile at the same fixed cameras.

**Acceptance checks:**

```text
<benchmark-command> --sample sponza --asset-profile <ci|dev|full> \
  --mode <cold|warm> --repeat <timing.repetitions> --output <BENCHMARK_JSON>
<benchmark-evidence-check-command> <BENCHMARK_JSON> validation/acceptance.json
<responsiveness-command> --sample sponza --asset-profile full \
  --event-script validation/loading-events.json --output <RESPONSIVENESS_JSON>
<render-evidence-check-command> <PROFILE_READBACK_JSON> validation/acceptance.json
```

Median and declared tail statistics must meet the profile's cold/warm first-frame, peak RSS,
retained staging, and per-pass GPU budgets in `validation/acceptance.json`. Each benchmark must use
the declared cache-control procedure. Event latency during CPU loading must remain within its bound,
the upload queue must execute on the recorded render thread only, and all Phase 5 numeric checks
must still pass. Exceptions require a measurement-backed decision record; screenshots alone do not
establish semantic preservation.

## Phase 7: Multi-Sample Framework Completion

**Status:** `not_started`

**Depends on:** Phases 3 through 5

**Goal:** Ray marching, Sponza, and clustered deferred rendering prove the same launcher/runtime
architecture, and a new minimal sample requires no platform/runtime modification.

**Roles:** The migration implementer owns sample ports and registry/docs updates. An independent
verifier builds/runs all default samples and performs the extension exercise from the documented
guide in a disposable worktree.

**Required evidence:**

- Registry/README consistency test and smoke results for all default samples.
- Numeric output/readback smoke evidence for ray marching, Sponza, and clustered deferred.
- Compute/SSBO capability and bounds/error telemetry for clustered deferred.
- A verifier-created minimal sample patch showing its complete file list and changed-file list.
- Retirement records for every removed or merged legacy sample.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "sample_registry|sample_smoke|clustered"
build/dev/render_samples --list-samples
build/dev/render_samples --sample ray-marching --frames 10 --validation-output <RAY_JSON>
build/dev/render_samples --sample sponza --asset-profile ci --frames 10 \
  --validation-output <SPONZA_JSON>
build/dev/render_samples --sample clustered-deferred --frames 10 \
  --validation-output <CLUSTERED_JSON>
<registry-doc-check-command>
<sample-extension-check-command> --guide docs/adding-a-sample.md --worktree <DISPOSABLE_WORKTREE>
```

All three samples must be default-built, exit zero, report finite non-empty numeric output, and
report zero GL-debug/live-resource errors. The extension exercise may add only its sample directory,
sample-local assets/shaders, build source listing when not automatically discovered, and one
registry entry. It must not modify application, platform, runtime, renderer core, or unrelated
sample files.

## Phase 8: DDGI Capability

**Status:** `not_started`

**Depends on:** Phases 5 and 7

**Goal:** Provide reusable probe storage and GPU BVH ray traversal whose results agree with an
independent CPU reference and contain no Sponza-specific engine types.

**Roles:** The DDGI-capability implementer owns engine APIs, BVH upload, compute traversal, probe
storage, and debug counters. An independent verifier owns or audits the CPU reference, generates
seeded rays independently, and evaluates GPU buffer readback. Screenshot inspection is not an
acceptance method.

**Required evidence:**

- API dependency scan proving `engine_render` does not include or name Sponza types.
- Deterministic fixture triangles/rays with independently generated CPU hit results.
- GPU readback for hit/miss, primitive ID, barycentrics, hit distance, traversal depth, and overflow.
- Probe atlas addressing/read/write sentinel tests for boundaries and ping-pong history.
- Rays/ms, traversal depth distribution, overflow count, and resource-lifecycle telemetry.
- DDGI settings, ownership, bindings, barriers, reset rules, invariants, and algorithm comments.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "bvh_cpu|bvh_gpu|probe_atlas|ddgi_resource"
<ddgi-ray-check-command> --fixture validation/fixtures/ddgi-rays.json \
  --seed <FIXED_SEED> --gpu-readback <RAY_RESULTS_JSON>
<ddgi-ray-evidence-command> <RAY_RESULTS_JSON> validation/acceptance.json
<probe-atlas-check-command> --config validation/acceptance.json --output <ATLAS_RESULTS_JSON>
<dependency-boundary-check-command> engine/render samples/sponza
```

Hit/miss and primitive agreement must meet `ddgi.ray_hit_agreement`; agreed hits must meet the
configured distance and barycentric tolerances. All values must be finite, every primitive/probe
index in range, and traversal/atlas overflow counts exactly zero. Atlas sentinels must prove every
logical texel maps to one intended physical texel with no cross-probe writes. The medium update
preset must meet its measured GPU budget or an approved fallback decision must be recorded before
Phase 9 starts.

## Phase 9: DDGI Delivery

**Status:** `not_started`

**Depends on:** Phase 8

**Goal:** Deliver stable, configurable single-bounce dynamic diffuse GI through shared renderer
interfaces with numerically demonstrated light transport, temporal response, visibility, and
resource safety.

**Roles:** The DDGI-integration implementer owns probe update and PBR integration. An independent
rendering verifier runs analytic fixtures, CPU comparisons, readback invariants, temporal tests,
feature ablation, and long-duration resource checks. Screenshots are diagnostic only.

**Required evidence:**

- Analytic diffuse color-transfer fixture with CPU-expected irradiance ranges.
- Per-frame probe irradiance/distance moments and final indirect-light readbacks.
- Seeded convergence, abrupt-light-change, moving-light, camera-motion, reset, visibility,
  relocation, and classification time series.
- DDGI-off, indirect-only, direct-only, combined, and reset ablation results.
- Low/medium/high GPU timings plus long-run GL-debug, NaN/Inf, energy, and live-resource counters.
- Pass/API comments documenting units, coordinate spaces, bindings, barriers, history, and biases.

**Acceptance checks:**

```text
ctest --preset dev --output-on-failure -R "ddgi_analytic|ddgi_temporal|ddgi_visibility|ddgi_reset"
<ddgi-validation-command> --fixture validation/fixtures/ddgi-color-transfer.json \
  --config validation/acceptance.json --output <DDGI_RESULTS_JSON>
<ddgi-temporal-command> --scenario validation/fixtures/ddgi-dynamic-light.json \
  --config validation/acceptance.json --output <TEMPORAL_RESULTS_JSON>
<ddgi-long-run-command> --sample sponza --preset medium \
  --frames <LONG_RUN_FRAMES> --output <LONG_RUN_JSON>
<ddgi-evidence-check-command> <DDGI_RESULTS_JSON> <TEMPORAL_RESULTS_JSON> <LONG_RUN_JSON> \
  validation/acceptance.json
```

Fixture irradiance and distance moments must match the CPU reference within configured tolerances.
Temporal error must reach the declared convergence bound by `ddgi.convergence_frame_count`; reset
must clear history on the next update; and dynamic light changes must produce correctly signed
indirect response in the predeclared probe regions. DDGI-off must numerically remove the indirect
term, not merely change presentation. Every preset must have finite values, energy no greater than
`ddgi.energy_bound`, zero out-of-range/overflow/GL-error counts, and zero live resources after
shutdown. The medium preset must meet its 1080p GPU budget.

## Phase 10: Consolidation

**Status:** `not_started`

**Depends on:** Phases 6, 7, and 9

**Goal:** Leave one documented, CI-enforced architecture with reproducible assets, maintained
samples, stable Sponza/DDGI behavior, and no active legacy implementation path.

**Roles:** The consolidation implementer removes superseded code, closes documentation gaps, and
assembles the final matrix/report. An independent verifier starts from a clean checkout, follows
only published documentation, runs the complete matrix, and repeats the new-sample exercise.

**Required evidence:**

- CI results for configure/build, unit, importer, shader, smoke, sanitizer, numeric rendering, and
  selected performance checks.
- Final benchmark/regression report linked to raw schema-valid artifacts.
- Dependency/duplicate/dead-code scan and explicit legacy removal inventory.
- Public-header ownership, const-correctness, namespace, dependency-direction, and comment review.
- README, architecture, adding-a-sample, asset, Sponza, and DDGI documentation walkthrough results.
- Deferred work captured as owned issues or decision records rather than hidden TODO comments.

**Acceptance checks:**

```text
cmake --preset dev
cmake --build --preset dev --parallel
ctest --preset dev --output-on-failure
<full-verification-command> --config validation/acceptance.json --output <FINAL_REPORT>
<legacy-scan-command>
<registry-doc-check-command>
<sample-extension-check-command> --guide docs/adding-a-sample.md --worktree <DISPOSABLE_WORKTREE>
<schema-check-command> validation/schemas/final-report.schema.json <FINAL_REPORT>
```

All earlier phase checks must pass at the final revision, not merely be linked from old revisions.
The default build and docs must reference no missing target, dependency, asset, or executable.
Legacy loaders, duplicate entry points, and any legacy CMake option must be absent from active
build/runtime paths. At least three maintained samples must pass numeric smoke validation. A
developer unfamiliar with the repository must successfully follow the architecture and extension
guides without modifying application/platform/runtime code. No phase may retain an expired
exception.

## Status Transition Checklist

Before editing the status table:

1. Assign different named implementer and verifier roles.
2. Confirm dependencies are `complete`.
3. Link the phase evidence record and raw artifact location.
4. Confirm every acceptance result is explicitly `pass`.
5. Confirm the verifier reran the checks at the recorded revision.
6. Confirm documentation/readability review and negative-path coverage passed.
7. Change the phase to `complete` only after all six checks are evidenced.

If any item fails, keep the phase `in_progress` or mark it `blocked` with a precise blocker and owner;
never infer completion from elapsed effort or apparent visual quality.
