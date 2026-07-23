# Adding a Sample

## Contract

A maintained sample implements `engine::sample::Sample` and provides a factory returning
`std::unique_ptr<Sample>`. Its descriptor name is the stable command-line identifier. `run()` owns
sample behavior and GPU resources, but it receives window dimensions, an optional fixed frame
limit, and a validated `AssetRoot`; it does not parse process arguments or infer paths from the
working directory.

## Steps

1. Add `src/samples/<name>_sample.cpp`.
2. Implement `run(const SampleRunConfig&)`, a factory, and a side-effect-free
   `SampleRegistration`.
3. Declare the registration function in `src/engine/sample/sample_registry.cpp` and add its result
   in `createDefaultRegistry()`.
4. Add the source to `engine_samples` in `CMakeLists.txt`.
5. Stage only the files the sample requires, or require an explicit external asset root. Never copy
   an undeclared resource tree.
6. Add launcher coverage for listing, a bounded run, initialization failure, and missing assets.
7. Add feature-specific numeric tests and raw GPU readbacks; screenshots are supplementary only.

## Ownership Pattern

Create the platform window before context-bound render objects so C++ reverse destruction releases
GPU handles first:

```cpp
platform::Window window(config);
render::initializeOpenGl();
render::SomePass pass(...);
```

Do not store owning raw GL handles in copyable types. New shared GPU abstractions belong in
`engine_render`, not in a sample. Until Phase 3 supplies common wrappers, sample-local owners must
delete every handle deterministically and document their required context/thread lifetime.

Registration metadata must be available without constructing the sample. Names use lowercase ASCII
letters, digits, and internal hyphens; summaries are nonempty single lines. This keeps
`--list-samples` free of sample, asset, GLFW, and OpenGL side effects and keeps machine-readable
launcher output valid.

## Frame Limits

Bounded execution must present exactly `SampleRunConfig::frameLimit` frames and return that count.
Use a fixed timestep or frame index for time-dependent behavior. Do not use wall-clock time in a
deterministic run, and do not emit the launcher completion record from sample code.

## Assets and Paths

Resolve files through `config.assetRoot.file("relative/path")`. This rejects missing files,
absolute paths, traversal, and symlink escape. A sample must not call
`std::filesystem::current_path()` for resource discovery.

For a large external model, add or extend a pinned manifest/profile and keep the build-tree staging
small. A clean checkout must still configure, build, list samples, and run tests that do not require
the external model.

## Comments and Documentation

Document public purpose, ownership, lifetime, thread restrictions, failure behavior, and render-pass
inputs/outputs/state assumptions. Use names and focused functions for routine control flow instead
of narrating each line. Update `docs/architecture.md` when a new target dependency or cross-sample
service is introduced.
