"""Tests that dependency closure hashes detect transitive vendored changes."""

from __future__ import annotations

import importlib.util
import pathlib
import tempfile
import unittest
from unittest import mock


SOURCE_ROOT = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = SOURCE_ROOT / "tools" / "build" / "verify_dependencies.py"
SPEC = importlib.util.spec_from_file_location("verify_dependencies", MODULE_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFY_DEPENDENCIES = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY_DEPENDENCIES)


class ClosureHashTests(unittest.TestCase):
    def test_nested_mutation_addition_and_removal_change_digest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            nested = root / "vendor" / "detail"
            nested.mkdir(parents=True)
            first = root / "vendor" / "public.h"
            second = nested / "private.h"
            first.write_bytes(b"public")
            second.write_bytes(b"private")

            initial, count = VERIFY_DEPENDENCIES.closure_sha256(root, ["vendor"])
            reordered, reordered_count = VERIFY_DEPENDENCIES.closure_sha256(
                root, ["vendor/detail", "vendor/public.h"]
            )
            self.assertEqual((initial, count), (reordered, reordered_count))
            self.assertEqual(2, count)

            second.write_bytes(b"changed")
            mutated, _ = VERIFY_DEPENDENCIES.closure_sha256(root, ["vendor"])
            self.assertNotEqual(initial, mutated)

            third = nested / "new.h"
            third.write_bytes(b"new")
            added, added_count = VERIFY_DEPENDENCIES.closure_sha256(root, ["vendor"])
            self.assertNotEqual(mutated, added)
            self.assertEqual(3, added_count)

            first.unlink()
            removed, removed_count = VERIFY_DEPENDENCIES.closure_sha256(root, ["vendor"])
            self.assertNotEqual(added, removed)
            self.assertEqual(2, removed_count)

    @mock.patch.object(VERIFY_DEPENDENCIES.subprocess, "run")
    def test_duplicate_multiarch_package_versions_are_normalized(
        self,
        run: mock.Mock,
    ) -> None:
        run.return_value = VERIFY_DEPENDENCIES.subprocess.CompletedProcess(
            args=[],
            returncode=0,
            stdout="1.2.3\n1.2.3\n",
            stderr="",
        )
        self.assertEqual(
            "1.2.3",
            VERIFY_DEPENDENCIES.installed_package_version("package"),
        )


if __name__ == "__main__":
    unittest.main()
