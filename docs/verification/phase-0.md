# Phase 0 Verification

## Verdict

**Phase 0 input and baseline gate: PASS.**

This verdict means the selected Sponza input, deterministic fixture, benchmark contract, and legacy
measurements are reproducible and numerically validated. It does **not** mean the legacy Sponza
renderer is correct. Every accepted Sponza run contains clear-valued view-position, view-normal,
and SSAO attachments; that correctness failure is a measured Phase 5 input.

## Revision and Environment

| Field | Value |
|---|---|
| Git revision | `44577008dfcdcbcad68cfb39ccfc38bdd87165f5` |
| Worktree | Dirty; all Phase 0 implementation and evidence was uncommitted |
| Capture-contract SHA-256 | `349e093a1c51964ad630769bd6d14d435b86a44d5a1d83118989bcc41ea02ffe` |
| Evidence completed | `2026-07-23T15:46:58Z` |
| OS | Linux Mint 22.2, kernel 6.8.0-88-generic, x86_64 |
| Compiler / CMake | GCC 13.3.0 / CMake 3.28.3 |
| Build | Release |
| GPU / driver | NVIDIA GeForce RTX 3060 / 570.153.02 |
| OpenGL | 4.3, NVIDIA 570.153.02 |
| Asset profile | `intel-sponza-base`, `full` |
| Resolution / seed | 800x600 / `1597463007` |
| Frames | 120 warm-up + 600 measured |
| Repetitions | 5 per sample |
| Cache declaration | `cold_cache=false`; live publisher request not run |

The source-contract hash covers the build file, baseline C++ sources/header, asset and benchmark
tools/tests, fixture, manifest, acceptance file, and benchmark schema. Documentation and generated
evidence are intentionally excluded.

## Commands

All commands were run from the repository root. Every command below exited `0`.

```bash
cmake -S . -B /tmp/engine-phase0-evidence-build-20260723 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/engine-phase0-evidence-build-20260723 --parallel 4
ctest --test-dir /tmp/engine-phase0-evidence-build-20260723 --output-on-failure

cmake -S . -B /tmp/engine-phase0-evidence-build-20260723 \
  -DCMAKE_BUILD_TYPE=Release \
  -DENGINE_SPONZA_ASSET_ROOT=/home/antash/workspace/engine/resources/main-sponza
ctest --test-dir /tmp/engine-phase0-evidence-build-20260723 --output-on-failure

PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests/assets -p 'test_*.py' -v
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s tests/benchmarks -p 'test_*.py' -v

python3 tools/assets/manage_assets.py fetch \
  --asset intel-sponza-base \
  --cache /home/antash/Downloads \
  --root /tmp/phase0-fetch-root-20260723 \
  --accept-license \
  --evidence artifacts/verification/phase-0/sponza-fetch.json
python3 tools/assets/manage_assets.py verify \
  --asset intel-sponza-base \
  --root /tmp/phase0-fetch-root-20260723 \
  --evidence artifacts/verification/phase-0/sponza-fetch-post-verify.json

python3 tools/assets/manage_assets.py verify-gltf \
  tests/fixtures/gltf/phase0_scene.gltf \
  --expect tests/fixtures/gltf/phase0_scene.expected.json \
  --evidence artifacts/verification/phase-0/fixture-verify.json
```

The ray-marching capture loop exited `0`:

```bash
for run in 1 2 3 4 5; do
  prlimit --core=0:0 python3 tools/benchmarks/capture_legacy.py \
    --sample ray-marching \
    --executable /tmp/engine-phase0-root-final-20260723/game_engine \
    --config validation/acceptance.json \
    --resource-root /home/antash/workspace/engine/resources \
    --output "/tmp/phase0-root-final-ray-${run}.json" \
    --schema validation/schemas/benchmark.schema.json \
    --build-type Release \
    --build-command 'cmake --build /tmp/engine-phase0-root-final-20260723 --parallel 4' \
    --timeout-seconds 180
done
```

The Sponza capture loop exited `0`:

```bash
for run in 1 2 3 4 5; do
  prlimit --core=0:0 python3 tools/benchmarks/capture_legacy.py \
    --sample sponza \
    --executable /tmp/engine-phase0-root-final-20260723/legacy_sponza \
    --config validation/acceptance.json \
    --resource-root /home/antash/workspace/engine/resources \
    --output "/tmp/phase0-root-final-sponza-${run}.json" \
    --manifest assets/manifest.json \
    --schema validation/schemas/benchmark.schema.json \
    --asset-profile full \
    --build-type Release \
    --build-command 'cmake --build /tmp/engine-phase0-root-final-20260723 --parallel 4' \
    --timeout-seconds 180
done
```

The ten records were validated before and after deduplication. The checked-in compressed bundle can
be replayed with:

```bash
mkdir -p /tmp/phase0-baseline-replay
tar -xzf artifacts/verification/phase-0/baseline-bundle.tar.gz \
  -C /tmp/phase0-baseline-replay
python3 tools/benchmarks/validate_benchmark.py \
  --acceptance validation/acceptance.json \
  validation/schemas/benchmark.schema.json \
  /tmp/phase0-baseline-replay/ray-{1,2,3,4,5}.json \
  /tmp/phase0-baseline-replay/sponza-{1,2,3,4,5}.json

python3 /home/antash/.codex/skills/validate-rendering-features/scripts/verify_evidence.py \
  artifacts/verification/phase-0/evidence.json \
  --profile asset-loading \
  --root artifacts/verification/phase-0 \
  --require-artifacts
```

## Results

| Requirement | Result | Evidence |
|---|---|---|
| Current-state source/build audit | PASS | [Current-state audit](../audit/current-state.md) |
| Pinned provenance, license, archive and closure hashes | PASS | [Asset manifest](../../assets/manifest.json) |
| Literal fetch into an empty root | PASS | [Fetch record](../../artifacts/verification/phase-0/sponza-fetch.json); checksum-verified cache hit |
| Independent post-fetch verification | PASS | [Post-fetch verification](../../artifacts/verification/phase-0/sponza-fetch-post-verify.json) |
| Atomic existing-archive install | PASS | [Archive install record](../../artifacts/verification/phase-0/sponza-archive-install.json) |
| Redistribution-safe deterministic fixture | PASS | [CC0 declaration](../../tests/fixtures/gltf/README.md) and [fixture result](../../artifacts/verification/phase-0/fixture-verify.json) |
| Asset positive/negative tests | PASS | 27/27 in [asset test log](../../artifacts/verification/phase-0/asset-tests.log) |
| Benchmark positive/negative tests | PASS | 20/20 in [benchmark test log](../../artifacts/verification/phase-0/benchmark-tests.log) |
| Clean Release configure/build | PASS | [Configure log](../../artifacts/verification/phase-0/configure.log), [build log](../../artifacts/verification/phase-0/build.log) |
| Default CTest independent of ignored Sponza tree | PASS | 3/3 in [default CTest log](../../artifacts/verification/phase-0/ctest-default.log) |
| Opt-in full closure CTest | PASS | 4/4 in [opt-in CTest log](../../artifacts/verification/phase-0/ctest-opt-in.log) |
| Five ray-marching complete records | PASS | [Baseline bundle](../../artifacts/verification/phase-0/baseline-bundle.tar.gz) |
| Five Sponza complete records | PASS | [Baseline bundle](../../artifacts/verification/phase-0/baseline-bundle.tar.gz) |
| Schema, raw buffers, timing summaries and GL-log reconciliation | PASS | [Bundle summary](../../artifacts/verification/phase-0/baseline-summary.json) and ten-record validator exit `0` |
| Finite readbacks and zero GL errors | PASS | Zero non-finite values and zero errors in all runs |
| Historical 78-second claim classified | PASS | Labeled unverified in the [audit](../audit/current-state.md) |
| Legacy Sponza rendering correctness | FAIL, non-gating observation | Position, normal and SSAO dynamic range is `0.0` |
| Live publisher network availability | NOT RUN, non-gating limitation | Fetch used `cache_hit=true` against the exact pinned archive |

The literal fetch gate passed because the fetch command independently rehashed the complete
3,987,608,266-byte cached archive, installed it atomically into an empty root, and verified the
result. It does not prove that Intel's endpoint is currently available; no network availability
claim is made.

## Baseline

Cross-run aggregation uses median and nearest-rank p95 over the five repetitions.

| Metric | Ray marching | Sponza |
|---|---:|---:|
| First-frame median / p95 | 218.403 / 445.969 ms | 40,756.477 / 40,778.830 ms |
| glTF load/upload median / p95 | N/A | 38,787.209 / 39,051.329 ms |
| Peak RSS median / p95 | 57,765,888 / 58,093,568 bytes | 10,536,452,096 / 10,538,360,832 bytes |
| Retained CPU staging | 0 bytes | 5,131,527,552 bytes |
| Per-run CPU-frame median, cross-run median | 0.003582 ms | 2.591229 ms |
| Per-run GPU-frame median, cross-run median | 0.061440 ms | 43.821056 ms |
| GL errors | 0 in every run | 0 in every run |
| GL performance warnings | 0 in every run | 11 in every run |

All five repetitions produced identical raw readback hashes. The bundle therefore stores one copy
of each unique raw buffer and GL log plus all ten records. It is 2,397,507 bytes compressed and has
SHA-256 `2088e8216ab403ec9df034c81fdae5c8ce64277679bcedc70b2d43b1bc921f94`.

## Known Failures

- Sponza view-position and view-normal contain clear RGB `(0,0,0)` with alpha `1`.
- Sponza SSAO contains only `0`.
- Sponza retains at least 5.13 GB of CPU-side image, source-buffer, vertex, and index payloads after
  the first frame. This exceeds the later 512 MiB optimization target by roughly 9.6 times.
- Five Sponza runs emit the same 11 NVIDIA shader-recompilation performance warnings.
- An intentionally invalid concurrent experiment ran two 10+ GB Sponza processes: one was
  OOM-killed and one faulted in the NVIDIA driver during contention. It is excluded from baseline
  statistics and retained only as failure-path context.

## Sign-Off

| Role | Agent | Result |
|---|---|---|
| Asset implementer | `/root/phase0_artifacts` | Complete |
| Baseline implementer | `/root/legacy_baseline_design` | Complete |
| Coordinator verification | `/root` | PASS |
| Independent verification | `/root/phase0_final_verifier` | PASS |
