"""Orchestrator failure-path tests that do not require a GPU context."""

from __future__ import annotations

import copy
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import tempfile
import unittest
from unittest import mock


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
TOOL_PATH = REPOSITORY_ROOT / "tools/benchmarks/capture_legacy.py"
SPEC = importlib.util.spec_from_file_location("capture_legacy", TOOL_PATH)
assert SPEC and SPEC.loader
capture_legacy = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture_legacy)


class CaptureLegacyTests(unittest.TestCase):
    def test_sponza_asset_description_maps_manifest_root_below_resources(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = pathlib.Path(directory)
            resources = temporary / "resources"
            asset_root = resources / "main-sponza"
            scene = asset_root / "main_sponza/scene.gltf"
            buffer = asset_root / "main_sponza/scene.bin"
            scene.parent.mkdir(parents=True)
            scene.write_text(
                json.dumps(
                    {
                        "asset": {"version": "2.0"},
                        "buffers": [{"uri": "scene.bin", "byteLength": 4}],
                    }
                )
                + "\n",
                encoding="utf-8",
            )
            buffer.write_bytes(b"\x01\x02\x03\x04")

            closure_digest = hashlib.sha256()
            for path in sorted((scene, buffer)):
                relative = path.relative_to(asset_root).as_posix()
                size = path.stat().st_size
                payload_hash = hashlib.sha256(path.read_bytes()).hexdigest()
                closure_digest.update(
                    f"{relative}\0{size}\0{payload_hash}\n".encode()
                )
            closure_bytes = scene.stat().st_size + buffer.stat().st_size

            source_manifest = json.loads(
                (REPOSITORY_ROOT / "assets/manifest.json").read_text(encoding="utf-8")
            )
            manifest = copy.deepcopy(source_manifest)
            asset = next(
                item for item in manifest["assets"]
                if item["id"] == "intel-sponza-base"
            )
            asset["archive"]["file_count"] = 2
            asset["archive"]["uncompressed_bytes"] = closure_bytes
            asset["local"].update(
                {
                    "observed_file_count": 2,
                    "observed_unpacked_bytes": closure_bytes,
                    "entrypoint": "main_sponza/scene.gltf",
                    "entrypoint_sha256": hashlib.sha256(scene.read_bytes()).hexdigest(),
                    "primary_buffer": "main_sponza/scene.bin",
                    "primary_buffer_sha256": hashlib.sha256(buffer.read_bytes()).hexdigest(),
                    "required_files": [],
                }
            )
            asset["gltf_closure"].update(
                {
                    "file_count": 2,
                    "total_bytes": closure_bytes,
                    "tree_sha256": closure_digest.hexdigest(),
                }
            )
            asset["profiles"]["full"].update(
                {
                    "entrypoint": asset["local"]["entrypoint"],
                    "file_count": 2,
                    "total_bytes": closure_bytes,
                    "tree_sha256": closure_digest.hexdigest(),
                }
            )
            manifest_path = temporary / "manifest.json"
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

            description = capture_legacy.asset_description(
                "sponza", resources, manifest_path
            )
            self.assertEqual(
                hashlib.sha256(scene.read_bytes()).hexdigest(),
                description["entrypoint_sha256"],
            )
            self.assertEqual(closure_digest.hexdigest(), description["closure_sha256"])
            self.assertEqual(closure_bytes, description["closure_bytes"])

    def test_child_failure_writes_partial_record_and_fresh_logs(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "failed.json"
            result = capture_legacy.main(
                [
                    "--sample",
                    "ray-marching",
                    "--executable",
                    "/bin/false",
                    "--config",
                    str(REPOSITORY_ROOT / "validation/acceptance.json"),
                    "--resource-root",
                    str(REPOSITORY_ROOT / "resources"),
                    "--output",
                    str(output),
                    "--timeout-seconds",
                    "1",
                ]
            )
            self.assertEqual(1, result)
            document = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual("partial", document["capture_status"])
            self.assertTrue(document["blockers"])
            self.assertTrue(
                (output.parent / "failed.artifacts" / "stderr.log").is_file()
            )
            validated = subprocess.run(
                [
                    "python3",
                    str(REPOSITORY_ROOT / "tools/benchmarks/validate_benchmark.py"),
                    str(REPOSITORY_ROOT / "validation/schemas/benchmark.schema.json"),
                    str(output),
                ],
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(0, validated.returncode, validated.stdout + validated.stderr)

    def test_existing_output_is_never_reused(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "existing.json"
            output.write_text("{}\n", encoding="utf-8")
            with self.assertRaisesRegex(capture_legacy.CaptureError, "refusing to overwrite"):
                capture_legacy.main(
                    [
                        "--sample",
                        "ray-marching",
                        "--executable",
                        "/bin/false",
                        "--config",
                        str(REPOSITORY_ROOT / "validation/acceptance.json"),
                        "--resource-root",
                        str(REPOSITORY_ROOT / "resources"),
                        "--output",
                        str(output),
                    ]
                )

    def test_sponza_asset_root_is_verified_before_child_launch(self) -> None:
        manifest = json.loads(
            (REPOSITORY_ROOT / "assets/manifest.json").read_text(encoding="utf-8")
        )
        entrypoint = pathlib.Path(
            next(
                asset for asset in manifest["assets"]
                if asset["id"] == "intel-sponza-base"
            )["local"]["entrypoint"]
        )
        with tempfile.TemporaryDirectory() as directory:
            temporary = pathlib.Path(directory)
            for case in ("empty", "wrong-bytes"):
                with self.subTest(case=case):
                    resource_root = temporary / case
                    resource_root.mkdir()
                    if case == "wrong-bytes":
                        wrong_entrypoint = resource_root / "main-sponza" / entrypoint
                        wrong_entrypoint.parent.mkdir(parents=True)
                        wrong_entrypoint.write_text("{}\n", encoding="utf-8")
                    output = temporary / f"{case}.json"
                    with mock.patch.object(
                        capture_legacy.subprocess,
                        "run",
                        side_effect=AssertionError("renderer child was launched"),
                    ) as child_run:
                        with self.assertRaisesRegex(
                            capture_legacy.CaptureError,
                            "Sponza asset preflight failed",
                        ):
                            capture_legacy.main(
                                [
                                    "--sample",
                                    "sponza",
                                    "--executable",
                                    "/bin/false",
                                    "--config",
                                    str(REPOSITORY_ROOT / "validation/acceptance.json"),
                                    "--resource-root",
                                    str(resource_root),
                                    "--output",
                                    str(output),
                                ]
                            )
                        child_run.assert_not_called()
                    self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
