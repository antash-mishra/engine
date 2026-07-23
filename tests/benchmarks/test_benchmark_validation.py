"""Positive and negative tests for benchmark and acceptance contracts."""

from __future__ import annotations

import copy
import contextlib
import hashlib
import importlib.util
import io
import json
import pathlib
import struct
import tempfile
import unittest


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL_PATH = REPOSITORY_ROOT / "tools" / "benchmarks" / "validate_benchmark.py"
SPEC = importlib.util.spec_from_file_location("validate_benchmark", TOOL_PATH)
assert SPEC and SPEC.loader
validate_benchmark = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validate_benchmark)


class BenchmarkValidationTests(unittest.TestCase):
    def setUp(self) -> None:
        self.partial_path = (
            REPOSITORY_ROOT
            / "artifacts"
            / "verification"
            / "phase-0"
            / "sponza-legacy-loader.partial.json"
        )
        self.partial = json.loads(self.partial_path.read_text(encoding="utf-8"))

    def test_partial_diagnostic_is_schema_compatible_and_explicitly_blocked(self) -> None:
        validate_benchmark.validate_benchmark(self.partial)
        self.assertEqual("partial", self.partial["capture_status"])
        self.assertGreater(len(self.partial["blockers"]), 0)

    def test_complete_capture_cannot_omit_frame_and_validation_data(self) -> None:
        corrupt = copy.deepcopy(self.partial)
        corrupt["capture_status"] = "complete"
        with self.assertRaisesRegex(validate_benchmark.ValidationError, "missing required fields"):
            validate_benchmark.validate_benchmark(corrupt)

    def test_invalid_asset_hash_is_rejected(self) -> None:
        corrupt = copy.deepcopy(self.partial)
        corrupt["asset"]["closure_sha256"] = "not-a-hash"
        with self.assertRaisesRegex(validate_benchmark.ValidationError, "lowercase SHA-256"):
            validate_benchmark.validate_benchmark(corrupt)

    def test_acceptance_fixture_hashes_and_parameters_validate(self) -> None:
        acceptance = validate_benchmark.read_object(
            REPOSITORY_ROOT / "validation" / "acceptance.json"
        )
        validate_benchmark.validate_acceptance(acceptance, REPOSITORY_ROOT)

    def test_tampered_acceptance_fixture_hash_is_rejected(self) -> None:
        acceptance = validate_benchmark.read_object(
            REPOSITORY_ROOT / "validation" / "acceptance.json"
        )
        acceptance["fixture_revision"]["gltf_sha256"] = "0" * 64
        with self.assertRaisesRegex(validate_benchmark.ValidationError, "fixture hash mismatch"):
            validate_benchmark.validate_acceptance(acceptance, REPOSITORY_ROOT)

    def make_complete(self, root: pathlib.Path) -> dict:
        raw = bytes((1, 2, 3, 4))
        (root / "readback.bin").write_bytes(raw)
        (root / "gl-debug.jsonl").write_text("", encoding="utf-8")
        camera = {
            "name": "ray_marching_default",
            "position": [0.0, 0.0, 5.0],
            "forward": [0.0, 0.0, -1.0],
            "up": [0.0, 1.0, 0.0],
            "vertical_fov_degrees": 35.0,
            "near_plane": 0.1,
            "far_plane": 100.0,
        }
        return {
            "schema_version": 1,
            "capture_status": "complete",
            "captured_at": "2026-07-23T00:00:00Z",
            "revision": {"git_commit": "0" * 40, "dirty": True},
            "build": {"type": "test", "compiler": "test", "command": "test"},
            "system": {"os": "test", "cpu": "test", "ram_bytes": 1},
            "gpu": {"name": "test", "driver": "test", "api": "OpenGL 4.3"},
            "asset": {
                "id": "test",
                "profile": "custom",
                "entrypoint_sha256": "0" * 64,
                "closure_sha256": "0" * 64,
                "closure_bytes": 0,
            },
            "settings": {
                "sample": "ray-marching",
                "width": 1,
                "height": 1,
                "vsync": False,
                "cold_cache": False,
                "warmup_frames": 0,
                "measured_frames": 1,
                "camera": camera,
                "seed": 1,
                "fixed_timestep_seconds": 1 / 60,
            },
            "startup": {
                "total_to_first_frame_ms": 1.0,
                "spans": [{"name": "test", "duration_ms": 1.0, "thread": "main"}],
            },
            "memory": {
                "peak_rss_bytes": 1,
                "retained_cpu_bytes_after_first_frame": 0,
                "retained_cpu_scope": "test fixture",
            },
            "frames": {
                "raw_cpu_ms": [1.0],
                "raw_gpu_ms": [0.5],
                "cpu_median_ms": 1.0,
                "cpu_p95_ms": 1.0,
                "gpu_median_ms": 0.5,
                "gpu_p95_ms": 0.5,
            },
            "readbacks": [
                {
                    "name": "final_color",
                    "source": "default_back_buffer",
                    "frame_index": 0,
                    "width": 1,
                    "height": 1,
                    "channels": 4,
                    "component_type": "uint8",
                    "byte_order": "little",
                    "row_order": "bottom_to_top",
                    "data_space": "display_encoded",
                    "raw_path": "readback.bin",
                    "sha256": hashlib.sha256(raw).hexdigest(),
                    "byte_count": len(raw),
                    "finite_count": 4,
                    "non_finite_count": 0,
                    "channel_statistics": [
                        {"min": 1, "max": 1, "mean": 1},
                        {"min": 2, "max": 2, "mean": 2},
                        {"min": 3, "max": 3, "mean": 3},
                        {"min": 4, "max": 4, "mean": 4},
                    ],
                }
            ],
            "validation": {
                "gl_debug_error_count": 0,
                "gl_debug_message_count": 0,
                "gl_debug_warning_count": 0,
                "non_finite_value_count": 0,
                "exit_code": 0,
                "gl_debug_available": True,
                "gl_debug_path": "gl-debug.jsonl",
                "gl_debug_byte_count": 0,
                "gl_debug_sha256": hashlib.sha256(b"").hexdigest(),
                "semantic_checks": [
                    {
                        "name": "final_color_nonuniform",
                        "readback": "final_color",
                        "status": "fail",
                        "required_for_capture": False,
                        "observed_dynamic_range": 0,
                        "minimum_dynamic_range": 1,
                    }
                ],
            },
        }

    def test_complete_record_and_artifacts_validate(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            schema = validate_benchmark.read_object(
                REPOSITORY_ROOT / "validation/schemas/benchmark.schema.json"
            )
            acceptance = validate_benchmark.read_object(
                REPOSITORY_ROOT / "validation/acceptance.json"
            )
            validate_benchmark.validate_schema_instance(document, schema, schema, "benchmark")
            validate_benchmark.validate_benchmark(document, root, acceptance)

    def test_complete_record_failure_signals_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            for field in ("gl_debug_error_count", "non_finite_value_count", "exit_code"):
                document = self.make_complete(root)
                document["validation"][field] = 1
                with self.assertRaises(validate_benchmark.ValidationError):
                    validate_benchmark.validate_benchmark(document, root)

    def test_complete_record_requires_exact_sample_and_capture_frame_counts(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            document["settings"]["measured_frames"] = 2
            with self.assertRaisesRegex(validate_benchmark.ValidationError, "raw_cpu_ms count"):
                validate_benchmark.validate_benchmark(document, root)
            document = self.make_complete(root)
            document["readbacks"][0]["frame_index"] = 1
            with self.assertRaisesRegex(validate_benchmark.ValidationError, "final measured frame"):
                validate_benchmark.validate_benchmark(document, root)
            document = self.make_complete(root)
            document["frames"]["raw_gpu_ms"] = []
            with self.assertRaisesRegex(validate_benchmark.ValidationError, "raw_gpu_ms count"):
                validate_benchmark.validate_benchmark(document, root)

    def test_all_frame_summaries_are_recomputed_with_nearest_rank_p95(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            cpu = [float(value) for value in range(1, 21)]
            gpu = [value * 0.5 for value in cpu]
            document["settings"]["measured_frames"] = 20
            document["readbacks"][0]["frame_index"] = 19
            document["frames"] = {
                "raw_cpu_ms": cpu,
                "raw_gpu_ms": gpu,
                "cpu_median_ms": 10.5,
                "cpu_p95_ms": 19.0,
                "gpu_median_ms": 5.25,
                "gpu_p95_ms": 9.5,
            }
            validate_benchmark.validate_benchmark(document, root)

            for field in (
                "cpu_median_ms",
                "cpu_p95_ms",
                "gpu_median_ms",
                "gpu_p95_ms",
            ):
                with self.subTest(field=field):
                    corrupt = copy.deepcopy(document)
                    corrupt["frames"][field] += 0.25
                    with self.assertRaisesRegex(
                        validate_benchmark.ValidationError, field
                    ):
                        validate_benchmark.validate_benchmark(corrupt, root)

    def test_date_only_capture_timestamp_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            document["captured_at"] = "2026-07-23"
            schema = validate_benchmark.read_object(
                REPOSITORY_ROOT / "validation/schemas/benchmark.schema.json"
            )
            with self.assertRaisesRegex(
                validate_benchmark.ValidationError, "RFC 3339 date-time"
            ):
                validate_benchmark.validate_schema_instance(
                    document, schema, schema, "benchmark"
                )
            with self.assertRaisesRegex(
                validate_benchmark.ValidationError, "RFC 3339 date-time"
            ):
                validate_benchmark.validate_benchmark(document, root)

    def test_artifact_escape_hash_size_and_statistics_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            corruptions = (
                ("raw_path", "../readback.bin", "escapes"),
                ("sha256", "f" * 64, "SHA-256 mismatch"),
                ("byte_count", 8, "byte_count"),
            )
            for field, value, message in corruptions:
                document = self.make_complete(root)
                document["readbacks"][0][field] = value
                with self.assertRaisesRegex(validate_benchmark.ValidationError, message):
                    validate_benchmark.validate_benchmark(document, root)
            document = self.make_complete(root)
            document["readbacks"][0]["channel_statistics"][0]["mean"] = 9
            with self.assertRaisesRegex(validate_benchmark.ValidationError, "disagrees"):
                validate_benchmark.validate_benchmark(document, root)

    def test_raw_float_nan_is_detected_independently_of_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            raw = struct.pack("<f", float("nan"))
            (root / "readback.bin").write_bytes(raw)
            readback = document["readbacks"][0]
            readback.update(
                {
                    "channels": 1,
                    "component_type": "float32",
                    "sha256": hashlib.sha256(raw).hexdigest(),
                    "byte_count": 4,
                    "finite_count": 1,
                    "non_finite_count": 0,
                    "channel_statistics": [{"min": 0, "max": 0, "mean": 0}],
                }
            )
            with self.assertRaisesRegex(validate_benchmark.ValidationError, "non-finite count"):
                validate_benchmark.validate_benchmark(document, root)

    def test_gl_debug_artifact_tamper_and_count_mismatches_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            document = self.make_complete(root)
            (root / "gl-debug.jsonl").write_text("tampered\n", encoding="utf-8")
            with self.assertRaisesRegex(
                validate_benchmark.ValidationError, "size mismatch"
            ):
                validate_benchmark.validate_benchmark(document, root)

            document = self.make_complete(root)
            document["validation"]["gl_debug_message_count"] = 1
            with self.assertRaisesRegex(
                validate_benchmark.ValidationError, "message_count disagrees"
            ):
                validate_benchmark.validate_benchmark(document, root)

            warning = {
                "source": 0x8246,
                "type": validate_benchmark.GL_DEBUG_TYPE_PERFORMANCE,
                "id": 1,
                "severity": 0x9148,
                "message": "performance warning",
            }
            payload = (json.dumps(warning) + "\n").encode()
            (root / "gl-debug.jsonl").write_bytes(payload)
            document = self.make_complete(root)
            (root / "gl-debug.jsonl").write_bytes(payload)
            document["validation"].update(
                {
                    "gl_debug_message_count": 1,
                    "gl_debug_byte_count": len(payload),
                    "gl_debug_sha256": hashlib.sha256(payload).hexdigest(),
                }
            )
            with self.assertRaisesRegex(
                validate_benchmark.ValidationError, "warning_count disagrees"
            ):
                validate_benchmark.validate_benchmark(document, root)

    def test_gl_debug_classification_matches_the_cpp_producer(self) -> None:
        records = [
            {
                "source": 0x8246,
                "type": validate_benchmark.GL_DEBUG_TYPE_ERROR,
                "id": 1,
                "severity": 0x9148,
                "message": "error by type",
            },
            {
                "source": 0x8246,
                "type": 0x8251,
                "id": 2,
                "severity": validate_benchmark.GL_DEBUG_SEVERITY_HIGH,
                "message": "error by severity",
            },
            {
                "source": 0x8246,
                "type": validate_benchmark.GL_DEBUG_TYPE_PERFORMANCE,
                "id": 3,
                "severity": 0x9148,
                "message": "warning by type",
            },
            {
                "source": 0x8246,
                "type": 0x8251,
                "id": 4,
                "severity": validate_benchmark.GL_DEBUG_SEVERITY_MEDIUM,
                "message": "warning by severity",
            },
        ]
        payload = b"".join(
            (json.dumps(record) + "\n").encode() for record in records
        )
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "gl-debug.jsonl"
            path.write_bytes(payload)
            validate_benchmark.validate_gl_debug_payload(
                path,
                {
                    "gl_debug_byte_count": len(payload),
                    "gl_debug_sha256": hashlib.sha256(payload).hexdigest(),
                    "gl_debug_message_count": 4,
                    "gl_debug_warning_count": 2,
                    "gl_debug_error_count": 2,
                },
            )

    def test_schema_rejects_forbidden_fields_and_invalid_ranges(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            schema = validate_benchmark.read_object(
                REPOSITORY_ROOT / "validation/schemas/benchmark.schema.json"
            )
            for mutate in (
                lambda value: value.update({"forbidden": True}),
                lambda value: value["asset"].update({"profile": "invalid"}),
                lambda value: value["settings"].update({"width": -1}),
                lambda value: value["system"].update({"ram_bytes": -1}),
                lambda value: value["startup"].update({"total_to_first_frame_ms": -1}),
            ):
                document = self.make_complete(root)
                mutate(document)
                with self.assertRaises(validate_benchmark.ValidationError):
                    validate_benchmark.validate_schema_instance(
                        document, schema, schema, "benchmark"
                    )

    def test_cli_rejects_an_unrelated_schema(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            schema = root / "schema.json"
            schema.write_text(
                json.dumps(
                    {
                        "$schema": "https://json-schema.org/draft/2020-12/schema",
                        "$id": "https://example.invalid/unrelated",
                        "type": "object",
                    }
                ),
                encoding="utf-8",
            )
            benchmark = root / "benchmark.json"
            benchmark.write_text(json.dumps(self.partial), encoding="utf-8")
            with contextlib.redirect_stdout(io.StringIO()):
                result = validate_benchmark.main([str(schema), str(benchmark)])
            self.assertEqual(1, result)


if __name__ == "__main__":
    unittest.main()
