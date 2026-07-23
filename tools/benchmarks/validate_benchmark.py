#!/usr/bin/env python3
"""Validate Phase 0 benchmark records without third-party Python packages.

This validator enforces the repository's benchmark schema contract and the
acceptance-file invariants needed by CI. A generic Draft 2020-12 validator may
also consume the checked-in schema; this tool exists so offline clean checkouts
do not need to download a Python package before validating baseline records.
"""

from __future__ import annotations

import argparse
import array
import datetime
import hashlib
import json
import math
import pathlib
import re
import sys
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_ACCEPTANCE = REPOSITORY_ROOT / "validation" / "acceptance.json"
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
GL_DEBUG_TYPE_ERROR = 0x824C
GL_DEBUG_TYPE_PERFORMANCE = 0x8250
GL_DEBUG_SEVERITY_HIGH = 0x9146
GL_DEBUG_SEVERITY_MEDIUM = 0x9147
RFC3339_DATE_TIME_PATTERN = re.compile(
    r"^(?P<date>[0-9]{4}-[0-9]{2}-[0-9]{2})"
    r"[Tt](?:[01][0-9]|2[0-3]):[0-5][0-9]:(?:[0-5][0-9]|60)"
    r"(?:\.[0-9]+)?(?:[Zz]|[+-](?:[01][0-9]|2[0-3]):[0-5][0-9])$"
)


class ValidationError(RuntimeError):
    """A benchmark or acceptance contract is incomplete or malformed."""


def require_rfc3339_date_time(value: Any, context: str) -> None:
    """Require the full date, time, seconds, and timezone form used by JSON Schema."""
    if not isinstance(value, str):
        raise ValidationError(f"{context} must be an RFC 3339 date-time")
    match = RFC3339_DATE_TIME_PATTERN.fullmatch(value)
    if match is None:
        raise ValidationError(f"{context} must be an RFC 3339 date-time")
    try:
        datetime.date.fromisoformat(match.group("date"))
    except ValueError as error:
        raise ValidationError(f"{context} must be an RFC 3339 date-time") from error


def _matches_type(value: Any, expected: str) -> bool:
    if expected == "object":
        return isinstance(value, dict)
    if expected == "array":
        return isinstance(value, list)
    if expected == "string":
        return isinstance(value, str)
    if expected == "boolean":
        return isinstance(value, bool)
    if expected == "integer":
        return isinstance(value, int) and not isinstance(value, bool)
    if expected == "number":
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    if expected == "null":
        return value is None
    raise ValidationError(f"schema uses unsupported type {expected!r}")


def validate_schema_instance(
    value: Any,
    schema: dict[str, Any],
    root_schema: dict[str, Any],
    context: str,
) -> None:
    """Apply the JSON-Schema subset used by the checked-in benchmark contract.

    Keeping this evaluator local makes clean offline checkouts deterministic;
    every keyword used by benchmark.schema.json is handled explicitly.
    """
    if "$ref" in schema:
        reference = schema["$ref"]
        if not isinstance(reference, str) or not reference.startswith("#/"):
            raise ValidationError(f"{context}: only local schema references are supported")
        target: Any = root_schema
        for part in reference[2:].split("/"):
            target = target[part.replace("~1", "/").replace("~0", "~")]
        validate_schema_instance(value, target, root_schema, context)
        return

    expected_type = schema.get("type")
    if expected_type is not None:
        candidates = expected_type if isinstance(expected_type, list) else [expected_type]
        if not any(_matches_type(value, candidate) for candidate in candidates):
            raise ValidationError(f"{context} does not match schema type {expected_type!r}")
    if "const" in schema and value != schema["const"]:
        raise ValidationError(f"{context} must equal {schema['const']!r}")
    if "enum" in schema and value not in schema["enum"]:
        raise ValidationError(f"{context} is not an allowed value")

    if isinstance(value, dict):
        required = schema.get("required", [])
        missing = [field for field in required if field not in value]
        if missing:
            raise ValidationError(f"{context} is missing required fields: {', '.join(missing)}")
        properties = schema.get("properties", {})
        additional = schema.get("additionalProperties", True)
        for key, item in value.items():
            if key in properties:
                validate_schema_instance(item, properties[key], root_schema, f"{context}.{key}")
            elif additional is False:
                raise ValidationError(f"{context} contains schema-forbidden field {key!r}")
            elif isinstance(additional, dict):
                validate_schema_instance(item, additional, root_schema, f"{context}.{key}")
    elif isinstance(value, list):
        if len(value) < schema.get("minItems", 0):
            raise ValidationError(f"{context} has too few items")
        if "maxItems" in schema and len(value) > schema["maxItems"]:
            raise ValidationError(f"{context} has too many items")
        for index, item_schema in enumerate(schema.get("prefixItems", [])):
            if index < len(value):
                validate_schema_instance(value[index], item_schema, root_schema, f"{context}[{index}]")
        if isinstance(schema.get("items"), dict):
            for index, item in enumerate(value):
                validate_schema_instance(item, schema["items"], root_schema, f"{context}[{index}]")
    elif isinstance(value, str):
        if len(value) < schema.get("minLength", 0):
            raise ValidationError(f"{context} is shorter than the schema minimum")
        if "pattern" in schema and not re.fullmatch(schema["pattern"], value):
            raise ValidationError(f"{context} does not match the schema pattern")
        schema_format = schema.get("format")
        if schema_format == "date-time":
            require_rfc3339_date_time(value, context)
        elif schema_format is not None:
            raise ValidationError(
                f"{context}: schema uses unsupported format {schema_format!r}"
            )
    elif isinstance(value, (int, float)) and not isinstance(value, bool):
        if "minimum" in schema and value < schema["minimum"]:
            raise ValidationError(f"{context} is below the schema minimum")
        if "maximum" in schema and value > schema["maximum"]:
            raise ValidationError(f"{context} exceeds the schema maximum")
        if "exclusiveMinimum" in schema and value <= schema["exclusiveMinimum"]:
            raise ValidationError(f"{context} must exceed the schema exclusive minimum")
        if "exclusiveMaximum" in schema and value >= schema["exclusiveMaximum"]:
            raise ValidationError(f"{context} must be below the schema exclusive maximum")

    for item_schema in schema.get("allOf", []):
        validate_schema_instance(value, item_schema, root_schema, context)
    condition = schema.get("if")
    if isinstance(condition, dict):
        try:
            validate_schema_instance(value, condition, root_schema, context)
            condition_matches = True
        except ValidationError:
            condition_matches = False
        branch = schema.get("then") if condition_matches else schema.get("else")
        if isinstance(branch, dict):
            validate_schema_instance(value, branch, root_schema, context)


def read_object(path: pathlib.Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"cannot read JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise ValidationError(f"expected a JSON object in {path}")
    return value


def require_fields(value: dict[str, Any], fields: tuple[str, ...], context: str) -> None:
    missing = [field for field in fields if field not in value]
    if missing:
        raise ValidationError(f"{context} is missing required fields: {', '.join(missing)}")


def require_finite_numbers(value: Any, context: str) -> None:
    if isinstance(value, bool) or value is None or isinstance(value, str):
        return
    if isinstance(value, (int, float)):
        if not math.isfinite(value):
            raise ValidationError(f"{context} contains a non-finite number")
        return
    if isinstance(value, list):
        for index, item in enumerate(value):
            require_finite_numbers(item, f"{context}[{index}]")
        return
    if isinstance(value, dict):
        for key, item in value.items():
            require_finite_numbers(item, f"{context}.{key}")
        return
    raise ValidationError(f"{context} contains unsupported value type {type(value).__name__}")


def require_sha256(value: Any, context: str) -> None:
    if not isinstance(value, str) or not SHA256_PATTERN.fullmatch(value):
        raise ValidationError(f"{context} must be a lowercase SHA-256")


def require_positive(value: Any, context: str, allow_zero: bool = False) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValidationError(f"{context} must be numeric")
    if value < 0 or (value == 0 and not allow_zero):
        qualifier = "nonnegative" if allow_zero else "positive"
        raise ValidationError(f"{context} must be {qualifier}")


def median(values: list[float]) -> float:
    """Reproduce the legacy C++ producer's sorted midpoint convention."""
    ordered = sorted(values)
    middle = len(ordered) // 2
    if len(ordered) % 2 == 0:
        return (ordered[middle - 1] + ordered[middle]) * 0.5
    return ordered[middle]


def percentile95(values: list[float]) -> float:
    """Reproduce the producer's nearest-rank p95: ceil(0.95 * n)."""
    ordered = sorted(values)
    rank = math.ceil(0.95 * len(ordered))
    return ordered[max(1, rank) - 1]


def validate_frame_summaries(frames: dict[str, Any]) -> None:
    expected = {
        "cpu_median_ms": median(frames["raw_cpu_ms"]),
        "cpu_p95_ms": percentile95(frames["raw_cpu_ms"]),
        "gpu_median_ms": median(frames["raw_gpu_ms"]),
        "gpu_p95_ms": percentile95(frames["raw_gpu_ms"]),
    }
    for field, computed in expected.items():
        if frames[field] != computed:
            raise ValidationError(
                f"frames.{field} disagrees with its raw frame samples: "
                f"recorded {frames[field]!r}, computed {computed!r}"
            )


def file_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_readback_payload(
    path: pathlib.Path, readback: dict[str, Any], context: str
) -> None:
    payload = path.read_bytes()
    if readback["component_type"] == "float32":
        values_array = array.array("f")
        values_array.frombytes(payload)
        if sys.byteorder != "little":
            values_array.byteswap()
        values: Any = values_array
    else:
        values = payload

    channels = readback["channels"]
    minima = [math.inf] * channels
    maxima = [-math.inf] * channels
    sums = [0.0] * channels
    counts = [0] * channels
    non_finite = 0
    for index, value in enumerate(values):
        numeric = float(value)
        if not math.isfinite(numeric):
            non_finite += 1
            continue
        channel = index % channels
        minima[channel] = min(minima[channel], numeric)
        maxima[channel] = max(maxima[channel], numeric)
        sums[channel] += numeric
        counts[channel] += 1

    if non_finite != readback["non_finite_count"]:
        raise ValidationError(f"{context} non-finite count disagrees with raw artifact")
    if len(values) - non_finite != readback["finite_count"]:
        raise ValidationError(f"{context} finite count disagrees with raw artifact")
    for channel, recorded in enumerate(readback["channel_statistics"]):
        computed = (
            0.0,
            0.0,
            0.0,
        ) if counts[channel] == 0 else (
            minima[channel],
            maxima[channel],
            sums[channel] / counts[channel],
        )
        for field, value in zip(("min", "max", "mean"), computed):
            if not math.isclose(
                recorded[field], value, rel_tol=1e-6, abs_tol=1e-7
            ):
                raise ValidationError(
                    f"{context}.channel_statistics[{channel}].{field} "
                    "disagrees with raw artifact"
                )


def validate_gl_debug_payload(
    path: pathlib.Path, validation: dict[str, Any]
) -> None:
    payload = path.read_bytes()
    if len(payload) != validation["gl_debug_byte_count"]:
        raise ValidationError("validation GL debug artifact size mismatch")
    if file_sha256(path) != validation["gl_debug_sha256"]:
        raise ValidationError("validation GL debug artifact SHA-256 mismatch")

    records: list[dict[str, Any]] = []
    for line_number, encoded_line in enumerate(payload.splitlines(), start=1):
        if not encoded_line:
            raise ValidationError(
                f"validation GL debug JSONL line {line_number} is empty"
            )
        try:
            record = json.loads(encoded_line.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValidationError(
                f"validation GL debug JSONL line {line_number} is invalid"
            ) from error
        if not isinstance(record, dict):
            raise ValidationError(
                f"validation GL debug JSONL line {line_number} must be an object"
            )
        expected_fields = {"source", "type", "id", "severity", "message"}
        if set(record) != expected_fields:
            raise ValidationError(
                f"validation GL debug JSONL line {line_number} has wrong fields"
            )
        for field in ("source", "type", "id", "severity"):
            value = record[field]
            if (
                not isinstance(value, int)
                or isinstance(value, bool)
                or not 0 <= value <= 0xFFFFFFFF
            ):
                raise ValidationError(
                    f"validation GL debug JSONL line {line_number}.{field} "
                    "must be an unsigned 32-bit integer"
                )
        if not isinstance(record["message"], str):
            raise ValidationError(
                f"validation GL debug JSONL line {line_number}.message must be a string"
            )
        records.append(record)

    error_count = 0
    warning_count = 0
    for record in records:
        if (
            record["type"] == GL_DEBUG_TYPE_ERROR
            or record["severity"] == GL_DEBUG_SEVERITY_HIGH
        ):
            error_count += 1
        elif (
            record["type"] == GL_DEBUG_TYPE_PERFORMANCE
            or record["severity"] == GL_DEBUG_SEVERITY_MEDIUM
        ):
            warning_count += 1

    if len(records) != validation["gl_debug_message_count"]:
        raise ValidationError(
            "validation.gl_debug_message_count disagrees with the JSONL artifact"
        )
    if warning_count != validation["gl_debug_warning_count"]:
        raise ValidationError(
            "validation.gl_debug_warning_count disagrees with the JSONL artifact"
        )
    if error_count != validation["gl_debug_error_count"]:
        raise ValidationError(
            "validation.gl_debug_error_count disagrees with the JSONL artifact"
        )


def validate_acceptance(document: dict[str, Any], root: pathlib.Path) -> None:
    require_fields(
        document,
        (
            "schema_version",
            "fixture_revision",
            "numeric",
            "image_readback",
            "timing",
            "memory",
            "cameras",
            "resolutions",
            "random_seed",
            "legacy_baseline",
            "ddgi",
        ),
        "acceptance",
    )
    if document["schema_version"] != 1:
        raise ValidationError("acceptance.schema_version must be 1")
    fixture = document["fixture_revision"]
    require_fields(fixture, ("gltf", "gltf_sha256", "expectation", "expectation_sha256"), "fixture")
    for path_field, hash_field in (
        ("gltf", "gltf_sha256"),
        ("expectation", "expectation_sha256"),
    ):
        require_sha256(fixture[hash_field], f"fixture.{hash_field}")
        path = (root / fixture[path_field]).resolve()
        if not path.is_file():
            raise ValidationError(f"fixture file is missing: {path}")
        if file_sha256(path) != fixture[hash_field]:
            raise ValidationError(f"fixture hash mismatch: {fixture[path_field]}")

    numeric = document["numeric"]
    for field in ("absolute_tolerance", "relative_tolerance"):
        require_positive(numeric.get(field), f"numeric.{field}", allow_zero=True)
    require_positive(
        document["image_readback"].get("max_bad_pixel_fraction"),
        "image_readback.max_bad_pixel_fraction",
        allow_zero=True,
    )
    if document["image_readback"]["max_bad_pixel_fraction"] > 1:
        raise ValidationError("image_readback.max_bad_pixel_fraction must not exceed 1")

    timing = document["timing"]
    require_fields(
        timing,
        (
            "repetitions",
            "warmup_frames",
            "measured_frames",
            "cold_start_budget_ms",
            "warm_start_budget_ms",
            "gpu_pass_budget_ms",
            "fixed_timestep_seconds",
            "capture_frame_rule",
        ),
        "timing",
    )
    for field in ("repetitions", "measured_frames", "cold_start_budget_ms", "warm_start_budget_ms"):
        require_positive(timing[field], f"timing.{field}")
    require_positive(timing["warmup_frames"], "timing.warmup_frames", allow_zero=True)
    require_positive(timing["fixed_timestep_seconds"], "timing.fixed_timestep_seconds")
    if timing["capture_frame_rule"] != "last_measured_frame":
        raise ValidationError("timing.capture_frame_rule must be last_measured_frame")
    for name, budget in timing["gpu_pass_budget_ms"].items():
        require_positive(budget, f"timing.gpu_pass_budget_ms.{name}")

    for field in ("peak_rss_budget_bytes", "retained_staging_budget_bytes"):
        require_positive(document["memory"].get(field), f"memory.{field}")
    baseline = document["legacy_baseline"]
    require_fields(
        baseline,
        ("feature_flags", "expected_readbacks", "diagnostic_min_dynamic_range"),
        "legacy_baseline",
    )
    for sample in ("ray-marching", "sponza"):
        names = baseline["expected_readbacks"].get(sample)
        if not isinstance(names, list) or not names or not all(
            isinstance(name, str) and name for name in names
        ):
            raise ValidationError(f"legacy_baseline.expected_readbacks.{sample} is invalid")
        thresholds = baseline["diagnostic_min_dynamic_range"].get(sample)
        if not isinstance(thresholds, dict) or set(thresholds) != set(names):
            raise ValidationError(
                f"legacy_baseline diagnostic ranges do not match {sample} readbacks"
            )
        for name, threshold in thresholds.items():
            require_positive(
                threshold,
                f"legacy_baseline.diagnostic_min_dynamic_range.{sample}.{name}",
                allow_zero=True,
            )
    for camera_name, camera in document["cameras"].items():
        require_fields(
            camera,
            (
                "position",
                "forward",
                "up",
                "vertical_fov_degrees",
                "near_plane",
                "far_plane",
            ),
            f"camera.{camera_name}",
        )
        for vector_name in ("position", "forward", "up"):
            vector = camera[vector_name]
            if not isinstance(vector, list) or len(vector) != 3:
                raise ValidationError(f"camera.{camera_name}.{vector_name} must be a vec3")
        require_positive(camera["near_plane"], f"camera.{camera_name}.near_plane")
        require_positive(camera["far_plane"], f"camera.{camera_name}.far_plane")
        if camera["far_plane"] <= camera["near_plane"]:
            raise ValidationError(f"camera.{camera_name} far plane must exceed near plane")

    ddgi = document["ddgi"]
    require_fields(
        ddgi,
        (
            "ray_hit_agreement",
            "hit_distance_tolerance",
            "irradiance_tolerance",
            "convergence_frame_count",
            "energy_bound",
            "update_budget_ms",
        ),
        "ddgi",
    )
    if not 0 <= ddgi["ray_hit_agreement"] <= 1:
        raise ValidationError("ddgi.ray_hit_agreement must be within [0, 1]")
    require_positive(ddgi["convergence_frame_count"], "ddgi.convergence_frame_count")
    require_positive(ddgi["energy_bound"], "ddgi.energy_bound")
    for preset in ("low", "medium", "high"):
        require_positive(ddgi["update_budget_ms"].get(preset), f"ddgi.update_budget_ms.{preset}")
    require_finite_numbers(document, "acceptance")


def validate_camera(camera: dict[str, Any], context: str) -> None:
    require_fields(
        camera,
        (
            "name",
            "position",
            "forward",
            "up",
            "vertical_fov_degrees",
            "near_plane",
            "far_plane",
        ),
        context,
    )
    for field in ("position", "forward", "up"):
        if not isinstance(camera[field], list) or len(camera[field]) != 3:
            raise ValidationError(f"{context}.{field} must be a vec3")


def validate_benchmark(
    document: dict[str, Any],
    artifact_root: pathlib.Path | None = None,
    acceptance: dict[str, Any] | None = None,
) -> None:
    require_fields(
        document,
        (
            "schema_version",
            "capture_status",
            "captured_at",
            "revision",
            "build",
            "system",
            "gpu",
            "asset",
            "settings",
        ),
        "benchmark",
    )
    if document["schema_version"] != 1:
        raise ValidationError("benchmark.schema_version must be 1")
    status = document["capture_status"]
    if status not in ("complete", "partial", "blocked"):
        raise ValidationError("benchmark.capture_status is invalid")
    require_rfc3339_date_time(document["captured_at"], "benchmark.captured_at")

    revision = document["revision"]
    require_fields(revision, ("git_commit", "dirty"), "revision")
    if not isinstance(revision["git_commit"], str) or not re.fullmatch(
        r"[0-9a-f]{40}", revision["git_commit"]
    ):
        raise ValidationError("revision.git_commit must be a full lowercase Git commit")
    if not isinstance(revision["dirty"], bool):
        raise ValidationError("revision.dirty must be boolean")
    require_fields(document["build"], ("type", "compiler", "command"), "build")
    require_fields(document["system"], ("os", "cpu", "ram_bytes"), "system")
    require_positive(document["system"]["ram_bytes"], "system.ram_bytes")
    require_fields(document["gpu"], ("name", "driver", "api"), "gpu")
    require_fields(
        document["asset"],
        ("id", "profile", "entrypoint_sha256", "closure_sha256", "closure_bytes"),
        "asset",
    )
    require_sha256(document["asset"]["entrypoint_sha256"], "asset.entrypoint_sha256")
    require_sha256(document["asset"]["closure_sha256"], "asset.closure_sha256")
    if document["asset"].get("profile") not in ("ci", "dev", "full", "custom"):
        raise ValidationError("asset.profile is invalid")
    require_positive(document["asset"]["closure_bytes"], "asset.closure_bytes", allow_zero=True)
    require_fields(document["settings"], ("sample", "width", "height", "camera"), "settings")
    require_positive(document["settings"]["width"], "settings.width")
    require_positive(document["settings"]["height"], "settings.height")
    validate_camera(document["settings"]["camera"], "settings.camera")

    if status == "complete":
        require_fields(
            document,
            ("startup", "memory", "frames", "readbacks", "validation"),
            "complete benchmark",
        )
        require_fields(document["startup"], ("total_to_first_frame_ms", "spans"), "startup")
        require_fields(
            document["memory"],
            (
                "peak_rss_bytes",
                "retained_cpu_bytes_after_first_frame",
                "retained_cpu_scope",
            ),
            "memory",
        )
        require_fields(
            document["frames"],
            (
                "raw_cpu_ms",
                "raw_gpu_ms",
                "cpu_median_ms",
                "cpu_p95_ms",
                "gpu_median_ms",
                "gpu_p95_ms",
            ),
            "frames",
        )
        require_fields(
            document["validation"],
            (
                "gl_debug_available",
                "gl_debug_message_count",
                "gl_debug_warning_count",
                "gl_debug_error_count",
                "gl_debug_path",
                "gl_debug_byte_count",
                "gl_debug_sha256",
                "non_finite_value_count",
                "exit_code",
            ),
            "validation",
        )
        require_fields(
            document["settings"],
            ("vsync", "cold_cache", "warmup_frames", "measured_frames"),
            "complete settings",
        )
        if not document["startup"]["spans"] or not document["frames"]["raw_cpu_ms"]:
            raise ValidationError("complete benchmark timing arrays must not be empty")
        require_positive(
            document["startup"]["total_to_first_frame_ms"],
            "startup.total_to_first_frame_ms",
            allow_zero=True,
        )
        require_positive(document["memory"]["peak_rss_bytes"], "memory.peak_rss_bytes")
        require_positive(
            document["memory"]["retained_cpu_bytes_after_first_frame"],
            "memory.retained_cpu_bytes_after_first_frame",
            allow_zero=True,
        )
        if not isinstance(document["memory"]["retained_cpu_scope"], str) or not document[
            "memory"
        ]["retained_cpu_scope"]:
            raise ValidationError("memory.retained_cpu_scope must be nonempty")
        measured_frames = document["settings"]["measured_frames"]
        require_positive(measured_frames, "settings.measured_frames")
        if len(document["frames"]["raw_cpu_ms"]) != measured_frames:
            raise ValidationError("frames.raw_cpu_ms count must equal settings.measured_frames")
        if len(document["frames"]["raw_gpu_ms"]) != measured_frames:
            raise ValidationError("frames.raw_gpu_ms count must equal settings.measured_frames")
        for index, duration in enumerate(document["frames"]["raw_cpu_ms"]):
            require_positive(duration, f"frames.raw_cpu_ms[{index}]", allow_zero=True)
        for index, duration in enumerate(document["frames"]["raw_gpu_ms"]):
            require_positive(duration, f"frames.raw_gpu_ms[{index}]", allow_zero=True)
        for field in ("cpu_median_ms", "cpu_p95_ms", "gpu_median_ms", "gpu_p95_ms"):
            require_positive(document["frames"][field], f"frames.{field}", allow_zero=True)
        validate_frame_summaries(document["frames"])

        validation = document["validation"]
        if not isinstance(validation["gl_debug_available"], bool):
            raise ValidationError("validation.gl_debug_available must be boolean")
        for field in (
            "gl_debug_message_count",
            "gl_debug_warning_count",
            "gl_debug_error_count",
            "gl_debug_byte_count",
        ):
            require_positive(validation[field], f"validation.{field}", allow_zero=True)
        if validation["gl_debug_warning_count"] > validation["gl_debug_message_count"]:
            raise ValidationError(
                "validation.gl_debug_warning_count exceeds message count"
            )
        if validation["gl_debug_error_count"] > validation["gl_debug_message_count"]:
            raise ValidationError(
                "validation.gl_debug_error_count exceeds message count"
            )
        require_sha256(validation["gl_debug_sha256"], "validation.gl_debug_sha256")
        if validation["gl_debug_error_count"] != 0:
            raise ValidationError("complete benchmark contains OpenGL debug errors")
        if validation["non_finite_value_count"] != 0:
            raise ValidationError("complete benchmark contains non-finite values")
        if validation["exit_code"] != 0:
            raise ValidationError("complete benchmark has a nonzero exit code")

        readbacks = document["readbacks"]
        if not isinstance(readbacks, list) or not readbacks:
            raise ValidationError("complete benchmark requires readbacks")
        expected_frame = (
            document["settings"]["warmup_frames"]
            + document["settings"]["measured_frames"]
            - 1
        )
        names: list[str] = []
        for index, readback in enumerate(readbacks):
            context = f"readbacks[{index}]"
            require_fields(
                readback,
                (
                    "name",
                    "frame_index",
                    "width",
                    "height",
                    "channels",
                    "component_type",
                    "raw_path",
                    "sha256",
                    "byte_count",
                    "finite_count",
                    "non_finite_count",
                    "channel_statistics",
                ),
                context,
            )
            names.append(readback["name"])
            if readback["frame_index"] != expected_frame:
                raise ValidationError(f"{context}.frame_index is not the final measured frame")
            for field in ("width", "height", "channels", "byte_count"):
                require_positive(readback[field], f"{context}.{field}")
            component_bytes = {"uint8": 1, "float32": 4}.get(readback["component_type"])
            if component_bytes is None:
                raise ValidationError(f"{context}.component_type is invalid")
            expected_bytes = (
                readback["width"]
                * readback["height"]
                * readback["channels"]
                * component_bytes
            )
            if readback["byte_count"] != expected_bytes:
                raise ValidationError(f"{context}.byte_count does not match its format")
            value_count = readback["width"] * readback["height"] * readback["channels"]
            if readback["finite_count"] + readback["non_finite_count"] != value_count:
                raise ValidationError(f"{context} finite/non-finite counts do not cover all values")
            if readback["non_finite_count"] != 0:
                raise ValidationError(f"{context} contains non-finite values")
            if len(readback["channel_statistics"]) != readback["channels"]:
                raise ValidationError(f"{context}.channel_statistics count is wrong")
            require_sha256(readback["sha256"], f"{context}.sha256")
            if artifact_root is not None:
                relative = pathlib.PurePosixPath(readback["raw_path"])
                if relative.is_absolute() or ".." in relative.parts:
                    raise ValidationError(f"{context}.raw_path escapes the artifact root")
                root = artifact_root.resolve()
                path = (root / pathlib.Path(*relative.parts)).resolve()
                if path != root and root not in path.parents:
                    raise ValidationError(f"{context}.raw_path escapes the artifact root")
                if not path.is_file():
                    raise ValidationError(f"{context} artifact is missing: {path}")
                if path.stat().st_size != readback["byte_count"]:
                    raise ValidationError(f"{context} artifact size mismatch")
                if file_sha256(path) != readback["sha256"]:
                    raise ValidationError(f"{context} artifact SHA-256 mismatch")
                validate_readback_payload(path, readback, context)
        if len(names) != len(set(names)):
            raise ValidationError("readback names must be unique")
        if acceptance is not None:
            expected_names = acceptance["legacy_baseline"]["expected_readbacks"].get(
                document["settings"]["sample"]
            )
            if expected_names is not None and set(names) != set(expected_names):
                raise ValidationError("readback names do not match acceptance configuration")
            thresholds = acceptance["legacy_baseline"][
                "diagnostic_min_dynamic_range"
            ].get(document["settings"]["sample"], {})
            checks = {
                check["readback"]: check
                for check in validation.get("semantic_checks", [])
            }
            if set(checks) != set(thresholds):
                raise ValidationError("semantic checks do not match acceptance configuration")
            by_name = {readback["name"]: readback for readback in readbacks}
            for name, threshold in thresholds.items():
                check = checks[name]
                observed = max(
                    channel["max"] - channel["min"]
                    for channel in by_name[name]["channel_statistics"]
                )
                status = "pass" if observed >= threshold else "fail"
                if not math.isclose(
                    check["observed_dynamic_range"],
                    observed,
                    rel_tol=1e-9,
                    abs_tol=1e-9,
                ):
                    raise ValidationError(f"semantic check {name} has wrong observed range")
                if check["minimum_dynamic_range"] != threshold or check["status"] != status:
                    raise ValidationError(f"semantic check {name} disagrees with acceptance")
                if check["required_for_capture"] and status != "pass":
                    raise ValidationError(f"required semantic check {name} failed")

        if artifact_root is not None:
            relative = pathlib.PurePosixPath(validation["gl_debug_path"])
            if relative.is_absolute() or ".." in relative.parts:
                raise ValidationError("validation.gl_debug_path escapes the artifact root")
            root = artifact_root.resolve()
            path = (root / pathlib.Path(*relative.parts)).resolve()
            if path != root and root not in path.parents:
                raise ValidationError("validation.gl_debug_path escapes the artifact root")
            if not path.is_file():
                raise ValidationError("validation.gl_debug_path is missing")
            validate_gl_debug_payload(path, validation)
    else:
        blockers = document.get("blockers")
        if not isinstance(blockers, list) or not blockers or not all(
            isinstance(item, str) and item for item in blockers
        ):
            raise ValidationError("partial/blocked benchmark requires nonempty blockers")
    require_finite_numbers(document, "benchmark")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("schema", type=pathlib.Path)
    parser.add_argument("benchmarks", type=pathlib.Path, nargs="+")
    parser.add_argument("--acceptance", type=pathlib.Path, default=DEFAULT_ACCEPTANCE)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        schema = read_object(args.schema.resolve())
        if schema.get("$schema") != "https://json-schema.org/draft/2020-12/schema":
            raise ValidationError("benchmark schema must declare Draft 2020-12")
        if schema.get("$id") != "https://mini-engine.local/schemas/startup-benchmark.schema.json":
            raise ValidationError("supplied schema is not the benchmark schema")
        acceptance = read_object(args.acceptance.resolve())
        validate_acceptance(acceptance, REPOSITORY_ROOT)
        for path in args.benchmarks:
            document = read_object(path.resolve())
            validate_schema_instance(document, schema, schema, "benchmark")
            validate_benchmark(document, path.resolve().parent, acceptance)
        print(
            json.dumps(
                {
                    "status": "pass",
                    "schema": str(args.schema),
                    "acceptance": str(args.acceptance),
                    "benchmarks": [str(path) for path in args.benchmarks],
                },
                indent=2,
            )
        )
        return 0
    except ValidationError as error:
        print(json.dumps({"status": "fail", "error": str(error)}, indent=2))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
