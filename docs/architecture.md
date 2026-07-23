# Architecture

## Scope

The maintained application is `render_samples`. It selects one registered sample and runs it
in-process over a small set of engine libraries. The Phase 0 `game_engine` and `legacy_sponza`
executables remain opt-in compatibility targets until their behavior is migrated and verified.

## Target Graph

```text
render_samples
  -> engine_samples
       -> engine_assets
       -> engine_render
            -> engine_platform
            -> engine_scene
       -> engine_platform
       -> engine_scene
```

`engine_glm`, `engine_warnings`, and `engine_sanitizers` are build-policy interface targets.
Maintained targets do not link Assimp or Draco. Those dependencies are resolved only when
`ENGINE_BUILD_LEGACY_SAMPLES=ON`.

## Responsibilities

| Target | Owns | Must not own |
|---|---|---|
| `engine_platform` | GLFW initialization, one window/context, polling, presentation, executable path | Sample selection, shaders, asset policy |
| `engine_assets` | Asset-root precedence, canonicalization, containment, required-file validation | Asset decoding, GPU upload, process cwd assumptions |
| `engine_scene` | Renderer-independent validated camera state and matrices | GLFW, GL handles, file IO |
| `engine_render` | The maintained GLAD implementation and context-bound fullscreen render resources | Window lifetime, sample registry, asset-root selection |
| `engine_samples` | Sample factories, descriptors, registry, sample-specific orchestration | Command-line parsing, process exit policy |
| `render_samples` | Strict CLI parsing, registry lookup, root resolution, completion reporting | GPU resource implementation, sample internals |

The target graph is intentionally smaller than the final architecture. Phase 2 centralizes input,
timing, resize, and application services. Phase 3 adds move-only resource wrappers and lifecycle
telemetry. Phase 4 introduces the scene importer/upload boundary.

## Startup and Shutdown

```text
parse full CLI
  -> create registry
  -> list, or resolve one sample
  -> resolve asset root
  -> sample creates Window
  -> initialize GL entry points
  -> create sample render resources
  -> render/present bounded or interactive frames
  -> destroy render resources
  -> destroy Window/context
  -> emit one sample_complete JSON record
```

All platform and GL operations currently run on the main thread. A sample owns its render resources
but receives an immutable validated asset root. GL resources must be destroyed before `Window`,
because the context must still be current when their destructors execute. Exceptions cross the
sample boundary and are converted into one process-level error and a nonzero exit code.

## Asset Roots

Resolution is deterministic and independent of the shell's current directory:

1. `--asset-root PATH`
2. `ENGINE_ASSET_ROOT`
3. `assets/` beside the `render_samples` executable

An explicit but invalid root fails closed and never falls through. Requested files are
canonicalized and must remain below the selected root. The build stages only the two shaders used
by the registered ray-marching sample; it never copies the external Sponza archive.

## Registration

`SampleRegistry` stores validated descriptors and factories. Listing descriptors does not create a
window or GL context. Factory ownership transfers through `std::unique_ptr<Sample>`, and duplicate
or malformed descriptors fail at registry construction. See
[`adding-a-sample.md`](adding-a-sample.md) for the extension path.

## Verification Boundaries

`--frames N` uses deterministic shader time and emits exactly one JSON completion record containing
the presented-frame count. Black-box launcher tests exercise parser failures, asset-root precedence,
three working directories, fixed-frame execution, and staged-asset contents. Phase-specific
rendering correctness still requires numeric GPU readback through the rendering validation skill;
the launcher completion record alone is only execution evidence.
