# Sponza Performance, Correctness, and DDGI Plan

## Purpose

Make Sponza a reliable, measurable sample, reduce its startup and frame costs, fix known rendering
and glTF correctness defects, and add Dynamic Diffuse Global Illumination without coupling the
feature to one model.

This plan assumes the application, resource ownership, and CPU/GPU scene boundaries from
[`01-codebase-refactor-plan.md`](01-codebase-refactor-plan.md). The integrated order is defined in
[`03-master-execution-plan.md`](03-master-execution-plan.md).

## Verified Constraints

- The expected file now exists locally at
  `resources/main-sponza/main_sponza/NewSponza_Main_glTF_003.gltf`, but the entire
  `resources/main-sponza/` directory is untracked. Phase 0 added a pinned asset manifest,
  checksum-gated fetch/install/verify workflow, license evidence, and a CC0 CI fixture; a clean
  checkout can validate the fixture and can reproduce full Sponza when the external archive is
  available.
- The restored archive is 6.3 GiB because it also contains FBX, USD, MAX, renders, and duplicate
  texture variants. The glTF references one 139,973,456-byte buffer and 72 unique 4096x4096 PNGs;
  all references exist and total 2,171,138,891 bytes (about 2.1 GiB).
- The glTF contains one scene, 155 nodes, 115 meshes, 405 indexed triangle primitives, 2,049,137
  primitive vertex references, 28 materials, 72 textures, and 24 punctual lights. Its buffer's
  actual size matches the declared byte length. The glTF and buffer SHA-256 values are
  `e04c4c540c74bdddcbd3f590a85c14119bfd5839702246a23b3a737c0cce2400` and
  `903d443b46b51c8397c25b65286d9f9e111121f12a3e0e303f19019982568d6b` respectively.
- The Phase 0 five-run baseline records a 40.756-second median first frame, 38.787 seconds in the
  combined glTF load/upload span, 10,536,452,096-byte median peak RSS, and a 5,131,527,552-byte
  retained CPU lower bound. Every run retains 600 CPU and GPU frame samples plus numeric attachment
  readbacks. The accepted runs declare `cold_cache=false`, so Phase 6 still needs controlled
  cold/warm methodology for optimization claims.
- [`docs/sponza_scene.md`](../sponza_scene.md) reports roughly 78 seconds of loading. That number
  is not directly comparable because no reference machine, cache state, asset revision, or timing
  boundary is documented. Treat it as a historical report, not a current benchmark.
- Phase 0 adds a temporary `legacy_sponza` build target so baseline behavior is compiled and tested.
  The common multi-sample launcher and engine-library target graph remain Phase 1 work.
- OpenGL 4.3 is the intended baseline. It has compute shaders, SSBOs, image load/store, and timer
  queries, but no hardware ray-query API.

## Issue Register

Priority meanings:

- P0: blocks reliable development or is memory-unsafe.
- P1: causes incorrect output, severe fragility, or major wasted work.
- P2: maintainability or performance debt that should be measured and scheduled.

| Priority | Finding | Evidence | Required action |
|---|---|---|---|
| P0 | Sponza requires a 6.3 GiB external asset. | Phase 0 manifest and verification record | Phase 0 added a pinned fetch/install/verify path and CI fixture. Phase 6 still owns processed `dev`/`full` profiles. |
| P0 | The original SSAO noise upload read beyond its source vector. | Phase 0 audit and baseline diff | Phase 0 replaced it with 25 deterministic `vec4` values. Phase 5 still owns the clear SSAO output. |
| P0 | The common launcher does not yet compile and register maintained samples. | `CMakeLists.txt`; dormant source audit | Phase 0 added only a temporary baseline target. Phase 1 must create the maintained target graph and launcher. |
| P1 | Configure previously copied the entire `resources/` directory. | Phase 0 CMake diff and build logs | Phase 0 removed the copy. Phase 1 must establish the final external asset-root contract. |
| P1 | Loading is fully synchronous: ASCII glTF parse and image decode, serial texture upload, mip generation, mesh repacking, and GPU buffer creation all block startup. | `include/load_gltf.h:104-151`; local run measured 39.390 seconds in the loader | Split import/decode from render-thread upload, instrument each stage, then pipeline work. |
| P1 | TinyGLTF retains source buffers and decoded images while the loader also retains copied CPU vertices and indices after GPU upload. | Public `model`, `meshes`, and `textures` in `include/load_gltf.h:90-97`; timed process peaked at 9.83 GiB RSS | Release staging payloads after upload or retain only an explicit CPU-copy option. |
| P1 | Every primitive is expanded through several temporary attribute vectors, copied into an interleaved vector, and allocated as its own VAO/VBO/EBO. | `include/load_gltf.h:257-477` | Validate accessors, reserve storage, build one staging allocation, and pack primitives into shared buffers. |
| P1 | Runtime IBL conversion, irradiance convolution, specular prefiltering, and BRDF LUT generation execute at every startup. | `src/sponza.cpp:339-460` | Precompute/cache environment products keyed by source HDR and settings. |
| P1 | `GLTFLoader::Render` binds all material textures and sets material uniforms during the G-buffer and depth-only passes even when their shaders do not use them. The scene is traversed three times per frame. | `src/sponza.cpp:538-545,603-608,618-651`; `include/load_gltf.h:775-913` | Build pass-specific draw lists and bind only required resources. |
| P1 | Offscreen G-buffer and SSAO targets are fixed at 800x600 while only the viewport changes on resize. SSAO noise scale is also fixed to the initial size. | `src/sponza.cpp:232-312,536,564,975-981` | Recreate size-dependent attachments and derive all screen values from framebuffer extent. |
| P1 | Final PBR shading samples screen-space SSAO using the mesh's secondary material UV. | `resources/sponza/modelSponza.fs:204` | Pass screen position or use a deferred/composite input so SSAO is sampled in screen space. |
| P1 | Scene transforms are incomplete: node `matrix` is ignored; lights use local transforms; cached `worldTransform` is unused; every glTF scene is rendered instead of the selected/default scene. | `include/load_gltf.h:694-772,919-1019` | Implement full glTF transform rules, select one scene, and flatten world transforms once. |
| P1 | Accessor decoding assumes float attributes, ignores sparse accessors and several validation rules, and assumes tightly packed index data. Non-indexed primitives draw nothing. | `include/load_gltf.h:257-367,408-476,902-903` | Add typed/normalized/strided/sparse accessor decoding and `glDrawArrays` support, with fixtures. |
| P1 | glTF sampler wrap values are computed but ignored; every texture is forced to repeat. | `include/load_gltf.h:526-542` | Use separate sampler objects and honor min/mag/wrap values. |
| P1 | Material handling changes glTF defaults, omits emissive and alpha cutoff, does not sort transparent draws, and does not honor double-sided materials. | `include/load_gltf.h:562-604,790-907` | Preserve specification values and build opaque, mask, and sorted blend queues. |
| P1 | No Sponza GPU resources are explicitly released before the context is destroyed. | `src/sponza.cpp:689-693`; no loader destructor | Move ownership into RAII types and assert zero live objects at sample shutdown. |
| P1 | Shader file/compile/link failures log and continue with an invalid program. Compute shader objects are not deleted after linking. | `src/shader.cpp:12-183,253-274` | Return structured initialization errors and make `ShaderProgram` move-only RAII. |
| P2 | Uniform locations are queried repeatedly per draw and uniform names are dynamically constructed for lights and SSAO samples. | `src/shader.cpp:189-250`; loader render/setup methods | Cache locations and move frame/material data to UBOs or SSBOs after profiling. |
| P2 | No frustum culling, material sorting, multi-draw, LOD, or occlusion culling is used for Sponza. | Whole Sponza draw path | Add in that order, stopping when frame budgets are met. |
| P2 | Global GL blend/depth/cull state is modified ad hoc; culling is disabled at initialization but shadow code still changes cull mode. | `src/sponza.cpp:145-151,603-609` | Add scoped pass state and explicit opaque/masked/transparent policies. |
| P2 | Documentation claims binary glTF, cascaded shadows, emissive materials, and up to 24 lights, while code loads only ASCII, uses one shadow map, omits emissive shading, and configures one directional light. | `docs/sponza_scene.md`; loader and shaders | Rewrite documentation from tested capabilities and mark planned features honestly. |

## Measurement Comes First

Add an opt-in benchmark mode before optimizing:

```text
render_samples --sample sponza \
  --asset-profile dev \
  --benchmark startup \
  --benchmark-output results/sponza-startup.json
```

Record environment metadata:

- git revision and dirty state
- build type and compiler
- CPU, RAM, GPU, driver, and OpenGL version
- asset manifest version/profile and source checksum
- cache state: disabled, cold, or warm
- window extent and renderer settings

Record startup spans using `steady_clock`:

1. manifest resolution and file open
2. JSON/GLB parsing and source buffer IO
3. image IO and decode, by image and total
4. accessor validation and CPU mesh conversion
5. material/node/light processing
6. GPU buffer allocation/upload
7. texture allocation/upload
8. mip generation
9. IBL cache lookup or precompute
10. shader compilation/linking
11. total to first responsive window
12. total to first fully rendered frame

Record bytes and counts: source bytes, decoded image bytes, retained CPU bytes, GPU bytes, meshes,
primitives, vertices, indices, images, textures, samplers, materials, nodes, draw calls, and shader
programs. Use GL timer queries for GPU upload/precompute/pass timing. In benchmark mode only, an
explicit `glFinish()` may delimit startup GPU work; never use it in the normal frame loop.

## Provisional Budgets

Confirm or revise these after the first reproducible baseline on a named reference machine:

| Metric | Development asset | Full-quality asset |
|---|---:|---:|
| Cold first fully rendered frame | <= 5 s | <= 30 s and at least 2x faster than baseline |
| Warm first fully rendered frame | <= 2 s | <= 10 s |
| Main-thread unresponsive interval | <= 100 ms after window creation | <= 100 ms |
| Retained CPU staging after upload | <= 10% of source + decoded payload | <= 10% |
| GL errors during load/render/shutdown | 0 | 0 |
| DDGI GPU budget at 1080p, medium preset | <= 4 ms | <= 4 ms |

The loader should expose progress and remain responsive even when the full-quality cold-load target
cannot be met on slower storage.

## Asset Pipeline

### Reproducible inputs

Add an asset manifest with:

- canonical model name/version and upstream source URL
- expected archive and key-file SHA-256 values
- license and attribution path
- unpacked logical root
- conversion tool versions and arguments
- profiles: `ci`, `dev`, and `full`

`ci` is a tiny purpose-built scene. `dev` limits texture resolution and geometry while preserving
all important material types and scene hierarchy. `full` preserves production inspection quality.

### Offline processing

Build deterministic processed artifacts rather than paying every cost at launch:

1. Convert the source scene to GLB or an equivalent bounded-file layout.
2. Generate precomputed mip chains.
3. Transcode color textures and data/normal textures to appropriate GPU-compressed KTX2 formats
   supported by the reference platform. Keep color-space metadata explicit.
4. Generate a material/image/sampler manifest and scene bounds.
5. Optionally optimize index/vertex order and quantization after a visual quality comparison.
6. Precompute environment cubemap, diffuse irradiance, specular prefilter mips, and BRDF LUT into a
   cache artifact.
7. Cache a packed scene representation keyed by source hashes, converter version, vertex layout,
   and renderer cache version.

Do not introduce Draco or mesh compression merely because the dependency is present. Compare parse,
decode, upload, file size, and retained memory against uncompressed processed GLB first.

## Loader and Upload Design

### CPU import result

`GltfImporter::import()` returns `Result<ImportedScene, ImportError>` and performs no OpenGL calls.
It must:

- support `.gltf` and `.glb`
- validate all indices and byte ranges before access
- handle accessor component type, normalization, stride, sparse data, and non-indexed primitives
- honor node matrix or TRS according to glTF rules
- resolve only the chosen/default scene and calculate world transforms and bounds once
- preserve glTF PBR factors, texture coordinate set, alpha mode/cutoff, double-sided state, emissive,
  normal scale, occlusion strength, and punctual-light properties
- report unsupported extensions without silently changing semantics

### Asynchronous staging

After the synchronous importer is correct:

1. Parse source metadata and read/decode independent images on a bounded worker pool.
2. Produce immutable staging batches with progress counters and cancellation.
3. Queue bounded GPU uploads on the render thread. Do not create a shared OpenGL context for the
   first implementation.
4. Show the window immediately with a loading/progress view, then atomically publish `GpuScene`
   only after required resources are ready.
5. Release source buffers, decoded images, and CPU mesh staging as soon as their uploads complete,
   unless `--retain-cpu-scene` is explicitly enabled for diagnostics.

### GPU scene layout

- Pack compatible primitives into shared vertex and index buffers.
- Keep immutable draw records with offsets, counts, bounds, material ID, and transform ID.
- Store per-frame transforms in an SSBO or UBO and materials in a stable GPU table.
- Deduplicate image storage; represent different glTF sampler choices with OpenGL sampler objects.
- Sort opaque/masked draws by pipeline and material. Sort transparent draws back-to-front.
- Build frustum-culled render views. Add multi-draw indirect only after draw-call timing shows value.

## Correctness Work Before DDGI

DDGI will amplify incorrect normals, color spaces, transforms, and depth. Complete these gates first:

1. Fix the SSAO noise buffer overread and make all screen-space targets resize-safe.
2. Sample SSAO in screen coordinates, not material UVs.
3. Establish one linear HDR lighting buffer. Decode sRGB base/emissive textures in hardware, keep
   normal/metallic/roughness/AO linear, and tone-map/gamma-encode once at final output.
4. Validate tangent handedness and fall back to derivative-based TBN or generated tangents when
   tangents are absent. Do not use a zero tangent in the PBR shader.
5. Implement selected scene and full hierarchy transforms for meshes and lights.
6. Separate opaque, alpha-mask, and alpha-blend passes; ensure depth/shadow passes use matching
   alpha-mask logic.
7. Replace fixed scene/shadow bounds with imported world bounds and a stable fitted light volume.
8. Add fixed-camera image baselines for direct-only, IBL-only, SSAO-only, shadow-only, and combined
   output.

## DDGI Scope

Version 1 targets dynamic direct lighting over static Sponza geometry. Moving lights and light color
changes update indirect diffuse lighting over time. Skinned or moving occluder support is deferred;
it requires BVH refit/rebuild or another acceleration structure.

Because OpenGL 4.3 has no hardware ray queries, use a probe-based DDGI implementation with a
software-traversed static triangle BVH in compute shaders. This is an engine-specific DDGI feature,
not a claim of compatibility or performance parity with vendor RTXGI implementations.

## DDGI Data and Passes

### Acceleration structure

- Build a static SAH BVH from world-space triangles during offline processing or first-load cache
  generation.
- Upload compact BVH nodes, triangle positions/indices, and the minimal hit-material table to
  SSBOs.
- Use iterative stack traversal with a bounded depth and record overflow counters.
- Validate GPU hit distances/normals against a CPU reference on a small fixture.

### Probe volume

`DdgiVolume` owns:

- origin, probe counts, and spacing
- per-probe state: active, inactive, needs relocation, update age
- ping-pong irradiance atlases using octahedral directional mapping
- ping-pong distance/moment atlases for visibility weighting
- update cursor, hysteresis, normal/view bias, and quality preset

Start with one bounded volume fitted to Sponza. Cascaded or scrolling volumes are later features.

### Frame passes

1. `DdgiTracePass`: update a configurable subset of probes. Rotate a low-discrepancy ray set each
   frame, traverse the BVH, and evaluate direct radiance plus emissive at hits. Use environment
   radiance on misses.
2. `DdgiBlendPass`: temporally blend irradiance and distance moments into the atlases. Clamp abrupt
   changes and expose hysteresis.
3. `DdgiClassifyPass`: mark probes inside geometry or far from useful space. Add this after the base
   path is validated.
4. `DdgiRelocatePass`: move invalid probes within a limited radius using distance information. Add
   after classification.
5. `ForwardPbrPass` or lighting composite: sample the eight surrounding probes, apply trilinear,
   normal, and moment-based visibility weights, and add only diffuse indirect energy.

The first milestone is single-bounce diffuse GI. Multi-bounce feedback may sample the previous
irradiance atlas at ray hits only after energy stability tests exist.

### Required debug views

- probe positions colored by state or update age
- irradiance and distance atlas inspection
- rays and hit points for one selected probe
- indirect-only, direct-only, and combined lighting
- probe weight/visibility at the selected pixel
- BVH traversal count and overflow heat map
- per-pass GPU time, probes updated, rays traced, and history reset count

## DDGI Milestones

### D0: Capability spike

- Trace rays against a small static fixture in a compute shader.
- Compare hits with CPU reference results.
- Measure rays/ms on the reference GPU and estimate the Sponza update budget.

Exit gate: zero reference mismatches within tolerance, zero traversal overflows, and a documented
quality/update preset that fits the provisional 4 ms budget. If it cannot fit, evaluate a coarse
voxel/DDA fallback before changing graphics APIs.

### D1: Probe storage and visualization

- Create the bounded probe grid and atlas addressing.
- Render probe states and atlas debug views.
- Add deterministic ray rotation and history reset.

Exit gate: all probes map to unique valid texels, resize/reload is leak-free, and debug views match
CPU-generated addressing fixtures.

### D2: Single-bounce diffuse GI

- Trace direct-lit hit radiance and environment misses.
- Blend irradiance/distance history and sample eight probes in PBR.
- Add quality presets controlling volume density, rays per update, and probes per frame.

Exit gate: a Cornell-box-style fixture shows the expected color transfer, Sponza indirect-only view
contains no NaN/Inf values, and medium preset meets the measured GPU budget.

### D3: Stability and leak reduction

- Add moment-based visibility, normal/view bias, classification, and relocation.
- Reset or reduce hysteresis for meaningful light/scene changes.
- Test camera motion, abrupt light changes, and long static runs.

Exit gate: no persistent bright energy growth, no severe wall leaks in the agreed fixed camera set,
and lighting converges after a documented number of frames.

### D4: Optional advanced behavior

- Multi-bounce feedback with energy clamps.
- Multiple/cascaded volumes.
- Static BVH cache compression.
- Dynamic geometry refit or a separate dynamic-occluder representation.

These are not required for the first usable DDGI sample.

## Benchmark and Regression Matrix

Run these combinations on the named reference machine:

| Area | Cases |
|---|---|
| Startup | `ci/dev/full` x cache `off/cold/warm` x Debug/Release |
| Window | 800x600, 1920x1080, resize sequence, minimized/restored |
| Rendering | direct, shadows, IBL, SSAO, DDGI individually and combined |
| DDGI | low/medium/high, static light, moving light, abrupt color/intensity change |
| Assets | valid scene plus each malformed/edge-case glTF fixture |
| Lifetime | load/unload Sponza 20 times in one process; sample switch where supported |

Store JSON summaries as CI artifacts. Do not commit machine-specific benchmark results as universal
claims; commit only the schema, reference configuration, selected baselines, and allowed thresholds.

## Definition of Done

- A clean checkout can fetch and verify the selected Sponza version or run the small fixture without
  external data.
- Startup benchmark output separates all CPU and GPU stages and reports memory/count metrics.
- The P0/P1 issue register is resolved or explicitly superseded with a tested design.
- Sponza load is responsive and meets the agreed development/full asset budgets.
- All Sponza render targets resize correctly and fixed-camera visual baselines pass.
- DDGI can be enabled/disabled at runtime without restarting or leaking resources.
- Dynamic light changes produce temporally converging indirect diffuse light over static geometry.
- The medium DDGI preset meets its agreed frame budget with no GL errors, NaN/Inf output, BVH stack
  overflow, or unbounded energy growth.
- Documentation states the actual supported asset formats, material features, light count, shadow
  method, DDGI limitations, controls, and benchmark environment.
