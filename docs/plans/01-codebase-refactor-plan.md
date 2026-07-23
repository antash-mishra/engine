# Codebase Refactor Plan

## Purpose

Turn the repository from a collection of standalone OpenGL experiments into a small,
maintainable rendering sample framework. The result must make a new sample cheap to add,
keep resource ownership explicit, and preserve the ability to study each rendering technique
without forcing every sample through one monolithic renderer.

This plan covers project structure, build organization, shared runtime code, rendering and
asset boundaries, sample migration, tests, and documentation. Sponza-specific performance,
correctness, and DDGI work is in
[`02-sponza-performance-ddgi-plan.md`](02-sponza-performance-ddgi-plan.md).

## Audit Baseline

The following facts were verified in the current checkout on 2026-07-22:

1. CMake builds only `src/ray_marching.cpp` into a target named `game_engine`.
   [`CMakeLists.txt`](../../CMakeLists.txt) does not define the Sponza or other sample targets,
   although the README documents several separate executables.
2. There are nine source files with their own `main()`, GLFW setup, input callbacks, frame
   timing, camera globals, and primitive drawing helpers. Most are 500 to 1,000 lines long.
3. `src/sponza.cpp` is 1,054 lines and `include/load_gltf.h` is a 1,023-line implementation
   header. The loader owns parsing, CPU conversion, OpenGL upload, materials, scene traversal,
   light extraction, and drawing.
4. Global types named `Vertex`, `Texture`, and `Mesh` exist independently in
   [`include/mesh.h`](../../include/mesh.h) and
   [`include/load_gltf.h`](../../include/load_gltf.h). They cannot safely coexist in a unified
   translation unit.
5. GPU objects are represented by public integer handles. Most types are copyable, have no
   destructor, and rely on optional manual `Delete()` calls. The Sponza path does not release
   its scene, framebuffer, texture, or shader resources before destroying the context.
6. Resource discovery depends on the current working directory being `build/`. Some shaders
   are loaded from `resources/...`, while other assets use the parent of `current_path()`.
7. `external/imgui` is an empty gitlink and the repository has no `.gitmodules` entry for it.
   `src/main.cpp` therefore fails to compile due to the missing `imgui.h`.
8. `src/hdr.cpp` fails to compile because it declares two local arrays named `attachments` in
   the same scope. All other `.cpp` files pass an isolated C++17 syntax check, but only the
   ray-marching target is part of the normal build.
9. The normal build succeeds, but it has no explicit C++ standard, warning policy, tests, CI,
   shader validation, or runtime smoke tests.
10. Third-party GLM, TinyGLTF, stb, JSON, and GLAD code is mixed into `include/`, which makes
    project headers difficult to distinguish from vendor headers and causes vendor warnings to
    pollute project builds.

## Goals

- Build one sample launcher with `--list-samples` and `--sample <name>`.
- Give every sample the same window, input, camera, timing, logging, asset-root, resize, and
  shutdown behavior.
- Make engine and sample code separate CMake targets with explicit dependencies.
- Make every OpenGL resource move-only and automatically destroyed while the context is alive.
- Separate CPU asset import from render-thread GPU upload and scene drawing.
- Let a render pass request only the scene data and material bindings that it needs.
- Make assets resolvable independent of the process working directory.
- Compile all maintained samples in CI and keep small non-GL logic testable without a display.
- Make the startup flow, ownership model, frame lifecycle, and render-pass order understandable to
  a contributor who has never read the repository.
- Document public extension points and non-obvious rendering code without narrating self-evident
  statements line by line.
- Provide a repeatable path for adding Sponza, clustered deferred shading, and DDGI as samples or
  optional rendering features.

## Non-Goals

- Do not build a general-purpose game engine, editor, full ECS, or scripting layer.
- Do not introduce a large render graph before the Sponza passes have clear resource contracts.
- Do not rewrite all legacy demos in one change.
- Do not change graphics APIs during the refactor. OpenGL 4.3 remains the baseline for compute
  shader samples; simpler samples may use a lower feature subset within the same context.
- Do not hide OpenGL behind an abstract Vulkan/DirectX-style backend.

## Architecture Decisions

### 1. One launcher, registered samples

Build a single executable named `render_samples`. It creates the application and OpenGL context,
then instantiates one registered sample. Optional per-sample convenience targets may call the same
launcher with a fixed argument, but they must not copy application setup.

```cpp
class ISample {
public:
    virtual ~ISample() = default;
    virtual std::string_view name() const = 0;
    virtual Result<void> initialize(SampleContext& context) = 0;
    virtual void update(const FrameInfo& frame) = 0;
    virtual void render(RenderContext& context) = 0;
    virtual void resize(Extent2D extent) = 0;
    virtual void shutdown() = 0;
};
```

`SampleContext` provides non-owning access to the device, assets, input, camera, logger, and
instrumentation. It must not expose the `Application` internals.

### 2. Explicit ownership and lifetime order

The ownership chain is:

```text
main
  -> Application
       -> Window / OpenGL context
       -> RenderDevice and shared GPU services
       -> AssetSystem and worker pool
       -> active ISample and all sample GPU resources
```

The active sample is destroyed first, then shared GPU services, then the OpenGL context. GPU
wrappers are move-only and use destructors. There are no raw owning `new` calls.

### 3. CPU scene data is independent of OpenGL

Use three explicit stages:

```text
source file -> ImportedScene (CPU/staging) -> GpuScene (render resources) -> RenderView
```

- `ImportedScene` contains validated nodes, meshes, materials, lights, image payloads, bounds, and
  diagnostics. It has no OpenGL includes.
- `GpuScene` owns buffers, textures, samplers, material tables, and immutable draw records.
- `RenderView` is a per-frame list of visible opaque, masked, and transparent draws with resolved
  transforms and material references.

This boundary permits worker-thread parsing and decoding while keeping all OpenGL calls on the
render thread.

### 4. Pass-specific scene drawing

Do not retain a universal `GLTFLoader::Render(shader)` function. It currently binds all material
textures even during depth and SSAO geometry passes. Introduce small pass interfaces instead:

- `DepthPass::execute(const RenderView&, const ShadowView&)`
- `GBufferPass::execute(const RenderView&, const CameraData&)`
- `ForwardPbrPass::execute(const RenderView&, const LightingData&)`
- `SkyPass::execute(...)`
- `PostProcessPass::execute(...)`

Start with explicit pass sequencing inside `SponzaSample`; only add a minimal render graph if
resource transitions and pass ordering become difficult after DDGI is introduced.

### 5. Stable namespaces and naming

All project code lives under `namespace engine`. Suggested nested namespaces are `app`, `render`,
`assets`, and `scene`. Sample implementations live under `namespace samples`.

Rename misspelled source and sample identifiers such as `deffered_rendering` to
`deferred_rendering` when each sample is migrated. Do not make a repository-wide rename before
the new build layout exists.

### 6. Readability and Documentation Are Part of the Design

Code is not considered migrated merely because it compiles. A new contributor must be able to
trace this path without reverse-engineering global state:

```text
main -> Application -> sample registry -> active ISample -> render passes -> GPU resources
```

Use descriptive types, focused functions, explicit data flow, and consistent file organization
before relying on comments. Add comments where they convey information the code cannot express:

- Public engine classes and extension interfaces document purpose, ownership, lifetime, thread
  restrictions, error behavior, and whether references are owning or non-owning.
- Every sample has a short overview describing its technique, initialization stages, per-frame
  pass order, required OpenGL features, assets, and supported debug modes.
- Each render pass documents its inputs, outputs, attachment formats, coordinate spaces, required
  GL state, resource bindings, and synchronization/barrier requirements.
- Shader interfaces document uniform-block, SSBO, image, sampler, attribute, and output bindings on
  both the C++ and GLSL sides. Shared binding constants have one authoritative definition.
- Complex algorithms document intent, invariants, numerical assumptions, failure modes, and a
  primary technical reference. DDGI, BVH traversal, temporal accumulation, probe relocation, PBR,
  SSAO, and shadow bias logic require this treatment.
- Workarounds and unusual constants explain why they exist, what hardware or asset constraint they
  address, and the condition under which they can be removed.
- Comments do not repeat syntax, preserve obsolete behavior, or compensate for oversized
  functions. Misleading comments fail review like incorrect code.

Add `docs/architecture.md` with module dependencies, ownership/lifetime order, frame flow, and the
CPU-import-to-GPU-upload pipeline. Add `docs/adding-a-sample.md` with a minimal working example,
registry/CMake steps, asset declaration, resize handling, fixed-frame smoke command, and expected
tests. Keep both documents updated in the same change as their public interfaces.

## Target Repository Layout

```text
CMakeLists.txt
cmake/
  CompilerWarnings.cmake
  Dependencies.cmake
  Sanitizers.cmake
engine/
  include/engine/
    app/Application.h
    app/Input.h
    app/Sample.h
    assets/AssetPath.h
    assets/AssetSystem.h
    assets/GltfImporter.h
    render/Buffer.h
    render/Framebuffer.h
    render/RenderDevice.h
    render/Sampler.h
    render/ShaderProgram.h
    render/Texture.h
    scene/GpuScene.h
    scene/ImportedScene.h
    scene/RenderView.h
  src/
samples/
  ray_marching/
    RayMarchingSample.cpp
    shaders/
  sponza/
    SponzaSample.cpp
    SponzaRenderer.cpp
    shaders/
  clustered_deferred/
tests/
  unit/
  integration/
tools/
  assets/
  benchmarks/
assets/
  common/
  fixtures/
  manifest.json
docs/
  architecture.md
  adding-a-sample.md
  plans/
```

Keep the existing `src/`, `include/`, and `resources/` directories as legacy input during the
migration. Remove them only after every retained sample has moved and its replacement is tested.

## CMake and Dependency Plan

1. Raise the minimum CMake version to a version available in CI and set C++17 explicitly.
2. Split targets:
   - `engine_platform`: GLFW application, input, timing, logging.
   - `engine_render`: GLAD, GPU wrappers, shaders, render targets, profiler.
   - `engine_assets`: asset paths, glTF importer, image decode, cache.
   - `engine_scene`: scene data, visibility, draw lists.
   - `render_samples`: launcher plus registered samples.
   - `engine_test_support`: fixtures and offscreen test helpers when available.
3. Link `OpenGL::GL`, GLFW, GLM, and TinyGLTF explicitly. Link Assimp only to a legacy importer
   target if it remains necessary. Do not make every sample depend on Assimp or Draco.
4. Adopt a versioned dependency manifest, preferably `vcpkg.json` with a pinned baseline. Remove
   the broken ImGui gitlink. If ImGui is kept, consume it as a real CMake dependency and make UI
   support optional with `ENGINE_ENABLE_IMGUI`.
5. Keep GLAD generated source as a clearly labeled vendored target under `third_party/glad/`.
   Stop treating all of `include/` as project-owned headers.
6. Add `ENGINE_BUILD_LEGACY_SAMPLES`, default `OFF`, only while migration is active.
7. Enable `CMAKE_EXPORT_COMPILE_COMMANDS`, project-only warnings, optional ASan/UBSan, and CTest.
8. Do not copy the entire resource tree at CMake configure time. Generate an asset-root definition
   and accept `--asset-root` plus `ENGINE_ASSET_ROOT`; copy only small test fixtures when needed.

## Shared Runtime Responsibilities

`Application` owns:

- GLFW initialization, window creation, GLAD loading, debug callback, and shutdown.
- Frame timing with `std::chrono::steady_clock` and a clamped simulation delta.
- Framebuffer extent and minimized-window handling.
- Input state with pressed/released edges, mouse capture, and scroll.
- Camera controller configuration supplied by the active sample.
- Sample selection, command-line parsing, and clean failure reporting.
- CPU and GPU profiler collection.

Samples own:

- Technique-specific shaders and render targets.
- Scene selection and sample-specific camera defaults.
- Pass order and technique settings.
- Debug views and sample-specific controls.

Samples must not initialize GLFW, calculate asset roots from `current_path()`, define global GPU
handles, or directly destroy the application window.

## Core Rendering Types

Implement only the wrappers that remove current ownership and validation problems:

- `ShaderProgram`: move-only; throws or returns an error on file, compile, or link failure; caches
  uniform locations; deletes compute shader objects after link; supports debug labels.
- `Buffer`: move-only; records target, size, and optional persistent mapping state.
- `VertexArray`: move-only; owns attribute layout only.
- `Texture` and `Sampler`: separate image storage from sampler state so glTF image data can be
  shared across multiple sampler configurations.
- `Framebuffer`: move-only; validates completeness and reports attachment details.
- `RenderTarget`: recreates size-dependent attachments on resize.
- `GpuTimer`: wraps timestamp or elapsed-time queries without blocking the current frame.

Avoid a generic `Handle<T>` abstraction until these concrete wrappers show meaningful duplication.

## Asset System Contract

`AssetSystem` resolves logical paths such as `sponza/scene.gltf` against this precedence:

1. `--asset-root`
2. `ENGINE_ASSET_ROOT`
3. a build-generated default based on the executable location

It validates existence before creating render resources and returns structured errors containing
the logical path, resolved path, sample name, and attempted roots. Large external assets are
declared in `assets/manifest.json` with source, version, checksum, license, and local destination.
They are not silently expected to exist.

## Migration Sequence

### R0: Freeze and characterize legacy behavior

- Record which legacy files compile, which assets exist, and which README commands are false.
- Capture a ray-marching screenshot and Sponza screenshot/performance baseline when a display and
  Sponza asset are available.
- Add a small triangle or cube fixture that can run without large external assets.

Exit gate: the repository has a written baseline and repeatable commands for every retained
sample, even if some commands are marked expected failures.

### R1: Establish the build and launcher

- Add the target split, dependency manifest, command-line parser, sample registry, and asset-root
  handling.
- Move ray marching first because it is the current build target and has minimal asset needs.
- Add `--list-samples`, `--sample ray-marching`, `--width`, `--height`, and `--frames`.

Exit gate: a clean checkout configures and builds with one documented command, ray marching runs
from both the repository root and build directory, and CTest has at least one non-GL test.

### R2: Replace global application state

- Move GLFW callbacks, input edge detection, timing, camera control, FPS reporting, and resize
  handling into `engine_platform`.
- Correct the inverted `LEFT`/`RIGHT` camera semantics and update input mapping together.
- Remove duplicated callbacks only from migrated samples.

Exit gate: two samples use the runtime with no GLFW lifecycle or global camera/time state in their
sample implementation.

### R3: Introduce GPU ownership wrappers

- Implement the concrete wrappers above and a debug OpenGL callback.
- Migrate primitive meshes, shaders, and render targets in ray marching and one framebuffer-based
  sample.
- Add lifetime tests with a mock deletion recorder where real OpenGL is not required.

Exit gate: migrated sample code contains no direct `glGen*`/`glDelete*` pairs and passes a repeated
create-run-destroy smoke test without leaked GL objects reported by instrumentation.

### R4: Separate import, upload, and scene drawing

- Move TinyGLTF implementation macros into one `.cpp` file.
- Introduce namespaced CPU scene structures, a validated glTF importer, `GpuScene`, and render
  views.
- Keep the Assimp loader as a namespaced legacy adapter until all OBJ samples are migrated or
  deliberately retired.

Exit gate: importer unit tests run without an OpenGL context, and a tiny glTF fixture imports,
uploads, renders, and releases cleanly.

### R5: Migrate maintained samples incrementally

Order:

1. Ray marching
2. Sponza
3. Clustered deferred rendering
4. PBR/IBL
5. HDR/bloom
6. Terrain
7. Remaining basic lighting/model samples, merged or retired where redundant

Each migration must add a registry entry, assets declaration, smoke command, screenshot or numeric
baseline, concise technique documentation, and comments for its pass contracts and non-obvious
algorithm choices. Do not preserve a sample solely because a source file exists; document
retirement decisions.

Exit gate: adding a basic sample requires a sample class, its shaders/assets, and one registry line,
with no edits to application/platform code.

### R6: Quality gates and legacy removal

- Add Linux CI for configure, build, unit tests, shader validation, and non-interactive smoke runs.
- Add formatting and project-only warning enforcement.
- Correct README claims and commands.
- Remove legacy directories, duplicate vendor code, stale shaders, and dead asset references after
  their replacements are verified.

Exit gate: all default targets compile in CI, no documented command points to a missing executable
or asset, and the legacy build option has been removed.

## Test Strategy

- Unit tests: asset path resolution, CLI parsing, camera movement, transforms, accessor decoding,
  material defaults, light hierarchy, cache keys, and DDGI math helpers.
- Import fixtures: indexed and non-indexed primitives, strided and normalized accessors, sparse
  accessors, matrix nodes, nested lights, alpha modes, multiple scenes, and shared images with
  distinct samplers.
- Shader checks: compile every maintained shader with `glslangValidator` or a real OpenGL smoke
  context; validate required uniforms and bindings where practical.
- Integration tests: launch each maintained sample for a fixed number of frames at a fixed extent,
  fail on GL debug errors, and write timing/counter JSON.
- Visual tests: fixed camera and settings, tolerant image comparison, plus explicit baseline update
  review. Keep fixtures small enough for CI.
- Sanitizers: run CPU import/unit tests under ASan and UBSan. GPU lifetime/error counters cover GL
  paths that sanitizers cannot see.

## Definition of Done

- `render_samples --list-samples` lists every maintained sample.
- Every maintained sample is built by the default build and has a fixed-frame smoke test.
- Application, renderer, scene, and sample ownership boundaries match this plan.
- No maintained code uses working-directory-based asset discovery or global owning GL handles.
- Size-dependent render targets resize correctly.
- Shader and asset failures stop initialization with actionable errors.
- A small glTF scene loads without an OpenGL context, then uploads through a separate render-thread
  step.
- The README matches the actual build and run commands.
- `docs/architecture.md` lets a new contributor trace startup, ownership, frame execution, scene
  import/upload, and shutdown without reading every implementation file.
- `docs/adding-a-sample.md` is verified by adding a minimal sample without modifying application or
  platform code.
- Public extension points, render passes, GPU/shader bindings, and complex algorithms satisfy the
  documentation rules above; no migrated file retains stale or misleading legacy comments.
- The Sponza and DDGI acceptance criteria in the second plan pass.

## Main Risks

- A broad rewrite can erase known-working rendering behavior. Migrate one executable at a time and
  keep visual baselines.
- Adding RAII destructors to currently copyable GL wrappers can introduce double deletion. Make
  types move-only before adding destructors.
- Async loading can accidentally move OpenGL work to threads without a current context. The CPU/GPU
  staging boundary must be in place first.
- A render graph can consume the project before it provides value. Keep explicit Sponza pass order
  until DDGI proves that automatic resource scheduling is needed.
- Full Sponza assets are too large for routine CI. Maintain a tiny fixture and a separate opt-in
  full-scene benchmark.
