#!/usr/bin/env python3
"""Emit and verify the exact system-package and vendored dependency lock."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import subprocess
import sys
from typing import Any


def load_lock(source_root: pathlib.Path) -> dict[str, Any]:
    lock_path = source_root / "dependencies.lock.json"
    with lock_path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema_version") != 1:
        raise ValueError(f"unsupported dependency lock schema in {lock_path}")
    return data


def locked_packages(lock: dict[str, Any], include_ci: bool) -> dict[str, str]:
    packages = dict(lock["package_lock"]["build"])
    if include_ci:
        packages.update(lock["package_lock"]["ci"])
    return packages


def installed_package_version(package: str) -> str:
    result = subprocess.run(
        ["dpkg-query", "-W", "-f=${Version}\\n", package],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return ""
    versions = sorted(set(result.stdout.splitlines()))
    return versions[0] if len(versions) == 1 else ",".join(versions)


def closure_files(source_root: pathlib.Path, paths: list[str]) -> list[pathlib.Path]:
    files: list[pathlib.Path] = []
    for relative in paths:
        candidate = source_root / relative
        if candidate.is_file():
            files.append(candidate)
        elif candidate.is_dir():
            files.extend(path for path in candidate.rglob("*") if path.is_file())
        else:
            raise FileNotFoundError(candidate)
    return sorted(set(files), key=lambda path: path.relative_to(source_root).as_posix())


def closure_sha256(source_root: pathlib.Path, paths: list[str]) -> tuple[str, int]:
    """Hash each relative name and byte stream so additions and removals change the lock."""
    digest = hashlib.sha256()
    files = closure_files(source_root, paths)
    for path in files:
        relative = path.relative_to(source_root).as_posix().encode("utf-8")
        digest.update(relative)
        digest.update(b"\0")
        with path.open("rb") as handle:
            for block in iter(lambda: handle.read(1024 * 1024), b""):
                digest.update(block)
        digest.update(b"\0")
    return digest.hexdigest(), len(files)


def verify_ci_action_pins(
    source_root: pathlib.Path,
    lock: dict[str, Any],
) -> tuple[dict[str, dict[str, Any]], list[str]]:
    workflow_path = source_root / ".github" / "workflows" / "ci.yml"
    workflow = workflow_path.read_text(encoding="utf-8")
    uses = re.findall(r"^\s*uses:\s*([^@\s]+)@([^\s#]+)", workflow, re.MULTILINE)
    configured: dict[str, list[str]] = {}
    for action, revision in uses:
        if action.startswith("./"):
            continue
        configured.setdefault(action, []).append(revision)

    failures: list[str] = []
    results: dict[str, dict[str, Any]] = {}
    expected_actions = lock["ci_actions"]
    for action, expected in sorted(expected_actions.items()):
        actual = configured.pop(action, [])
        matches = bool(actual) and all(revision == expected for revision in actual)
        results[action] = {
            "expected": expected,
            "actual": actual,
            "matches": matches,
        }
        if not matches:
            failures.append(
                f"CI action {action}: expected only {expected}, got {actual or '<missing>'}"
            )

    for action, revisions in sorted(configured.items()):
        failures.append(f"CI action {action} is not present in dependencies.lock.json")
        results[action] = {
            "expected": "<missing-lock-entry>",
            "actual": revisions,
            "matches": False,
        }
    return results, failures


def verify(source_root: pathlib.Path, include_ci: bool) -> int:
    lock = load_lock(source_root)
    failures: list[str] = []
    package_results: dict[str, dict[str, str | bool]] = {}
    for package, expected in sorted(locked_packages(lock, include_ci).items()):
        actual = installed_package_version(package)
        matches = actual == expected
        package_results[package] = {
            "expected": expected,
            "actual": actual or "<not-installed>",
            "matches": matches,
        }
        if not matches:
            failures.append(f"package {package}: expected {expected}, got {actual or '<not-installed>'}")

    file_results: dict[str, dict[str, str | int | bool]] = {}
    for dependency, fields in sorted(lock["vendored_dependencies"].items()):
        expected = fields["closure_sha256"]
        try:
            actual, file_count = closure_sha256(source_root, fields["paths"])
        except FileNotFoundError as error:
            actual, file_count = "", 0
            failures.append(f"vendored closure {dependency}: missing {error.filename}")
        matches = actual == expected
        file_results[dependency] = {
            "expected": expected,
            "actual": actual or "<missing>",
            "file_count": file_count,
            "matches": matches,
        }
        if not matches and actual:
            failures.append(
                f"vendored closure {dependency}: expected {expected}, got {actual}"
            )

    action_results, action_failures = verify_ci_action_pins(source_root, lock)
    failures.extend(action_failures)

    print(
        json.dumps(
            {
                "schema_version": 1,
                "status": "pass" if not failures else "fail",
                "include_ci": include_ci,
                "packages": package_results,
                "vendored_files": file_results,
                "ci_actions": action_results,
                "failures": failures,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0 if not failures else 1


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)

    for command in ("apt-arguments", "verify"):
        subparser = subparsers.add_parser(command)
        subparser.add_argument(
            "--source-root",
            type=pathlib.Path,
            default=pathlib.Path(__file__).resolve().parents[2],
        )
        subparser.add_argument("--include-ci", action="store_true")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    source_root = arguments.source_root.resolve()
    lock = load_lock(source_root)
    if arguments.command == "apt-arguments":
        packages = locked_packages(lock, arguments.include_ci)
        print(" ".join(f"{name}={version}" for name, version in sorted(packages.items())))
        return 0
    return verify(source_root, arguments.include_ci)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (KeyError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"dependency lock error: {error}", file=sys.stderr)
        raise SystemExit(2)
