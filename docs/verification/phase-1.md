# Phase 1 Verification

## Verdict

**Phase 1 build-foundation gate: PASS.**

All local and independent checks pass. The exact committed implementation revision also passed the
pushed GitHub Actions workflow, and the downloaded CI artifact contains the expected JUnit report
and complete CTest log. The independent verifier found no implementation or evidence blocker.

## Revision and Environment

| Field | Value |
|---|---|
| Accepted implementation revision | `831d4c67955100807132466210dfc3162e0212f6` |
| Remote revision | `origin/agent/multi-sample-foundation` at `831d4c67955100807132466210dfc3162e0212f6` |
| Worktree | Clean at the accepted revision before CI evidence capture |
| Draft pull request | [PR #1](https://github.com/antash-mishra/engine/pull/1) |
| Clean CI run / job | [Run 30029253959](https://github.com/antash-mishra/engine/actions/runs/30029253959) / [job 89281429706](https://github.com/antash-mishra/engine/actions/runs/30029253959/job/89281429706) |
| Independent source | Frozen effective-state copy at `/tmp/engine-phase1-independent.MV33d4/source` |
| Evidence manifest SHA-256 | `8ad17eb3dc649bd6d369f0bc14bc5a8d86ff4c5348b826ce9a9dec497522fcf1` |
| Evidence completed | `2026-07-23T17:33:19Z` |
| OS | Linux Mint 22.2, kernel 6.8.0-88-generic, x86_64 |
| Compiler / CMake / Ninja | GCC 13.3.0 / CMake 3.28.3 / Ninja 1.11.1 |
| GPU / driver | NVIDIA GeForce RTX 3060 / 570.153.02 |
| Configurations | Debug, Release, ASan+UBSan, legacy Release |
| External asset profile | None; Sponza is deliberately absent from maintained builds |

## Commands

The independent verifier copied the frozen effective source into an empty build workspace:

```bash
ROOT=/tmp/engine-phase1-independent.MV33d4
SOURCE="$ROOT/source"
mkdir -p "$SOURCE"
rsync -a \
  --exclude='/build/' \
  --exclude='/resources/main-sponza/' \
  --exclude='/CMakeFiles/' \
  --exclude='__pycache__/' \
  --exclude='*.pyc' \
  ./ "$SOURCE/"
mkdir -p "$SOURCE/build/dev/.cmake/api/v1/query"
touch "$SOURCE/build/dev/.cmake/api/v1/query/codemodel-v2"
```

The required preset, tests, JUnit output, and named targets all exited `0`:

```bash
cd "$SOURCE"
cmake --preset dev
cmake --build --preset dev --parallel 2
ctest --preset dev --output-on-failure \
  --output-junit "$ROOT/ctest-dev.xml"
cmake --build --preset dev --parallel 2 \
  --target engine_platform engine_render engine_assets engine_scene
```

The verifier inspected CMake File API target types/sources/dependencies, used `ldd` to reject
Assimp/Draco leakage, used `nm` to require exactly one maintained `gladLoadGLLoader` definition,
and rejected default-built legacy executables. Launcher discovery did not require a display or
asset root:

```bash
BUILD="$SOURCE/build/dev"
BIN="$BUILD/render_samples"
UNRELATED=$(mktemp -d "$ROOT/unrelated.XXXXXX")
cd "$UNRELATED"
env -u DISPLAY -u ENGINE_ASSET_ROOT "$BIN" --list-samples
env -u ENGINE_ASSET_ROOT timeout 45s \
  "$BIN" --sample ray-marching --frames 10
```

The completion record was parsed with `jq`, then frame presentation and file access were checked
independently:

```bash
env -u ENGINE_ASSET_ROOT timeout 45s \
  ltrace -f -e glfwSwapBuffers -o "$ROOT/ltrace-swaps.log" \
  "$BIN" --sample ray-marching --frames 10
test "$(grep -c glfwSwapBuffers "$ROOT/ltrace-swaps.log")" -eq 10

env -u ENGINE_ASSET_ROOT timeout 45s \
  strace -f -e trace=file -o "$ROOT/strace-files.log" \
  "$BIN" --sample ray-marching --frames 1
rg 'assets/shaders/(vertexCube|fragmentCube)\.glsl' "$ROOT/strace-files.log"
```

A relocated executable plus adjacent assets ran one frame. The same executable without adjacent
assets exited `1` and reported the missing executable-relative root. The build tree contained
exactly two shader files totaling 2,920 bytes and no Sponza path.

The positive dependency check exited `0`. A nested GLM mutation in an isolated source copy exited
`1`, reported `status=fail`, and set `vendored_files.glm.matches=false`:

```bash
python3 tools/build/verify_dependencies.py verify --source-root "$SOURCE"
rsync -a --exclude='/.git/' --exclude='/build/' "$SOURCE/" "$ROOT/tamper-source/"
printf '\n// verifier tamper probe\n' >> \
  "$ROOT/tamper-source/include/glm/detail/setup.hpp"
python3 "$ROOT/tamper-source/tools/build/verify_dependencies.py" verify \
  --source-root "$ROOT/tamper-source"
test "$?" -eq 1
```

The expected-failure exit status above was captured with `set +e` before the assertion.
Release, sanitizer, and compatibility builds all exited `0`:

```bash
cd "$SOURCE"
cmake --preset release
cmake --build --preset release --parallel 2
build/release/render_samples --sample ray-marching --frames 10

cmake --preset sanitizers
cmake --build --preset sanitizers --parallel 2
ctest --preset sanitizers --output-on-failure \
  --output-junit "$ROOT/ctest-sanitizers.xml"

cmake -S "$SOURCE" -B "$SOURCE/build/legacy" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=OFF \
  -DENGINE_BUILD_LEGACY_SAMPLES=ON
cmake --build "$SOURCE/build/legacy" --parallel 2 \
  --target game_engine legacy_sponza
```

Every build log was scanned for `warning:` and `error:` and contained zero matches.
`git diff --check`, `git diff --cached --check`, generated-junk scans, gitlink scans, and
`git submodule status` also passed in the source worktree.

The retained evidence can be checked with:

```bash
cd artifacts/verification/phase-1
sha256sum --check SHA256SUMS
```

The exact pushed revision and retained CI artifact were checked with:

```bash
gh run view 30029253959 --log
gh run download 30029253959 --name phase-1-ctest --dir <EMPTY_DIRECTORY>
python3 - <<'PY'
import xml.etree.ElementTree as ET
root = ET.parse("<EMPTY_DIRECTORY>/junit.xml").getroot()
assert root.attrib["tests"] == "7"
assert root.attrib["failures"] == "0"
PY
```

## Results

| Requirement | Result | Evidence |
|---|---|---|
| Real maintained target graph | PASS | [CMake graph](../../artifacts/verification/phase-1/target-graph.dot) and [dev build](../../artifacts/verification/phase-1/build-dev.log) |
| Clean Debug configure/build, zero diagnostics | PASS on frozen effective source | [Configure](../../artifacts/verification/phase-1/configure-dev.log), [build](../../artifacts/verification/phase-1/build-dev.log) |
| CTest with launcher negative paths | PASS, 7/7 CTest and 8 launcher tests | [CTest log](../../artifacts/verification/phase-1/ctest-dev.log), [JUnit](../../artifacts/verification/phase-1/ctest-dev.xml) |
| Sample registry discovery without construction | PASS | `sample_registry_unit` in JUnit |
| Listed ray-marching sample | PASS | [Listing](../../artifacts/verification/phase-1/list-unrelated.log) |
| Exactly 10 frames | PASS | [Completion JSON](../../artifacts/verification/phase-1/run-10-unrelated.log), [10 independent swaps](../../artifacts/verification/phase-1/ltrace-swaps.log) |
| Working-directory-independent asset discovery | PASS | [File trace](../../artifacts/verification/phase-1/strace-files.log) and relocation positive/negative logs |
| No copied Sponza archive | PASS | [Two staged files](../../artifacts/verification/phase-1/staged-assets.txt), 2,920 bytes |
| Pinned and machine-checked dependencies | PASS | [Positive lock result](../../artifacts/verification/phase-1/dependency-positive.json) |
| Transitive vendored mutation rejection | PASS | [Tamper result](../../artifacts/verification/phase-1/dependency-tamper.json) |
| Exact CI packages available from apt | PASS, 8/8 | [Install arguments](../../artifacts/verification/phase-1/hardening-apt-arguments.txt), [candidate check](../../artifacts/verification/phase-1/hardening-apt-candidate-availability.txt) |
| Dependency verifier regression tests | PASS, 3/3 | [Unit log](../../artifacts/verification/phase-1/hardening-dependency-unit.log) |
| Optimized maintained build | PASS | [Release build](../../artifacts/verification/phase-1/build-release.log) and [10-frame result](../../artifacts/verification/phase-1/run-release-10.log) |
| Address/undefined sanitizer suite | PASS, 7/7 | [Sanitizer log](../../artifacts/verification/phase-1/ctest-sanitizers.log), [JUnit](../../artifacts/verification/phase-1/ctest-sanitizers.xml) |
| Legacy comparison targets still compile | PASS | [Legacy build](../../artifacts/verification/phase-1/build-legacy.log) |
| Public API contracts and contributor guides | PASS | [Comment scan](../../artifacts/verification/phase-1/api-contract-comments.txt), [architecture](../architecture.md), [sample guide](../adding-a-sample.md) |
| Broken ImGui gitlink removed; no source junk | PASS | Empty gitlink/submodule/junk evidence files |
| CI workflow pins actions and retains JUnit | PASS | [Workflow](../../.github/workflows/ci.yml), [run metadata](../../artifacts/verification/phase-1/ci-run-831d4c6.json), and [artifact metadata](../../artifacts/verification/phase-1/ci-artifacts-831d4c6.json) |
| Retained evidence integrity | PASS, 45/45 | [SHA-256 manifest](../../artifacts/verification/phase-1/SHA256SUMS) |
| Project revision contains Phase 1 | PASS | [Accepted revision record](../../artifacts/verification/phase-1/clean-revision-ci.txt) |
| Clean-checkout pushed CI job and artifact | PASS, 7/7 | [CI log](../../artifacts/verification/phase-1/ci-run-831d4c6.log), [JUnit](../../artifacts/verification/phase-1/ci-junit-831d4c6.xml), and [CTest log](../../artifacts/verification/phase-1/ci-last-test-831d4c6.log) |

## Completion Basis

The accepted implementation revision is committed, pushed, and represented by an exact successful
CI run. The independent verifier checked its head SHA, branch, successful job steps, uploaded
artifact metadata, a byte-identical artifact redownload, all seven named CTest cases, and the
repaired 45-file checksum manifest. The verifier explicitly authorized closure.

The CI packages and action source revisions are exact. The CI-only lock includes Xvfb, Xauthority,
and Mesa's software DRI renderer so `--no-install-recommends` cannot omit headless OpenGL support.
Ubuntu package availability still depends on mutable archive repositories; only the CI install and
run can validate that path.

## Sign-Off

| Role | Agent | Result |
|---|---|---|
| Build/launcher implementer | `/root/phase1_build_launcher`, `/root` | Code complete |
| Coordinator verification | `/root` | Code-controlled gates PASS |
| Independent verification | `/root/phase1_acceptance_design` | PASS; closure authorized |
| Overall Phase 1 gate | `/root` | COMPLETE |
