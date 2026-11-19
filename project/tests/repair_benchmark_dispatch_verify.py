"""Exercise the real repair_benchmark CLI without contacting a cluster."""

import re
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path


BINARY = str(Path(sys.argv.pop(1)).resolve())
FAILURE_COUNTS = {"single": 1, "double": 2, "triple": 3, "quadruple": 4}
CASE_COUNTS = {
    "NLRC": {64: (2, 4, 3, 2), 96: (2, 5, 5, 5), 128: (2, 4, 5, 6)},
    "HLRC": {64: (3, 8, 8, 4), 96: (3, 8, 10, 10), 128: (3, 8, 10, 12)},
}
TASK = re.compile(r"  task owner=(\d+) failed=\[([\d,]+)\], sources=\[([\d,]+)\]")


def coding_parameters(code, k):
    if code == "NLRC":
        return 3, {64: 5, 96: 7, 128: 9}[k]
    if code == "HLRC":
        return 2, {64: 6, 96: 8, 128: 10}[k]
    return 3, {64: 3, 96: 4, 128: 5}[k]


class DispatchVerification(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="hlrc-dispatch-")
        self.addCleanup(self.directory.cleanup)
        self.config = Path(self.directory.name) / "parameters.xml"

    def run_plan(self, code, k, failure, *, configured_k=None, r=None, z=None, extra=()):
        expected_r, expected_z = coding_parameters(code, k)
        fields = {
            "AlignedSize": 4096, "UnitSize": 8192, "BlockSize": 1048576,
            "CoordinatorIP": "127.0.0.1", "CoordinatorPort": 1,
            "ClientIP": "127.0.0.1", "AppendMode": "UNILRC_MODE",
            "CodeType": code, "k": k if configured_k is None else configured_k,
            "r": expected_r if r is None else r, "z": expected_z if z is None else z,
            "NodeNum": 160,
        }
        root = ET.Element("Configuration")
        for name, value in fields.items():
            ET.SubElement(root, name).text = str(value)
        ET.ElementTree(root).write(self.config, encoding="utf-8", xml_declaration=True)
        return subprocess.run(
            [BINARY, str(k), failure, "--check-plan", str(self.config), *extra],
            capture_output=True, text=True, timeout=5, check=False,
        )

    def parse_plan(self, result, code, k, failure):
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        r, z = coding_parameters(code, k)
        self.assertIn(f"Repair plan: code={code}, k={k}, r={r}, z={z}, failure={failure}", output)
        self.assertIn("Repair plan check passed.", output)
        self.assertNotIn("RPC failed", output)
        cases = {}
        for line in result.stdout.splitlines():
            if line.startswith("Case "):
                name = line[5:]
                self.assertNotIn(name, cases)
                cases[name] = []
            elif line.startswith("  task "):
                match = TASK.fullmatch(line)
                self.assertIsNotNone(match, line)
                owner = int(match[1])
                failed = [int(value) for value in match[2].split(",")]
                sources = [int(value) for value in match[3].split(",")]
                cases[name].append((owner, failed, sources))
        self.assertTrue(cases, output)
        for name, tasks in cases.items():
            self.assertTrue(tasks, name)
            failed = [block for _, blocks, _ in tasks for block in blocks]
            self.assertEqual(len(failed), FAILURE_COUNTS[failure], name)
            self.assertEqual(len(set(failed)), len(failed), name)
            for owner, blocks, sources in tasks:
                self.assertEqual(owner, blocks[0], name)
                self.assertEqual(len(set(sources)), len(sources), name)
                self.assertFalse(set(sources) & set(failed), name)
                self.assertTrue(all(0 <= block < k + r + z for block in blocks + sources), name)
        return cases

    def test_all_nlrc_hlrc_entry_points(self):
        for code, sizes in CASE_COUNTS.items():
            for k, counts in sizes.items():
                for failure, expected_count in zip(FAILURE_COUNTS, counts):
                    with self.subTest(code=code, k=k, failure=failure):
                        cases = self.parse_plan(self.run_plan(code, k, failure), code, k, failure)
                        self.assertEqual(len(cases), expected_count)
                        if failure == "triple":
                            tail = k + (1 if code == "NLRC" else 2) + 1
                            failed = [k, k + 1, tail]
                            name = "G0+G1+G2" if code == "NLRC" else "G0+G1+L1"
                            self.assertEqual(cases[name], [(k, failed, list(range(k)))])

    def test_k64_quadruple_shared_tasks(self):
        cases = self.parse_plan(self.run_plan("NLRC", 64, "quadruple"), "NLRC", 64, "quadruple")
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (67, [67, 68], list(range(22))), (69, [69, 70], list(range(22, 44))),
        ])
        self.assertEqual(cases["G0+G1+G2+L0"], [(64, [64, 65, 66, 67], list(range(64)))])
        cases = self.parse_plan(self.run_plan("HLRC", 64, "quadruple"), "HLRC", 64, "quadruple")
        self.assertEqual(cases["L0+L1+L3+U0"], [
            (70, [70, 66, 67], list(range(16, 32)) + list(range(16)) + [64]),
            (69, [69], list(range(48, 64))),
        ])
        self.assertEqual(cases["G0+G1+L1+L3"], [(64, [64, 65, 67, 69], list(range(64)))])

    def test_nlrc_k64_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 64, "triple"),
                                "NLRC", 64, "triple")
        self.assertEqual(list(cases), ["L0+L1+L2", "L0+L1+L4", "G0+G1+G2"])
        self.assertEqual(cases["L0+L1+L2"], [
            (67, [67, 68], list(range(22))),
            (69, [69], list(range(22, 44))),
        ])
        self.assertEqual(cases["L0+L1+L4"], [
            (67, [67, 68], list(range(22))),
            (71, [71], list(range(44, 64)) + [64, 65, 66]),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(64, [64, 65, 66], list(range(64)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("NLRC", 64, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k64_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 64, "double"),
                                "OptimalLRC", 64, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (67, [67], list(range(21)) + [64, 65, 66]),
            (68, [68], list(range(21, 42)) + [64, 65, 66]),
        ])
        self.assertEqual(cases["L0+L2"], [
            (67, [67], list(range(21)) + [64, 65, 66]),
            (69, [69], list(range(42, 64)) + [64, 65, 66]),
        ])
        self.assertEqual(cases["G0+G1"], [(64, [64, 65], list(range(64)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("OptimalLRC", 64, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_uniform_k64_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 64, "double"),
                                "UniformLRC", 64, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (67, [67], list(range(22))),
            (68, [68], list(range(22, 44))),
        ])
        self.assertEqual(cases["L0+L2"], [
            (67, [67], list(range(22))),
            (69, [69], list(range(44, 64)) + [64, 65, 66]),
        ])
        self.assertEqual(cases["G0+G1"], [(64, [64, 65], list(range(64)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("UniformLRC", 64, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_standard_lrc_triple_and_quadruple_regressions(self):
        for code in ("AzureLRC", "OptimalLRC", "UniformLRC"):
            for k in (64, 96, 128):
                for failure in ("triple", "quadruple"):
                    with self.subTest(code=code, k=k, failure=failure):
                        self.parse_plan(self.run_plan(code, k, failure), code, k, failure)

    def test_invalid_parameters_still_fail(self):
        for code in ("NLRC", "HLRC"):
            r, z = coding_parameters(code, 64)
            for overrides in ({"r": r + 1}, {"z": z + 1}, {"configured_k": 96}):
                with self.subTest(code=code, overrides=overrides):
                    result = self.run_plan(code, 64, "quadruple", **overrides)
                    self.assertNotEqual(result.returncode, 0)
                    self.assertNotIn("Repair plan check passed.", result.stdout)
        result = self.run_plan("NLRC", 64, "quintuple")
        self.assertEqual(result.returncode, 2)
        self.assertIn("unsupported failure type", result.stderr)

    def test_repeat_options_are_checked_without_network(self):
        result = self.run_plan("NLRC", 96, "single", extra=(
            "--repeat", "30", "--warmup", "2", "--stripe-id", "34"))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("repeat=30, warmup=2, stripe=34", result.stdout)
        for extra in (("--repeat", "0"), ("--repeat", "-1"), ("--repeat", "1000001"),
                      ("--warmup", "-1"), ("--stripe-id", "-1"), ("--repeat",),
                      ("--repeat", "abc"), ("--repeat", "2", "--repeat", "3"),
                      ("--unknown", "1"), ("--warmup", "999999999999999999999")):
            with self.subTest(extra=extra):
                result = self.run_plan("NLRC", 96, "single", extra=extra)
                self.assertEqual(result.returncode, 2, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
