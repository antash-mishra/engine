"""Focused tests for deterministic glTF and asset verification contracts."""

from __future__ import annotations

import base64
import json
import pathlib
import stat
import struct
import subprocess
import sys
import tempfile
import unittest
import zipfile


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
from tools.assets import archive_install, asset_common, gltf_validation


class ManageAssetsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.fixture = REPOSITORY_ROOT / "tests" / "fixtures" / "gltf" / "phase0_scene.gltf"
        self.expected = self.fixture.with_name("phase0_scene.expected.json")

    def write_document(
        self, directory: str, name: str, document: dict[str, object]
    ) -> pathlib.Path:
        path = pathlib.Path(directory) / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def make_zip_asset(
        self, archive_path: pathlib.Path, *, max_file_bytes: int = 1024
    ) -> dict[str, object]:
        with zipfile.ZipFile(archive_path) as package:
            members = package.infolist()
        file_members = [member for member in members if not member.is_dir()]
        sha256, md5 = archive_install.archive_digests(archive_path)
        return {
            "id": "synthetic",
            "archive": {
                "filename": archive_path.name,
                "size": archive_path.stat().st_size,
                "sha256": sha256,
                "etag_md5": md5,
                "entry_count": len(members),
                "file_count": len(file_members),
                "uncompressed_bytes": sum(member.file_size for member in file_members),
                "limits": {
                    "max_entries": 16,
                    "max_uncompressed_bytes": 4096,
                    "max_file_bytes": max_file_bytes,
                    "max_compression_ratio": 1000,
                },
            },
        }

    def test_fixture_matches_independent_expected_contract(self) -> None:
        result = gltf_validation.verify_gltf(self.fixture, self.expected)
        self.assertEqual("pass", result["status"])
        self.assertEqual([10.0, 2.0, 0.0], result["summary"]["world_position_bounds"]["min"])
        self.assertEqual(0.2, result["summary"]["material_0"]["metallic_factor"])
        self.assertEqual("directional", result["summary"]["light_0"]["type"])
        self.assertEqual([0, 1, 2], result["summary"]["geometry_0"]["indices"])
        self.assertEqual(
            [[0.0, 0.0, 0.0], [1.0, 0.0, 0.0], [0.0, 1.0, 0.0]],
            result["summary"]["geometry_0"]["positions"],
        )
        self.assertEqual(
            [[10.0, 4.0, 1.0]], result["summary"]["light_node_world_translations"]
        )

    def test_fixture_bounds_ignore_untrusted_accessor_metadata(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["accessors"][0]["min"] = [-500.0, -500.0, -500.0]
        document["accessors"][0]["max"] = [500.0, 500.0, 500.0]
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "wrong-metadata.gltf", document)
            result = gltf_validation.verify_gltf(path, self.expected)
        self.assertEqual(
            {"min": [10.0, 2.0, 0.0], "max": [11.0, 3.0, 0.0]},
            result["summary"]["world_position_bounds"],
        )

    def test_fixture_position_payload_corruption_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        header, payload = document["buffers"][0]["uri"].split(",", 1)
        decoded = bytearray(base64.b64decode(payload))
        decoded[0:36] = b"\0" * 36
        document["buffers"][0]["uri"] = header + "," + base64.b64encode(decoded).decode("ascii")
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "corrupt-position.gltf", document)
            with self.assertRaisesRegex(asset_common.VerificationError, "geometry_0.positions"):
                gltf_validation.verify_gltf(path, self.expected)

    def test_fixture_out_of_range_index_payload_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        header, payload = document["buffers"][0]["uri"].split(",", 1)
        decoded = bytearray(base64.b64decode(payload))
        decoded[96:102] = struct.pack("<3H", 0, 1, 65535)
        document["buffers"][0]["uri"] = header + "," + base64.b64encode(decoded).decode("ascii")
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "corrupt-index.gltf", document)
            with self.assertRaisesRegex(asset_common.VerificationError, "missing position"):
                gltf_validation.verify_gltf(path)

    def test_out_of_range_accessor_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["accessors"][0]["count"] = 4
        with tempfile.TemporaryDirectory() as directory:
            corrupt = pathlib.Path(directory) / "corrupt.gltf"
            corrupt.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(asset_common.VerificationError, "exceeds bufferView"):
                gltf_validation.verify_gltf(corrupt)

    def test_missing_external_reference_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["buffers"][0]["uri"] = "missing.bin"
        with tempfile.TemporaryDirectory() as directory:
            corrupt = pathlib.Path(directory) / "missing-reference.gltf"
            corrupt.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(asset_common.VerificationError, "cannot read buffer"):
                gltf_validation.verify_gltf(corrupt)

    def test_closure_checksum_changes_when_payload_changes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            payload = root / "payload.bin"
            payload.write_bytes(b"expected")
            before = gltf_validation.closure_fingerprint(
                root, [pathlib.PurePosixPath("payload.bin")]
            )
            payload.write_bytes(b"corrupt!")
            after = gltf_validation.closure_fingerprint(
                root, [pathlib.PurePosixPath("payload.bin")]
            )
            self.assertNotEqual(before["tree_sha256"], after["tree_sha256"])
            with self.assertRaisesRegex(asset_common.VerificationError, "tree_sha256 mismatch"):
                asset_common.compare_fields(after, before)

    def test_node_cycle_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["nodes"][1]["children"] = [0]
        with tempfile.TemporaryDirectory() as directory:
            corrupt = pathlib.Path(directory) / "cyclic.gltf"
            corrupt.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(asset_common.VerificationError, "cycle"):
                gltf_validation.verify_gltf(corrupt)

    def test_malformed_top_level_container_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["nodes"] = {}
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "bad-container.gltf", document)
            with self.assertRaisesRegex(asset_common.VerificationError, "nodes must be an array"):
                gltf_validation.verify_gltf(path)

    def test_malformed_container_element_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["buffers"] = ["not-an-object"]
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "bad-element.gltf", document)
            with self.assertRaisesRegex(
                asset_common.VerificationError, r"buffers\[0\] must be an object"
            ):
                gltf_validation.verify_gltf(path)

    def test_cli_reports_malformed_container_without_traceback(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["buffers"] = ["not-an-object"]
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "bad-cli.gltf", document)
            result = subprocess.run(
                [
                    sys.executable,
                    str(REPOSITORY_ROOT / "tools" / "assets" / "manage_assets.py"),
                    "verify-gltf",
                    str(path),
                ],
                cwd=REPOSITORY_ROOT,
                check=False,
                capture_output=True,
                text=True,
            )
        self.assertEqual(1, result.returncode)
        self.assertEqual("fail", json.loads(result.stdout)["status"])
        self.assertNotIn("Traceback", result.stderr)

    def test_buffer_view_cannot_exceed_declared_buffer_length(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["buffers"][0]["byteLength"] = 100
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "declared-buffer.gltf", document)
            with self.assertRaisesRegex(
                asset_common.VerificationError, "exceeds declared byteLength"
            ):
                gltf_validation.verify_gltf(path)

    def test_invalid_sparse_reference_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["accessors"][0]["sparse"] = {
            "count": 1,
            "indices": {"bufferView": 99, "componentType": 5121},
            "values": {"bufferView": 1},
        }
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "bad-sparse.gltf", document)
            with self.assertRaisesRegex(
                asset_common.VerificationError, "sparse.indices.bufferView"
            ):
                gltf_validation.verify_gltf(path)

    def test_sparse_accessor_without_base_buffer_decodes_override(self) -> None:
        payload = struct.pack("<B3f", 1, 2.0, 3.0, 4.0)
        document = {
            "asset": {"version": "2.0"},
            "scene": 0,
            "scenes": [{"nodes": []}],
            "buffers": [
                {
                    "byteLength": len(payload),
                    "uri": "data:application/octet-stream;base64,"
                    + base64.b64encode(payload).decode("ascii"),
                }
            ],
            "bufferViews": [
                {"buffer": 0, "byteOffset": 0, "byteLength": 1},
                {"buffer": 0, "byteOffset": 1, "byteLength": 12},
            ],
            "accessors": [
                {
                    "componentType": 5126,
                    "count": 3,
                    "type": "VEC3",
                    "sparse": {
                        "count": 1,
                        "indices": {"bufferView": 0, "componentType": 5121},
                        "values": {"bufferView": 1},
                    },
                }
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "sparse.gltf", document)
            gltf_validation.validate_references(document, path)
            decoded = gltf_validation.decode_accessor(document, path, 0)
        self.assertEqual([[0, 0, 0], [2.0, 3.0, 4.0], [0, 0, 0]], decoded)

    def test_invalid_khr_light_index_is_detected(self) -> None:
        document = json.loads(self.fixture.read_text(encoding="utf-8"))
        document["nodes"][2]["extensions"]["KHR_lights_punctual"]["light"] = 1
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_document(directory, "bad-light.gltf", document)
            with self.assertRaisesRegex(
                asset_common.VerificationError, "KHR_lights_punctual.light"
            ):
                gltf_validation.verify_gltf(path)

    def test_uri_traversal_is_detected(self) -> None:
        with self.assertRaisesRegex(asset_common.VerificationError, "unsafe relative"):
            asset_common.safe_relative_uri("../outside.bin")

    def test_fetch_requires_license_acceptance_before_network(self) -> None:
        manifest = asset_common.read_json(REPOSITORY_ROOT / "assets" / "manifest.json")
        asset = asset_common.load_asset(manifest, "intel-sponza-base")
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(asset_common.VerificationError, "accept-license"):
                archive_install.fetch_archive(asset, pathlib.Path(directory), accept_license=False)

    def test_malformed_manifest_reports_a_contract_error(self) -> None:
        manifest = {"schema_version": 1, "assets": [{"id": "broken"}]}
        with self.assertRaisesRegex(asset_common.VerificationError, "missing source.download_url"):
            asset_common.load_asset(manifest, "broken")

    def test_unsafe_archive_filename_is_rejected(self) -> None:
        manifest = asset_common.read_json(REPOSITORY_ROOT / "assets" / "manifest.json")
        manifest["assets"][0]["archive"]["filename"] = "../main_sponza.zip"
        with self.assertRaisesRegex(asset_common.VerificationError, "safe basename"):
            asset_common.load_asset(manifest, "intel-sponza-base")

    def test_drive_like_archive_filename_is_rejected(self) -> None:
        manifest = asset_common.read_json(REPOSITORY_ROOT / "assets" / "manifest.json")
        manifest["assets"][0]["archive"]["filename"] = "C:main_sponza.zip"
        with self.assertRaisesRegex(asset_common.VerificationError, "safe basename"):
            asset_common.load_asset(manifest, "intel-sponza-base")

    def test_full_profile_must_match_closure_contract(self) -> None:
        manifest = asset_common.read_json(REPOSITORY_ROOT / "assets" / "manifest.json")
        manifest["assets"][0]["profiles"]["full"]["total_bytes"] += 1
        with self.assertRaisesRegex(asset_common.VerificationError, "profiles.full.total_bytes"):
            asset_common.load_asset(manifest, "intel-sponza-base")

    def test_zip_path_traversal_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "traversal.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("../outside.txt", b"bad")
            asset = self.make_zip_asset(archive)
            with self.assertRaisesRegex(asset_common.VerificationError, "unsafe ZIP member"):
                archive_install.inspect_zip(asset, archive)

    def test_zip_symbolic_link_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "symlink.zip"
            member = zipfile.ZipInfo("main_sponza/link")
            member.create_system = 3
            member.external_attr = (stat.S_IFLNK | 0o777) << 16
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr(member, "target")
            asset = self.make_zip_asset(archive)
            with self.assertRaisesRegex(asset_common.VerificationError, "symbolic link"):
                archive_install.inspect_zip(asset, archive)

    def test_zip_member_size_limit_is_enforced(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "oversize.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("main_sponza/file.bin", b"1234")
            asset = self.make_zip_asset(archive, max_file_bytes=3)
            with self.assertRaisesRegex(asset_common.VerificationError, "per-file size limit"):
                archive_install.inspect_zip(asset, archive)

    def test_archive_checksum_corruption_is_detected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "checksum.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("main_sponza/file.bin", b"original")
            asset = self.make_zip_asset(archive)
            with archive.open("ab") as stream:
                stream.write(b"corruption")
            asset["archive"]["size"] = archive.stat().st_size
            with self.assertRaisesRegex(asset_common.VerificationError, "SHA-256 mismatch"):
                archive_install.verify_archive(asset, archive)

    def test_symbolic_link_install_root_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = pathlib.Path(directory)
            archive = parent / "asset.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("main_sponza/file.bin", b"data")
            asset = self.make_zip_asset(archive)
            target = parent / "target"
            target.mkdir()
            link = parent / "install-link"
            link.symlink_to(target, target_is_directory=True)
            with self.assertRaisesRegex(asset_common.VerificationError, "symbolic link"):
                archive_install.install_archive(asset, archive, link, accept_license=True)

    def test_failed_atomic_install_leaves_empty_root_and_no_staging_directory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            parent = pathlib.Path(directory)
            archive = parent / "incomplete.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("main_sponza/not-the-entrypoint.txt", b"content")
            asset = self.make_zip_asset(archive)
            asset.update(
                {
                    "local": {
                        "entrypoint": "main_sponza/missing.gltf",
                        "entrypoint_sha256": "0" * 64,
                        "primary_buffer": "main_sponza/missing.bin",
                        "primary_buffer_sha256": "0" * 64,
                    }
                }
            )
            root = parent / "install-root"
            root.mkdir()
            with self.assertRaisesRegex(asset_common.VerificationError, "entrypoint is missing"):
                archive_install.install_archive(asset, archive, root, accept_license=True)
            self.assertEqual([], list(root.iterdir()))
            self.assertEqual([], list(parent.glob(".install-root.install-*")))


if __name__ == "__main__":
    unittest.main()
