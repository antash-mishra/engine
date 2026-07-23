"""Shared manifest, hashing, and path-safety contracts for asset tooling."""

from __future__ import annotations

import hashlib
import json
import pathlib
import urllib.parse
from typing import Any


REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = REPOSITORY_ROOT / "assets" / "manifest.json"
HASH_CHUNK_BYTES = 4 * 1024 * 1024
SHA256_HEX_LENGTH = 64
MD5_HEX_LENGTH = 32


class VerificationError(RuntimeError):
    """A deterministic asset input or installation contract failed."""


def read_json(path: pathlib.Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise VerificationError(f"cannot read JSON {path}: {error}") from error
    if not isinstance(value, dict):
        raise VerificationError(f"expected a JSON object in {path}")
    return value


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(HASH_CHUNK_BYTES), b""):
            digest.update(chunk)
    return digest.hexdigest()


def safe_relative_uri(uri: str) -> pathlib.PurePosixPath:
    """Return a normalized local URI or reject external/traversing input."""
    parsed = urllib.parse.urlsplit(uri)
    if parsed.scheme or parsed.netloc or parsed.query or parsed.fragment:
        raise VerificationError(f"external or decorated URI is not allowed: {uri!r}")
    decoded = urllib.parse.unquote(parsed.path)
    if "\\" in decoded:
        raise VerificationError(f"backslashes are not allowed in glTF URIs: {uri!r}")
    path = pathlib.PurePosixPath(decoded)
    if path.is_absolute() or not path.parts or any(part in ("", ".", "..") for part in path.parts):
        raise VerificationError(f"unsafe relative glTF URI: {uri!r}")
    return path


def resolve_under(root: pathlib.Path, relative: pathlib.PurePosixPath) -> pathlib.Path:
    root = root.resolve()
    resolved = (root / pathlib.Path(*relative.parts)).resolve()
    if resolved != root and root not in resolved.parents:
        raise VerificationError(f"path escapes asset root: {relative}")
    return resolved


def require_object(value: Any, context: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise VerificationError(f"{context} must be an object")
    return value


def require_array(value: Any, context: str) -> list[Any]:
    if not isinstance(value, list):
        raise VerificationError(f"{context} must be an array")
    return value


def require_index(value: Any, upper_bound: int, context: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value < upper_bound:
        raise VerificationError(f"{context} references invalid index {value!r}")
    return value


def require_nonnegative_integer(value: Any, context: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise VerificationError(f"{context} must be a nonnegative integer")
    return value


def compare_fields(actual: dict[str, Any], expected: dict[str, Any], prefix: str = "") -> None:
    for key, expected_value in expected.items():
        field = f"{prefix}.{key}" if prefix else key
        if key not in actual:
            raise VerificationError(f"expected field is absent: {field}")
        actual_value = actual[key]
        if isinstance(expected_value, dict):
            if not isinstance(actual_value, dict):
                raise VerificationError(f"{field} is not an object")
            compare_fields(actual_value, expected_value, field)
        elif actual_value != expected_value:
            raise VerificationError(
                f"{field} mismatch: expected {expected_value!r}, got {actual_value!r}"
            )


def require_hex_digest(value: Any, length: int, context: str) -> str:
    if (
        not isinstance(value, str)
        or len(value) != length
        or any(character not in "0123456789abcdef" for character in value)
    ):
        raise VerificationError(f"{context} must be {length} lowercase hexadecimal characters")
    return value


def safe_archive_filename(value: Any) -> str:
    if (
        not isinstance(value, str)
        or not value
        or "\\"
        in value
        or ":"
        in value
        or any(ord(character) < 32 for character in value)
    ):
        raise VerificationError("archive.filename must be a safe basename")
    path = pathlib.PurePosixPath(value)
    if path.is_absolute() or len(path.parts) != 1 or path.name in (".", ".."):
        raise VerificationError("archive.filename must be a safe basename")
    if path.suffix.lower() != ".zip":
        raise VerificationError("archive.filename must identify a ZIP archive")
    return value


def load_asset(manifest: dict[str, Any], asset_id: str) -> dict[str, Any]:
    """Validate the pinned manifest record and return exactly one asset."""
    if manifest.get("schema_version") != 1:
        raise VerificationError("unsupported or malformed asset manifest")
    assets = require_array(manifest.get("assets"), "assets")
    asset_objects = [
        require_object(asset, f"assets[{index}]") for index, asset in enumerate(assets)
    ]
    matches = [asset for asset in asset_objects if asset.get("id") == asset_id]
    if len(matches) != 1:
        raise VerificationError(f"asset id must match exactly once: {asset_id!r}")
    asset = matches[0]
    required_paths = (
        ("source", "download_url"),
        ("archive", "filename"),
        ("archive", "size"),
        ("archive", "sha256"),
        ("archive", "sha256_source"),
        ("archive", "etag_md5"),
        ("archive", "last_modified"),
        ("archive", "s3_version_id"),
        ("archive", "entry_count"),
        ("archive", "file_count"),
        ("archive", "uncompressed_bytes"),
        ("archive", "limits"),
        ("local", "root"),
        ("local", "entrypoint"),
        ("local", "entrypoint_sha256"),
        ("local", "primary_buffer"),
        ("local", "primary_buffer_sha256"),
        ("gltf_closure", "file_count"),
        ("gltf_closure", "total_bytes"),
        ("gltf_closure", "tree_sha256"),
    )
    for section, field in required_paths:
        if not isinstance(asset.get(section), dict) or field not in asset[section]:
            raise VerificationError(f"asset {asset_id!r} is missing {section}.{field}")

    archive = asset["archive"]
    filename = safe_archive_filename(archive["filename"])
    archive_size = require_nonnegative_integer(archive["size"], "archive.size")
    if archive_size == 0:
        raise VerificationError("archive.size must be positive")
    require_hex_digest(archive["sha256"], SHA256_HEX_LENGTH, "archive.sha256")
    require_hex_digest(archive["etag_md5"], MD5_HEX_LENGTH, "archive.etag_md5")
    for field in ("sha256_source", "last_modified", "s3_version_id"):
        if not isinstance(archive[field], str) or not archive[field]:
            raise VerificationError(f"archive.{field} must be a nonempty string")
    download_url = asset["source"]["download_url"]
    if not isinstance(download_url, str):
        raise VerificationError("source.download_url must be a string")
    parsed_download = urllib.parse.urlsplit(download_url)
    if parsed_download.scheme != "https" or not parsed_download.netloc:
        raise VerificationError("source.download_url must be an absolute HTTPS URL")
    query_filename = urllib.parse.parse_qs(parsed_download.query).get("fileName")
    if query_filename != [filename]:
        raise VerificationError("source.download_url fileName must match archive.filename")
    for field in ("entry_count", "file_count", "uncompressed_bytes"):
        require_nonnegative_integer(archive[field], f"archive.{field}")
    limits = require_object(archive["limits"], "archive.limits")
    for field in (
        "max_entries",
        "max_uncompressed_bytes",
        "max_file_bytes",
        "max_compression_ratio",
    ):
        value = require_nonnegative_integer(limits.get(field), f"archive.limits.{field}")
        if value == 0:
            raise VerificationError(f"archive.limits.{field} must be positive")
    if archive["entry_count"] > limits["max_entries"]:
        raise VerificationError("archive.entry_count exceeds archive.limits.max_entries")
    if archive["uncompressed_bytes"] > limits["max_uncompressed_bytes"]:
        raise VerificationError(
            "archive.uncompressed_bytes exceeds archive.limits.max_uncompressed_bytes"
        )
    local = asset["local"]
    if local.get("observed_file_count") != archive["file_count"]:
        raise VerificationError("local.observed_file_count must match archive.file_count")
    if local.get("observed_unpacked_bytes") != archive["uncompressed_bytes"]:
        raise VerificationError(
            "local.observed_unpacked_bytes must match archive.uncompressed_bytes"
        )

    for section, field in (
        ("archive", "sha256"),
        ("local", "entrypoint_sha256"),
        ("local", "primary_buffer_sha256"),
        ("gltf_closure", "tree_sha256"),
    ):
        require_hex_digest(asset[section][field], SHA256_HEX_LENGTH, f"{section}.{field}")

    closure = require_object(asset["gltf_closure"], "gltf_closure")
    profile_container = require_object(asset.get("profiles"), "profiles")
    full_profile = require_object(profile_container.get("full"), "profiles.full")
    if full_profile.get("status") != "available-locally":
        raise VerificationError("profiles.full.status must be 'available-locally'")
    if not isinstance(full_profile.get("contents"), str) or not full_profile["contents"]:
        raise VerificationError("profiles.full.contents must be a nonempty string")
    expected_profile = {
        "entrypoint": asset["local"]["entrypoint"],
        "file_count": closure["file_count"],
        "total_bytes": closure["total_bytes"],
        "tree_sha256": closure["tree_sha256"],
    }
    compare_fields(full_profile, expected_profile, "profiles.full")

    required_files = require_array(asset["local"].get("required_files", []), "local.required_files")
    for index, required in enumerate(required_files):
        required = require_object(required, f"local.required_files[{index}]")
        if not isinstance(required.get("path"), str):
            raise VerificationError(f"local.required_files[{index}].path must be a string")
        safe_relative_uri(required["path"])
        require_nonnegative_integer(required.get("size"), f"local.required_files[{index}].size")
        require_hex_digest(
            required.get("sha256"),
            SHA256_HEX_LENGTH,
            f"local.required_files[{index}].sha256",
        )
    return asset
