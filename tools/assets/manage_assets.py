#!/usr/bin/env python3
"""CLI for pinned rendering-asset fetch, install, and offline validation."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
import zipfile
from typing import Any


# Direct script execution starts with tools/assets on sys.path. Add the repository
# root so the same package imports work from CLI, CTest, and unit tests.
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parents[2]
if str(REPOSITORY_ROOT) not in sys.path:
    sys.path.insert(0, str(REPOSITORY_ROOT))

from tools.assets.archive_install import (  # noqa: E402
    default_cache_directory,
    fetch_archive,
    install_archive,
)
from tools.assets.asset_common import (  # noqa: E402
    DEFAULT_MANIFEST,
    VerificationError,
    load_asset,
    read_json,
)
from tools.assets.gltf_validation import verify_asset, verify_gltf  # noqa: E402


def emit_result(result: dict[str, Any], evidence_path: pathlib.Path | None) -> None:
    """Print one JSON result and optionally persist the identical evidence."""
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if evidence_path is not None:
        evidence_path.parent.mkdir(parents=True, exist_ok=True)
        evidence_path.write_text(payload, encoding="utf-8")
    sys.stdout.write(payload)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=pathlib.Path, default=DEFAULT_MANIFEST)
    subparsers = parser.add_subparsers(dest="command", required=True)

    verify = subparsers.add_parser("verify", help="verify an extracted manifest asset offline")
    verify.add_argument("--asset", required=True)
    verify.add_argument("--root", type=pathlib.Path)
    verify.add_argument("--evidence", type=pathlib.Path)

    fixture = subparsers.add_parser(
        "verify-gltf", help="validate a standalone deterministic glTF fixture"
    )
    fixture.add_argument("entrypoint", type=pathlib.Path)
    fixture.add_argument("--expect", type=pathlib.Path)
    fixture.add_argument("--evidence", type=pathlib.Path)

    fetch = subparsers.add_parser(
        "fetch", help="download, verify, and atomically install an asset"
    )
    fetch.add_argument("--asset", required=True)
    fetch.add_argument(
        "--root",
        type=pathlib.Path,
        required=True,
        help="empty external install root",
    )
    fetch.add_argument(
        "--cache",
        type=pathlib.Path,
        default=default_cache_directory(),
        help="verified archive cache outside the Git worktree",
    )
    fetch.add_argument("--accept-license", action="store_true")
    fetch.add_argument("--evidence", type=pathlib.Path)

    install = subparsers.add_parser(
        "install", help="verify and atomically install an existing archive"
    )
    install.add_argument("--asset", required=True)
    install.add_argument("--archive", type=pathlib.Path, required=True)
    install.add_argument("--root", type=pathlib.Path, required=True)
    install.add_argument("--accept-license", action="store_true")
    install.add_argument("--evidence", type=pathlib.Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        if args.command == "verify-gltf":
            result = verify_gltf(
                args.entrypoint.resolve(),
                args.expect.resolve() if args.expect else None,
            )
        else:
            asset = load_asset(read_json(args.manifest.resolve()), args.asset)
            if args.command == "verify":
                result = verify_asset(asset, args.root)
            elif args.command == "install":
                result = install_archive(asset, args.archive, args.root, args.accept_license)
            else:
                archive_result = fetch_archive(asset, args.cache, args.accept_license)
                result = install_archive(
                    asset,
                    pathlib.Path(archive_result["path"]),
                    args.root,
                    args.accept_license,
                )
                result["download"] = archive_result
        emit_result(result, args.evidence)
        return 0
    except (VerificationError, zipfile.BadZipFile, OSError) as error:
        emit_result({"status": "fail", "error": str(error)}, getattr(args, "evidence", None))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
