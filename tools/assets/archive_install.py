"""Checksum-gated archive download and atomic, bounded ZIP installation."""

from __future__ import annotations

import hashlib
import os
import pathlib
import shutil
import stat
import tempfile
import urllib.request
import zipfile
from typing import Any

from tools.assets.asset_common import (
    HASH_CHUNK_BYTES,
    REPOSITORY_ROOT,
    VerificationError,
    resolve_under,
)
from tools.assets.gltf_validation import verify_asset


ALLOWED_ZIP_COMPRESSION = {zipfile.ZIP_STORED, zipfile.ZIP_DEFLATED}


def default_cache_directory() -> pathlib.Path:
    base = pathlib.Path(os.environ.get("XDG_CACHE_HOME", pathlib.Path.home() / ".cache"))
    return base / "mini-engine" / "assets"


def require_external_path(path: pathlib.Path, context: str) -> pathlib.Path:
    resolved = path.expanduser().resolve()
    if resolved == REPOSITORY_ROOT or REPOSITORY_ROOT in resolved.parents:
        raise VerificationError(f"{context} must be outside the Git worktree")
    return resolved


def archive_digests(path: pathlib.Path) -> tuple[str, str]:
    sha256 = hashlib.sha256()
    md5 = hashlib.md5(usedforsecurity=False)
    try:
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(HASH_CHUNK_BYTES), b""):
                sha256.update(chunk)
                md5.update(chunk)
    except OSError as error:
        raise VerificationError(f"cannot read archive {path}: {error}") from error
    return sha256.hexdigest(), md5.hexdigest()


def verify_archive(asset: dict[str, Any], archive_path: pathlib.Path) -> dict[str, Any]:
    """Verify exact archive size, project SHA-256, and recorded ETag/MD5."""
    archive = asset["archive"]
    archive_path = archive_path.expanduser().resolve()
    if not archive_path.is_file():
        raise VerificationError(f"archive is missing: {archive_path}")
    actual_size = archive_path.stat().st_size
    if actual_size != archive["size"]:
        raise VerificationError(
            f"archive size mismatch: expected {archive['size']}, got {actual_size}"
        )
    sha256, md5 = archive_digests(archive_path)
    if sha256 != archive["sha256"]:
        raise VerificationError(
            f"archive SHA-256 mismatch: expected {archive['sha256']}, got {sha256}"
        )
    if md5 != archive["etag_md5"]:
        raise VerificationError(
            f"archive MD5/ETag mismatch: expected {archive['etag_md5']}, got {md5}"
        )
    return {
        "path": str(archive_path),
        "size": actual_size,
        "sha256": sha256,
        "etag_md5": md5,
    }


def fetch_archive(
    asset: dict[str, Any], destination: pathlib.Path, accept_license: bool
) -> dict[str, Any]:
    """Fetch into an external cache and atomically publish only verified bytes."""
    archive = asset["archive"]
    if not accept_license:
        raise VerificationError("fetch requires --accept-license")
    destination = require_external_path(destination, "download cache")
    destination.mkdir(parents=True, exist_ok=True)
    output = destination / archive["filename"]
    if output.exists():
        result = verify_archive(asset, output)
        result.update({"status": "pass", "asset_id": asset["id"], "cache_hit": True})
        return result

    temporary_path: pathlib.Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{archive['filename']}.",
            suffix=".part",
            dir=destination,
            delete=False,
        ) as stream:
            temporary_path = pathlib.Path(stream.name)
            try:
                with urllib.request.urlopen(asset["source"]["download_url"]) as response:
                    content_length = response.headers.get("Content-Length")
                    if content_length is not None and int(content_length) != archive["size"]:
                        raise VerificationError(
                            f"publisher Content-Length mismatch: expected {archive['size']}, "
                            f"got {content_length}"
                        )
                    downloaded = 0
                    while chunk := response.read(HASH_CHUNK_BYTES):
                        downloaded += len(chunk)
                        if downloaded > archive["size"]:
                            raise VerificationError("download exceeds pinned archive size")
                        stream.write(chunk)
            except (OSError, ValueError) as error:
                raise VerificationError(f"archive download failed: {error}") from error
        verified = verify_archive(asset, temporary_path)
        os.replace(temporary_path, output)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
    verified.update({"status": "pass", "asset_id": asset["id"], "cache_hit": False})
    verified["path"] = str(output)
    return verified


def safe_zip_member_path(name: str) -> pathlib.PurePosixPath:
    if not name or "\0" in name or "\\" in name or name.startswith("/"):
        raise VerificationError(f"unsafe ZIP member name: {name!r}")
    trimmed = name[:-1] if name.endswith("/") else name
    components = trimmed.split("/")
    if not trimmed or any(component in ("", ".", "..") for component in components):
        raise VerificationError(f"unsafe ZIP member name: {name!r}")
    if ":" in components[0]:
        raise VerificationError(f"unsafe ZIP member drive/path: {name!r}")
    return pathlib.PurePosixPath(*components)


def inspect_zip(asset: dict[str, Any], archive_path: pathlib.Path) -> list[zipfile.ZipInfo]:
    """Preflight every ZIP member before writing any extracted data."""
    archive = asset["archive"]
    limits = archive["limits"]
    try:
        with zipfile.ZipFile(archive_path) as package:
            members = package.infolist()
    except (OSError, zipfile.BadZipFile, NotImplementedError) as error:
        raise VerificationError(f"cannot inspect ZIP archive: {error}") from error

    if len(members) != archive["entry_count"]:
        raise VerificationError(
            f"ZIP entry count mismatch: expected {archive['entry_count']}, got {len(members)}"
        )
    if len(members) > limits["max_entries"]:
        raise VerificationError("ZIP entry count exceeds safety limit")

    seen_paths: set[str] = set()
    file_count = 0
    uncompressed_bytes = 0
    for member in members:
        relative = safe_zip_member_path(member.filename)
        canonical = str(relative).casefold()
        if canonical in seen_paths:
            raise VerificationError(f"duplicate or case-colliding ZIP path: {member.filename!r}")
        seen_paths.add(canonical)
        if relative.parts[0] != "main_sponza":
            raise VerificationError(
                f"ZIP member is outside expected main_sponza root: {member.filename!r}"
            )
        if member.flag_bits & 0x1:
            raise VerificationError(f"encrypted ZIP member is not allowed: {member.filename!r}")
        if member.compress_type not in ALLOWED_ZIP_COMPRESSION:
            raise VerificationError(
                f"unsupported ZIP compression for {member.filename!r}: {member.compress_type}"
            )
        unix_mode = (member.external_attr >> 16) & 0xFFFF
        if member.create_system == 3 and unix_mode:
            file_type = stat.S_IFMT(unix_mode)
            if file_type == stat.S_IFLNK:
                raise VerificationError(f"ZIP symbolic link is not allowed: {member.filename!r}")
            if file_type not in (0, stat.S_IFREG, stat.S_IFDIR):
                raise VerificationError(f"ZIP special file is not allowed: {member.filename!r}")
        if member.is_dir():
            continue
        file_count += 1
        uncompressed_bytes += member.file_size
        if member.file_size > limits["max_file_bytes"]:
            raise VerificationError(f"ZIP member exceeds per-file size limit: {member.filename!r}")
        if member.file_size > 0 and member.compress_size == 0:
            raise VerificationError(
                f"ZIP member has an invalid compression ratio: {member.filename!r}"
            )
        if member.compress_size and member.file_size / member.compress_size > limits[
            "max_compression_ratio"
        ]:
            raise VerificationError(
                f"ZIP member exceeds compression-ratio limit: {member.filename!r}"
            )

    if file_count != archive["file_count"]:
        raise VerificationError(
            f"ZIP file count mismatch: expected {archive['file_count']}, got {file_count}"
        )
    if uncompressed_bytes != archive["uncompressed_bytes"]:
        raise VerificationError(
            "ZIP uncompressed byte count mismatch: "
            f"expected {archive['uncompressed_bytes']}, got {uncompressed_bytes}"
        )
    if uncompressed_bytes > limits["max_uncompressed_bytes"]:
        raise VerificationError("ZIP uncompressed bytes exceed safety limit")
    return members


def extract_zip(
    asset: dict[str, Any], archive_path: pathlib.Path, destination: pathlib.Path
) -> None:
    """Stream preflighted regular files while enforcing actual write limits."""
    members = inspect_zip(asset, archive_path)
    limits = asset["archive"]["limits"]
    actual_total = 0
    try:
        with zipfile.ZipFile(archive_path) as package:
            for member in members:
                relative = safe_zip_member_path(member.filename)
                output = resolve_under(destination, relative)
                if member.is_dir():
                    output.mkdir(parents=True, exist_ok=True)
                    continue
                output.parent.mkdir(parents=True, exist_ok=True)
                written = 0
                with package.open(member, "r") as source, output.open("xb") as target:
                    while chunk := source.read(HASH_CHUNK_BYTES):
                        written += len(chunk)
                        actual_total += len(chunk)
                        if written > member.file_size:
                            raise VerificationError(
                                f"ZIP member expanded beyond declared size: {member.filename!r}"
                            )
                        if actual_total > limits["max_uncompressed_bytes"]:
                            raise VerificationError("ZIP extraction exceeds total byte limit")
                        target.write(chunk)
                if written != member.file_size:
                    raise VerificationError(
                        f"ZIP member size mismatch after extraction: {member.filename!r}"
                    )
    except (OSError, zipfile.BadZipFile, RuntimeError) as error:
        if isinstance(error, VerificationError):
            raise
        raise VerificationError(f"ZIP extraction failed: {error}") from error
    if actual_total != asset["archive"]["uncompressed_bytes"]:
        raise VerificationError(
            "ZIP extracted byte count mismatch: "
            f"expected {asset['archive']['uncompressed_bytes']}, got {actual_total}"
        )


def install_archive(
    asset: dict[str, Any],
    archive_path: pathlib.Path,
    root: pathlib.Path,
    accept_license: bool,
) -> dict[str, Any]:
    """Verify and install via sibling staging, then atomically rename."""
    if not accept_license:
        raise VerificationError("install requires --accept-license")
    requested_root = root.expanduser()
    if requested_root.is_symlink():
        raise VerificationError("install root must not be a symbolic link")
    root = require_external_path(root, "install root")
    if root.exists() and (not root.is_dir() or any(root.iterdir())):
        raise VerificationError("install root must not exist or must be an empty directory")
    root.parent.mkdir(parents=True, exist_ok=True)
    archive_evidence = verify_archive(asset, archive_path)
    root_existed = root.exists()
    staging = pathlib.Path(tempfile.mkdtemp(prefix=f".{root.name}.install-", dir=root.parent))
    installed = False
    try:
        extract_zip(asset, pathlib.Path(archive_evidence["path"]), staging)
        asset_evidence = verify_asset(asset, staging)
        if root_existed:
            root.rmdir()
        try:
            os.replace(staging, root)
            installed = True
            asset_evidence["root"] = str(root)
        except OSError:
            if root_existed and not root.exists():
                root.mkdir()
            raise
    except OSError as error:
        raise VerificationError(f"atomic asset install failed: {error}") from error
    finally:
        if not installed and staging.exists():
            shutil.rmtree(staging)
    return {
        "status": "pass",
        "asset_id": asset["id"],
        "archive": archive_evidence,
        "install_root": str(root),
        "verification": asset_evidence,
    }
