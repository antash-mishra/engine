#!/usr/bin/env python3
"""Capture one deterministic Phase 0 run from a legacy rendering executable."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import sys
import tempfile
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

from tools.assets.asset_common import (  # noqa: E402
    VerificationError as AssetVerificationError,
    compare_fields,
    load_asset,
    read_json as read_asset_manifest,
    resolve_under,
)
from tools.assets.gltf_validation import (  # noqa: E402
    closure_fingerprint,
    local_uri_closure,
)


DEFAULT_MANIFEST = REPOSITORY_ROOT / "assets" / "manifest.json"
DEFAULT_SCHEMA = REPOSITORY_ROOT / "validation" / "schemas" / "benchmark.schema.json"


class CaptureError(RuntimeError):
    """The child executable could not produce a complete baseline record."""


def read_object(path: pathlib.Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise CaptureError(f"expected JSON object in {path}")
    return value


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def closure(files: list[pathlib.Path], root: pathlib.Path) -> tuple[str, int]:
    """Hash paths, sizes, and payload hashes so renames also change the closure."""
    digest = hashlib.sha256()
    total = 0
    for path in sorted(files):
        relative = path.resolve().relative_to(root.resolve()).as_posix()
        size = path.stat().st_size
        payload_hash = sha256_file(path)
        digest.update(f"{relative}\0{size}\0{payload_hash}\n".encode())
        total += size
    return digest.hexdigest(), total


def git_revision() -> dict[str, Any]:
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=REPOSITORY_ROOT, text=True
    ).strip()
    dirty = bool(
        subprocess.check_output(
            ["git", "status", "--porcelain"], cwd=REPOSITORY_ROOT, text=True
        ).strip()
    )
    return {"git_commit": commit, "dirty": dirty}


def system_description() -> dict[str, Any]:
    os_name = platform.platform()
    os_release = pathlib.Path("/etc/os-release")
    if os_release.is_file():
        values = {}
        for line in os_release.read_text(encoding="utf-8").splitlines():
            key, separator, value = line.partition("=")
            if separator:
                values[key] = value.strip('"')
        os_name = f"{values.get('PRETTY_NAME', os_name)}, {platform.platform()}"

    cpu = platform.processor()
    cpuinfo = pathlib.Path("/proc/cpuinfo")
    if cpuinfo.is_file():
        for line in cpuinfo.read_text(encoding="utf-8", errors="replace").splitlines():
            if line.startswith("model name"):
                cpu = line.partition(":")[2].strip()
                break
    ram_bytes = os.sysconf("SC_PAGE_SIZE") * os.sysconf("SC_PHYS_PAGES")
    return {"os": os_name, "cpu": cpu or "unknown", "ram_bytes": ram_bytes}


def asset_description(
    sample: str, resource_root: pathlib.Path, manifest_path: pathlib.Path
) -> dict[str, Any]:
    if sample == "ray-marching":
        vertex = resource_root / "shaders" / "vertexCube.glsl"
        fragment = resource_root / "shaders" / "fragmentCube.glsl"
        closure_hash, closure_bytes = closure([vertex, fragment], resource_root)
        return {
            "id": "procedural-raymarch",
            "profile": "custom",
            "entrypoint_sha256": sha256_file(fragment),
            "closure_sha256": closure_hash,
            "closure_bytes": closure_bytes,
        }

    try:
        asset = load_asset(
            read_asset_manifest(manifest_path), "intel-sponza-base"
        )
        local = asset["local"]
        configured_root = pathlib.PurePosixPath(local["root"])
        try:
            asset_relative_to_resources = configured_root.relative_to("resources")
        except ValueError as error:
            raise AssetVerificationError(
                "manifest local.root must be below the repository resources root"
            ) from error
        asset_root = resolve_under(resource_root, asset_relative_to_resources)
        entrypoint = pathlib.PurePosixPath(local["entrypoint"])
        entrypoint_path = resolve_under(asset_root, entrypoint)
        if not entrypoint_path.is_file():
            raise AssetVerificationError(
                f"asset entrypoint is missing: {entrypoint_path}"
            )

        observed_entrypoint_hash = sha256_file(entrypoint_path)
        if observed_entrypoint_hash != local["entrypoint_sha256"]:
            raise AssetVerificationError("glTF entrypoint SHA-256 mismatch")

        gltf = read_asset_manifest(entrypoint_path)
        observed_closure = closure_fingerprint(
            asset_root, local_uri_closure(gltf, entrypoint)
        )
        expected_closure = {
            field: asset["gltf_closure"][field]
            for field in ("algorithm", "file_count", "total_bytes", "tree_sha256")
        }
        compare_fields(observed_closure, expected_closure, "gltf_closure")
    except (AssetVerificationError, KeyError, OSError) as error:
        raise CaptureError(f"Sponza asset preflight failed: {error}") from error

    return {
        "id": asset["id"],
        "profile": "full",
        "entrypoint_sha256": observed_entrypoint_hash,
        "closure_sha256": observed_closure["tree_sha256"],
        "closure_bytes": observed_closure["total_bytes"],
    }


def relative_artifact(
    path_value: str,
    output: pathlib.Path,
    run_root: pathlib.Path,
    artifact_root: pathlib.Path,
) -> tuple[str, pathlib.Path]:
    original = pathlib.Path(path_value).resolve()
    try:
        within_run = original.relative_to(run_root.resolve())
    except ValueError as error:
        raise CaptureError(f"child artifact escapes its fresh run directory: {original}") from error
    path = (artifact_root / within_run).resolve()
    root = output.parent.resolve()
    if path != root and root not in path.parents:
        raise CaptureError(f"child artifact escapes output directory: {path}")
    return path.relative_to(root).as_posix(), path


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sample", required=True, choices=("ray-marching", "sponza"))
    parser.add_argument("--executable", type=pathlib.Path, required=True)
    parser.add_argument("--config", type=pathlib.Path, required=True)
    parser.add_argument("--resource-root", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--schema", type=pathlib.Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--asset-profile", choices=("full",), default="full")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--timeout-seconds", type=float, default=600.0)
    parser.add_argument(
        "--build-command",
        default="cmake --build build/phase-0 --parallel",
    )
    return parser


def compiler_description(executable: pathlib.Path) -> str:
    compiler = "c++"
    for directory in (executable.parent, *executable.parents):
        cache = directory / "CMakeCache.txt"
        if cache.is_file():
            for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
                if line.startswith("CMAKE_CXX_COMPILER:FILEPATH="):
                    compiler = line.partition("=")[2]
                    break
            break
    try:
        return subprocess.check_output(
            [compiler, "--version"], text=True, stderr=subprocess.STDOUT
        ).splitlines()[0]
    except (OSError, subprocess.SubprocessError):
        return compiler


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists():
        raise CaptureError(f"refusing to overwrite existing benchmark: {output}")
    executable = args.executable.resolve()
    resource_root = args.resource_root.resolve()
    artifact_root = output.parent / f"{output.stem}.artifacts"
    if artifact_root.exists():
        raise CaptureError(f"refusing to reuse artifact directory: {artifact_root}")

    # Bind provenance to the supplied bytes before creating configuration or
    # starting the child. A missing, stale, or different Sponza tree cannot run.
    verified_asset = asset_description(
        args.sample, resource_root, args.manifest.resolve()
    )
    acceptance = read_object(args.config.resolve())

    camera_name = (
        "ray_marching_default" if args.sample == "ray-marching" else "sponza_legacy_default"
    )
    camera = acceptance["cameras"][camera_name]
    width, height = acceptance["resolutions"]["baseline"]
    timing = acceptance["timing"]
    run_root = pathlib.Path(
        tempfile.mkdtemp(prefix=f".{output.stem}.run-", dir=output.parent)
    ).resolve()
    child_result = run_root / "child-result.json"
    child_config = run_root / "child-config.json"
    child_config.write_text(
        json.dumps(
            {
                "width": width,
                "height": height,
                "warmup_frames": timing["warmup_frames"],
                "measured_frames": timing["measured_frames"],
                "seed": acceptance["random_seed"],
                "fixed_timestep_seconds": timing["fixed_timestep_seconds"],
                "resource_root": str(resource_root),
                "artifact_root": str(run_root),
                "child_result": str(child_result),
                "camera": camera,
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )

    captured_at = datetime.datetime.now(datetime.timezone.utc).isoformat().replace("+00:00", "Z")
    command = [str(executable), "--legacy-baseline-config", str(child_config)]
    timed_out = False
    try:
        completed = subprocess.run(
            command,
            text=True,
            capture_output=True,
            check=False,
            timeout=args.timeout_seconds,
        )
        stdout = completed.stdout
        stderr = completed.stderr
        return_code = completed.returncode
    except subprocess.TimeoutExpired as error:
        timed_out = True
        stdout = error.stdout or ""
        stderr = error.stderr or ""
        return_code = 124
    (run_root / "stdout.log").write_text(stdout, encoding="utf-8")
    (run_root / "stderr.log").write_text(stderr, encoding="utf-8")

    has_child_result = child_result.is_file()
    os.replace(run_root, artifact_root)
    common = {
        "schema_version": 1,
        "captured_at": captured_at,
        "revision": git_revision(),
        "build": {
            "type": args.build_type,
            "compiler": compiler_description(executable),
            "command": args.build_command,
        },
        "system": system_description(),
        "asset": verified_asset,
        "settings": {
            "sample": args.sample,
            "width": width,
            "height": height,
            "camera": {"name": camera_name, **camera},
        },
    }
    if not has_child_result:
        partial = {
            **common,
            "capture_status": "partial",
            "gpu": {"name": "unavailable", "driver": "unavailable", "api": "unavailable"},
            "blockers": [
                (
                    f"legacy child exceeded the {args.timeout_seconds:g} second timeout"
                    if timed_out
                    else f"legacy child exited {return_code} before writing its result"
                )
            ],
        }
        output.write_text(json.dumps(partial, indent=2) + "\n", encoding="utf-8")
        return 1
    child = read_object(artifact_root / child_result.name)
    if return_code != 0:
        child_validation = child.get("validation", {})
        partial = {
            **common,
            "capture_status": "partial",
            "gpu": {
                "name": child.get("gpu", {}).get("name", "unavailable"),
                "driver": child.get("gpu", {}).get("driver", "unavailable"),
                "api": child.get("gpu", {}).get("api", "unavailable"),
            },
            "observations": [
                {
                    "name": "child_exit_code",
                    "value": return_code,
                    "unit": "code",
                    "source": "legacy baseline child",
                },
                {
                    "name": "gl_debug_error_count",
                    "value": child_validation.get("gl_debug_error_count", 0),
                    "unit": "messages",
                    "source": "OpenGL debug callback",
                },
                {
                    "name": "non_finite_value_count",
                    "value": child_validation.get("non_finite_value_count", 0),
                    "unit": "values",
                    "source": "legacy readback scanner",
                },
            ],
            "blockers": [
                "legacy child reported a validation failure; inspect child-result.json and raw artifacts"
            ],
        }
        output.write_text(json.dumps(partial, indent=2) + "\n", encoding="utf-8")
        return 1

    readbacks = []
    for readback in child["readbacks"]:
        relative, raw_path = relative_artifact(
            readback["raw_path"], output, run_root, artifact_root
        )
        readback = dict(readback)
        readback["raw_path"] = relative
        readback["byte_count"] = raw_path.stat().st_size
        readback["sha256"] = sha256_file(raw_path)
        readbacks.append(readback)

    validation = dict(child["validation"])
    debug_relative, debug_path = relative_artifact(
        validation["gl_debug_path"], output, run_root, artifact_root
    )
    validation["gl_debug_path"] = debug_relative
    validation["gl_debug_byte_count"] = debug_path.stat().st_size
    validation["gl_debug_sha256"] = sha256_file(debug_path)
    validation["exit_code"] = return_code
    thresholds = acceptance["legacy_baseline"]["diagnostic_min_dynamic_range"][
        args.sample
    ]
    readbacks_by_name = {item["name"]: item for item in readbacks}
    semantic_checks = []
    for name, threshold in thresholds.items():
        readback = readbacks_by_name[name]
        observed = max(
            channel["max"] - channel["min"]
            for channel in readback["channel_statistics"]
        )
        semantic_checks.append(
            {
                "name": f"{name}_nonuniform",
                "readback": name,
                "status": "pass" if observed >= threshold else "fail",
                "required_for_capture": False,
                "observed_dynamic_range": observed,
                "minimum_dynamic_range": threshold,
            }
        )
    validation["semantic_checks"] = semantic_checks

    feature_prefix = "ray_marching." if args.sample == "ray-marching" else "sponza."
    feature_flags = {
        name: value
        for name, value in acceptance["legacy_baseline"]["feature_flags"].items()
        if name.startswith(feature_prefix)
    }
    document = {
        **common,
        "capture_status": "complete",
        "gpu": {
            "name": child["gpu"]["name"],
            "driver": child["gpu"]["driver"],
            "api": child["gpu"]["api"],
        },
        "settings": {
            "sample": args.sample,
            "width": width,
            "height": height,
            "vsync": False,
            "cold_cache": False,
            "warmup_frames": timing["warmup_frames"],
            "measured_frames": timing["measured_frames"],
            "camera": {"name": camera_name, **camera},
            "seed": acceptance["random_seed"],
            "fixed_timestep_seconds": timing["fixed_timestep_seconds"],
            "feature_flags": feature_flags,
        },
        "startup": {
            "total_to_first_frame_ms": child["startup"]["total_to_first_frame_ms"],
            "spans": child["startup"]["spans"],
        },
        "memory": {
            "peak_rss_bytes": child["memory"]["peak_rss_bytes"],
            "retained_cpu_bytes_after_first_frame": child["memory"][
                "retained_cpu_bytes_after_first_frame"
            ],
            "retained_cpu_scope": child["memory"]["retained_cpu_scope"],
        },
        "frames": child["frames"],
        "readbacks": readbacks,
        "validation": validation,
    }
    output.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")

    schema_check = subprocess.run(
        [
            sys.executable,
            str(REPOSITORY_ROOT / "tools/benchmarks/validate_benchmark.py"),
            str(args.schema.resolve()),
            str(output),
            "--acceptance",
            str(args.config.resolve()),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    (artifact_root / "schema-validation.log").write_text(
        schema_check.stdout + schema_check.stderr, encoding="utf-8"
    )

    # Keep invalid evidence for diagnosis, but never report a successful capture
    # when either the child or independent artifact validator observed a failure.
    return 0 if return_code == 0 and schema_check.returncode == 0 else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (CaptureError, KeyError, OSError, json.JSONDecodeError) as error:
        print(json.dumps({"status": "fail", "error": str(error)}, indent=2), file=sys.stderr)
        raise SystemExit(1)
