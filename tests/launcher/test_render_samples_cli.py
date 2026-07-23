#!/usr/bin/env python3
"""Black-box launcher tests for parsing, asset roots, cwd independence, and frame limits."""

from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import tempfile
import unittest


ARGUMENTS = argparse.ArgumentParser(add_help=False)
ARGUMENTS.add_argument("--executable", type=pathlib.Path, required=True)
ARGUMENTS.add_argument("--build-dir", type=pathlib.Path, required=True)
ARGUMENTS.add_argument("--source-dir", type=pathlib.Path, required=True)
OPTIONS, UNITTEST_ARGUMENTS = ARGUMENTS.parse_known_args()

EXECUTABLE = OPTIONS.executable.resolve()
BUILD_DIR = OPTIONS.build_dir.resolve()
SOURCE_DIR = OPTIONS.source_dir.resolve()
STAGED_ASSETS = BUILD_DIR / "assets"


class LauncherCliTests(unittest.TestCase):
    maxDiff = None

    def run_launcher(
        self,
        arguments: list[str],
        *,
        cwd: pathlib.Path | None = None,
        asset_environment: str | None = None,
    ) -> subprocess.CompletedProcess[str]:
        environment = os.environ.copy()
        environment.pop("ENGINE_ASSET_ROOT", None)
        if asset_environment is not None:
            environment["ENGINE_ASSET_ROOT"] = asset_environment
        return subprocess.run(
            [str(EXECUTABLE), *arguments],
            cwd=cwd or SOURCE_DIR,
            env=environment,
            text=True,
            capture_output=True,
            check=False,
            timeout=45,
        )

    def assert_parse_failure(self, arguments: list[str], expected: str) -> None:
        result = self.run_launcher(arguments)
        self.assertEqual(2, result.returncode, result.stdout + result.stderr)
        self.assertIn(expected, result.stderr)
        self.assertIn("Usage:", result.stderr)
        self.assertNotIn("sample_complete", result.stdout)

    def assert_completed_frames(
        self,
        result: subprocess.CompletedProcess[str],
        expected_frames: int,
    ) -> None:
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        records = [
            json.loads(line)
            for line in result.stdout.splitlines()
            if line.startswith("{")
        ]
        self.assertEqual(1, len(records), result.stdout)
        self.assertEqual(
            {
                "event": "sample_complete",
                "sample": "ray-marching",
                "frames_rendered": expected_frames,
            },
            records[0],
        )

    def test_help_and_listing_do_not_initialize_assets_or_gl(self) -> None:
        help_result = self.run_launcher(["--help"], asset_environment="/missing")
        self.assertEqual(0, help_result.returncode)
        self.assertIn("render_samples --list-samples", help_result.stdout)

        list_result = self.run_launcher(["--list-samples"], asset_environment="/missing")
        self.assertEqual(0, list_result.returncode)
        self.assertEqual(
            ["ray-marching\tProcedural signed-distance-field ray marcher"],
            list_result.stdout.splitlines(),
        )

    def test_parser_rejects_invalid_and_ambiguous_arguments(self) -> None:
        cases = [
            ([], "missing required --sample"),
            (["--sample"], "--sample requires a value"),
            (["--sample", "ray-marching", "--frames"], "--frames requires a value"),
            (["--sample", "ray-marching", "--width", "0"], "--width requires an integer"),
            (["--sample", "ray-marching", "--height", "-1"], "--height requires an integer"),
            (["--sample", "ray-marching", "--frames", "+1"], "--frames requires an integer"),
            (["--sample", "ray-marching", "--frames", "1x"], "--frames requires an integer"),
            (["--sample", "ray-marching", "--frames", "10000001"], "--frames requires an integer"),
            (["--sample", "ray-marching", "--width", "16385"], "--width requires an integer"),
            (["--sample", "a", "--sample", "b"], "duplicate option: --sample"),
            (["--frames", "1", "--frames", "2", "--sample", "ray-marching"], "duplicate option"),
            (["--help", "--list-samples"], "--help cannot be combined"),
            (["--list-samples", "--sample", "ray-marching"], "--list-samples cannot be combined"),
            (["--sample", "ray-marching", "--unknown"], "unknown argument"),
            (["ray-marching"], "unknown argument"),
        ]
        for arguments, expected in cases:
            with self.subTest(arguments=arguments):
                self.assert_parse_failure(arguments, expected)

    def test_unknown_sample_fails_before_asset_resolution(self) -> None:
        result = self.run_launcher(["--sample", "missing"], asset_environment="/missing")
        self.assertEqual(2, result.returncode)
        self.assertIn("unknown sample: missing", result.stderr)
        self.assertNotIn("asset root", result.stderr)

    def test_explicit_and_environment_roots_fail_closed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            temporary = pathlib.Path(directory)
            missing = temporary / "missing"
            ordinary_file = temporary / "file"
            ordinary_file.write_text("not a directory", encoding="utf-8")

            explicit = self.run_launcher(
                [
                    "--sample",
                    "ray-marching",
                    "--frames",
                    "1",
                    "--asset-root",
                    str(missing),
                ],
                asset_environment=str(STAGED_ASSETS),
            )
            self.assertEqual(1, explicit.returncode)
            self.assertIn("asset root from --asset-root", explicit.stderr)

            environment = self.run_launcher(
                ["--sample", "ray-marching", "--frames", "1"],
                asset_environment=str(ordinary_file),
            )
            self.assertEqual(1, environment.returncode)
            self.assertIn("asset root from ENGINE_ASSET_ROOT", environment.stderr)

    def test_fixed_frame_run_is_exact_and_cwd_independent(self) -> None:
        with tempfile.TemporaryDirectory() as unrelated:
            working_directories = [
                SOURCE_DIR,
                BUILD_DIR,
                pathlib.Path(unrelated),
            ]
            for cwd in working_directories:
                with self.subTest(cwd=cwd):
                    result = self.run_launcher(
                        ["--sample", "ray-marching", "--frames", "10"],
                        cwd=cwd,
                    )
                    self.assert_completed_frames(result, 10)

    def test_asset_root_precedence_runs_with_valid_cli_over_invalid_environment(self) -> None:
        result = self.run_launcher(
            [
                "--sample",
                "ray-marching",
                "--frames",
                "1",
                "--asset-root",
                str(STAGED_ASSETS),
            ],
            asset_environment="/definitely/missing",
        )
        self.assert_completed_frames(result, 1)

    def test_environment_asset_root_runs_from_unrelated_directory(self) -> None:
        with tempfile.TemporaryDirectory() as unrelated:
            result = self.run_launcher(
                ["--sample", "ray-marching", "--frames", "1"],
                cwd=pathlib.Path(unrelated),
                asset_environment=str(STAGED_ASSETS),
            )
        self.assert_completed_frames(result, 1)

    def test_build_stages_only_declared_small_assets(self) -> None:
        files = sorted(
            path.relative_to(BUILD_DIR).as_posix()
            for path in (BUILD_DIR / "assets").rglob("*")
            if path.is_file()
        )
        self.assertEqual(
            [
                "assets/shaders/fragmentCube.glsl",
                "assets/shaders/vertexCube.glsl",
            ],
            files,
        )
        self.assertFalse(any("sponza" in path.lower() for path in files))
        self.assertLess(
            sum((BUILD_DIR / path).stat().st_size for path in files),
            64 * 1024,
        )


if __name__ == "__main__":
    unittest.main(argv=[pathlib.Path(__file__).name, *UNITTEST_ARGUMENTS])
