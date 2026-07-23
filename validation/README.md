# Validation Inputs

`acceptance.json` fixes the inputs and thresholds used by later phase gates. Tests must read these
values rather than select tolerances, cameras, seeds, or budgets after observing a candidate run.
Changing a value requires a decision record and rerunning every affected baseline.

## Parameter Basis

- The legacy camera values come directly from `src/ray_marching.cpp` and `src/sponza.cpp`.
  Ray marching hardcodes projection planes `0.1` and `100.0`; Sponza uses the shared camera defaults
  `0.5` and `60.0`.
- General numeric tolerances target 32-bit shader arithmetic. Buffer overrides are wider only where
  half-float storage, sampling, or screen-space filtering adds quantization.
- A maximum bad-pixel fraction of `0.001` permits defined raster-edge disagreement while every
  NaN/Inf or out-of-range intermediate still fails independently.
- Five startup repetitions and 600 measured frames are initial minimum sample counts for median and
  p95 statistics. Performance comparisons must retain every raw sample.
- The 60-second cold-start and 12 GiB peak-RSS limits are provisional regression ceilings, not
  optimization success criteria. They preserve headroom around the 2026-07-22 legacy diagnostic
  while Phase 6 establishes tighter cold/warm and retained-memory targets.
- The 512 MiB retained-staging target requires decoded source payloads to be released after upload.
  It is intentionally much lower than the legacy peak and remains unverified.
- DDGI tolerances and 2/4/8 ms update budgets are predeclared engineering targets for low, medium,
  and high presets. Phase 8 must validate that they are feasible on the reference GPU; any revision
  requires a recorded decision before Phase 9 measurements.

The fixture hashes pin the exact locally-authored glTF input and expectation file. The asset
manifest separately pins the full Sponza glTF URI closure.

## Commands

```bash
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/benchmarks -p 'test_*.py' -v
python3 tools/benchmarks/validate_benchmark.py \
  validation/schemas/benchmark.schema.json \
  artifacts/verification/phase-0/sponza-legacy-loader.partial.json
```

The checked-in Sponza loader JSON is a historical `partial` diagnostic. Five current-contract
ray-marching and five Sponza captures are retained in
`artifacts/verification/phase-0/baseline-bundle.tar.gz`. Their GL-debug JSONL and numeric readbacks
are hash-bound, deduplicated only after identical hashes were established, and independently
revalidated after bundle extraction. `docs/verification/phase-0.md` records the complete commands,
metrics, limitations, and verdict.

## Legacy Capture

Phase 0 adds instrumentation to the two existing executables without introducing the future sample
launcher. Running either executable with no arguments keeps its interactive loop. Deterministic
capture is enabled only through the repository orchestrator:

```bash
python3 tools/benchmarks/capture_legacy.py \
  --sample ray-marching \
  --executable build/phase-0/game_engine \
  --config validation/acceptance.json \
  --resource-root resources \
  --output artifacts/verification/phase-0/ray-marching.json
```

`startup.total_to_first_frame_ms` runs from entry into `main` through `glFinish` after the first
complete final draw. CPU frame samples measure command submission and exclude swap, synchronous
readback, and the first-frame finish. GPU samples use `GL_TIME_ELAPSED`; result collection may block
only after the measured loop. Readbacks happen on the zero-based final measured frame and store
bottom-to-top, little-endian raw data next to the JSON record.

`memory.peak_rss_bytes` is the kernel-reported process high-water mark. The separate
`retained_cpu_bytes_after_first_frame` value is a scoped lower bound: Sponza counts still-live
TinyGLTF buffer/image payload sizes and duplicated loader mesh vertex/index sizes, while the
procedural ray sample reports zero asset staging. Neither field estimates GPU memory.

The validator independently checks artifact containment, size, SHA-256, finite values, channel
statistics, sample count, capture frame, GL diagnostics, and child exit status. It also recomputes
median and nearest-rank p95 from every raw CPU/GPU sample and enforces a full RFC 3339 capture
timestamp. The GL-debug JSONL byte count and SHA-256 are bound into the record; every line is parsed,
classified using the same error/high and performance/medium rules as the C++ producer, and
reconciled with all three message counts. A screenshot is not used as correctness evidence.

For Sponza, the orchestrator hashes the manifest-selected glTF entrypoint and its complete local URI
closure before it starts the renderer. The resulting asset fields are derived from the supplied
`--resource-root` bytes. The child keeps `resources/` as its shader root while the manifest maps the
asset check to `resources/main-sponza/`; an empty, stale, or different asset tree is rejected before
child launch.

The multi-gigabyte Sponza closure check is opt-in so a clean checkout can run default CTest:

```bash
cmake -S . -B build/phase-0 \
  -DENGINE_SPONZA_ASSET_ROOT="$PWD/resources/main-sponza"
ctest --test-dir build/phase-0 --output-on-failure -R '^asset_manifest$'
```
