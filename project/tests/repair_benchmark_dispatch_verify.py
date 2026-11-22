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

    def test_nlrc_k128_muti_exact_shared_scenarios(self):
        result = self.run_plan("NLRC", 128, "muti")
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode, 0, output)
        self.assertIn("failure=muti", output)
        cases = {}
        current = None
        for line in result.stdout.splitlines():
            if line.startswith("Case "):
                current = line[5:]
                cases[current] = []
            elif line.startswith("  task "):
                match = TASK.fullmatch(line)
                self.assertIsNotNone(match, line)
                cases[current].append((int(match[1]),
                    [int(value) for value in match[2].split(",")],
                    [int(value) for value in match[3].split(",")]))
        self.assertEqual(list(cases), ["G0+G1", "G0+G1+G2"])
        self.assertEqual(cases["G0+G1"], [(128, [128, 129], list(range(128)))])
        self.assertEqual(cases["G0+G1+G2"],
                         [(128, [128, 129, 130], list(range(128)))])
        for code, k, overrides in (("HLRC", 128, {}), ("NLRC", 96, {}),
                                   ("NLRC", 128, {"r": 2}),
                                   ("NLRC", 128, {"z": 8})):
            rejected = self.run_plan(code, k, "muti", **overrides)
            self.assertNotEqual(rejected.returncode, 0)
            message = rejected.stdout + rejected.stderr
            self.assertTrue("muti requires CodeType=NLRC, k=128, r=3, z=9" in message or
                            "Invalid NLRC parameters" in message, message)

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
        shared = list(range(16, 32)) + list(range(16)) + [64]
        self.assertEqual(cases["L0+L1+L3+U0"], [
            (70, [70, 66, 67], shared),
            (69, [69], list(range(48, 64))),
        ])
        self.assertEqual(cases["L0+L1+L2+U0"], [
            (70, [70, 66, 67], shared),
            (68, [68], list(range(32, 48)) + [65]),
        ])
        self.assertEqual(cases["L0+L1+U0+U1"], [
            (70, [70, 66, 67], shared),
            (71, [71], list(range(33, 48)) + list(range(50, 64))),
        ])
        self.assertEqual(cases["G0+G1+L1+L3"],
                         [(64, [64, 65, 67, 69], list(range(64)))])

    def test_nlrc_k64_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 64, "double"),
                                "NLRC", 64, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "L0+L4", "G0+G1"])
        self.assertEqual(cases["L0+L1"],
                         [(67, [67, 68], list(range(22)))])
        self.assertEqual(cases["L0+L2"], [
            (67, [67], list(range(22))),
            (69, [69], list(range(22, 44))),
        ])
        self.assertEqual(cases["L0+L4"], [
            (67, [67], list(range(22))),
            (71, [71], list(range(44, 64)) + [64, 65, 66]),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(64, [64, 65], list(range(64)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("NLRC", 64, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_hlrc_k128_quadruple_exact_sources_and_owners(self):
        cases=self.parse_plan(self.run_plan("HLRC",128,"quadruple"),"HLRC",128,"quadruple")
        names=["L1+L2+L3+U0","L1+L2+L3+L5","L0+L1+L2+L3","L0+L1+L2+L4","L1+L2+L5+U0","L1+L2+L4+U0","L1+L5+L6+U0","L1+L4+L5+U0","L5+L6+L7+U0","L4+L5+L6+U0","L1+L5+U0+U1","G0+G1+L1+L2"]
        self.assertEqual(list(cases),names)
        l1=list(range(16,32));l2=list(range(32,48));l3=list(range(48,64));l0=list(range(16))+[128];l4=list(range(64,80))+[129];l5=list(range(80,96));l6=list(range(96,112));l7=list(range(112,128))
        shared4=list(range(1,16))+l1+l2+l3;shared3=l1+l2+list(range(1,16))+list(range(50,64));shared2=l1+list(range(1,16))+list(range(34,48))+list(range(50,64));u0=list(range(1,16))+list(range(18,32))+list(range(34,48))+l3;sharedu1=list(range(64,80))+l5+list(range(98,112))+list(range(114,128))
        expected={"L1+L2+L3+U0":[(138,[138,131,132,133],shared4)],"L1+L2+L3+L5":[(131,[131],l1),(132,[132],l2),(133,[133],l3),(135,[135],l5)],"L0+L1+L2+L3":[(130,[130],l0),(131,[131],l1),(132,[132],l2),(133,[133],l3)],"L0+L1+L2+L4":[(130,[130],l0),(131,[131],l1),(132,[132],l2),(134,[134],l4)],"L1+L2+L5+U0":[(138,[138,131,132],shared3),(135,[135],l5)],"L1+L2+L4+U0":[(138,[138,131,132],shared3),(134,[134],l4)],"L1+L5+L6+U0":[(138,[138,131],shared2),(135,[135],l5),(136,[136],l6)],"L1+L4+L5+U0":[(138,[138,131],shared2),(134,[134],l4),(135,[135],l5)],"L5+L6+L7+U0":[(135,[135],l5),(136,[136],l6),(137,[137],l7),(138,[138],u0)],"L4+L5+L6+U0":[(134,[134],l4),(135,[135],l5),(136,[136],l6),(138,[138],u0)],"L1+L5+U0+U1":[(138,[138,131],shared2),(139,[139,135],sharedu1)],"G0+G1+L1+L2":[(128,[128,129,131,132],list(range(128)))]}
        for name,tasks in expected.items():self.assertEqual(cases[name],tasks)
        for overrides in ({"r":3},{"z":8}):self.assertNotEqual(self.run_plan("HLRC",128,"quadruple",**overrides).returncode,0)

    def test_hlrc_k128_triple_exact_sources_and_owners(self):
        cases=self.parse_plan(self.run_plan("HLRC",128,"triple"),"HLRC",128,"triple")
        self.assertEqual(list(cases),["L1+L2+L3","L0+L1+L2","L0+L1+L4","L1+L2+U0","L1+L5+U0","L1+L4+U0","L5+L6+U0","L4+L5+U0","L1+U0+U1","G0+G1+L1"])
        l1=list(range(16,32));l2=list(range(32,48));l3=list(range(48,64));l0=list(range(16))+[128];l4=list(range(64,80))+[129];l5=list(range(80,96));l6=list(range(96,112))
        shared3=l1+l2+list(range(1,16))+list(range(50,64));shared2=l1+list(range(1,16))+list(range(34,48))+list(range(50,64));u0=list(range(1,16))+list(range(18,32))+list(range(34,48))+list(range(48,64));u1=list(range(65,80))+list(range(82,96))+list(range(98,112))+list(range(114,128))
        expected={"L1+L2+L3":[(131,[131],l1),(132,[132],l2),(133,[133],l3)],"L0+L1+L2":[(130,[130],l0),(131,[131],l1),(132,[132],l2)],"L0+L1+L4":[(130,[130],l0),(131,[131],l1),(134,[134],l4)],"L1+L2+U0":[(138,[138,131,132],shared3)],"L1+L5+U0":[(138,[138,131],shared2),(135,[135],l5)],"L1+L4+U0":[(138,[138,131],shared2),(134,[134],l4)],"L5+L6+U0":[(135,[135],l5),(136,[136],l6),(138,[138],u0)],"L4+L5+U0":[(134,[134],l4),(135,[135],l5),(138,[138],u0)],"L1+U0+U1":[(138,[138,131],shared2),(139,[139],u1)],"G0+G1+L1":[(128,[128,129,131],list(range(128)))]}
        for name,tasks in expected.items():self.assertEqual(cases[name],tasks)
        for overrides in ({"r":3},{"z":8}):self.assertNotEqual(self.run_plan("HLRC",128,"triple",**overrides).returncode,0)

    def test_hlrc_k128_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 128, "double"),
                                "HLRC", 128, "double")
        self.assertEqual(list(cases), ["L1+L2", "L0+L1", "L0+L4", "L1+U0", "L5+U0", "L4+U0", "U0+U1", "G0+L0"])
        l0=list(range(16))+[128];l4=list(range(64,80))+[129]
        u0=list(range(1,16))+list(range(18,32))+list(range(34,48))+list(range(48,64))
        shared=list(range(16,32))+list(range(1,16))+list(range(34,48))+list(range(48,64))
        u1=list(range(65,80))+list(range(82,96))+list(range(98,112))+list(range(114,128))
        expected={
          "L1+L2":[(131,[131],list(range(16,32))),(132,[132],list(range(32,48)))],
          "L0+L1":[(130,[130],l0),(131,[131],list(range(16,32)))],
          "L0+L4":[(130,[130],l0),(134,[134],l4)],
          "L1+U0":[(138,[138,131],shared)],
          "L5+U0":[(135,[135],list(range(80,96))),(138,[138],u0)],
          "L4+U0":[(134,[134],l4),(138,[138],u0)],
          "U0+U1":[(138,[138],u0),(139,[139],u1)],
          "G0+L0":[(128,[128,130],list(range(128)))],}
        for name,tasks in expected.items(): self.assertEqual(cases[name],tasks)
        for overrides in ({"r":3},{"z":8}):
            self.assertNotEqual(self.run_plan("HLRC",128,"double",**overrides).returncode,0)

    def test_hlrc_k96_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 96, "quadruple"), "HLRC", 96, "quadruple")
        self.assertEqual(list(cases), ["L0+L1+L2+U0", "L1+L2+L4+U0", "L1+L2+L4+L5", "L0+L1+L2+L4", "L0+L1+L2+L3", "L1+L4+L5+U0", "L1+L3+L4+U0", "L1+L4+U0+U1", "L0+L1+L2+U1", "G0+G1+L1+L2"])
        l1=list(range(16,32));l2=list(range(32,48));l4=list(range(64,80));l5=list(range(80,96));l0=list(range(16))+[96];l3=list(range(48,64))+[97]
        shared4=list(range(16))+[96]+l1+l2;shared3=list(range(1,16))+l1+l2;shared2=l1+list(range(1,16))+list(range(34,48));shared_u1=list(range(49,64))+l4+list(range(82,96));u1=list(range(49,64))+list(range(66,80))+list(range(82,96))
        expected={
          "L0+L1+L2+U0":[(104,[104,98,99,100],shared4)],
          "L1+L2+L4+U0":[(104,[104,99,100],shared3),(102,[102],l4)],
          "L1+L2+L4+L5":[(99,[99],l1),(100,[100],l2),(102,[102],l4),(103,[103],l5)],
          "L0+L1+L2+L4":[(98,[98],l0),(99,[99],l1),(100,[100],l2),(102,[102],l4)],
          "L0+L1+L2+L3":[(98,[98],l0),(99,[99],l1),(100,[100],l2),(101,[101],l3)],
          "L1+L4+L5+U0":[(104,[104,99],shared2),(102,[102],l4),(103,[103],l5)],
          "L1+L3+L4+U0":[(104,[104,99],shared2),(101,[101],l3),(102,[102],l4)],
          "L1+L4+U0+U1":[(104,[104,99],shared2),(105,[105,102],shared_u1)],
          "L0+L1+L2+U1":[(98,[98],l0),(99,[99],l1),(100,[100],l2),(105,[105],u1)],
          "G0+G1+L1+L2":[(96,[96,97,99,100],list(range(96)))],}
        for name,tasks in expected.items(): self.assertEqual(cases[name],tasks)
        for overrides in ({"r":3},{"z":6}): self.assertNotEqual(self.run_plan("HLRC",96,"quadruple",**overrides).returncode,0)

    def test_hlrc_k96_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 96, "triple"),
                                "HLRC", 96, "triple")
        self.assertEqual(list(cases), [
            "L1+L2+U0", "L1+L2+L4", "L0+L1+L2", "L0+L1+L3",
            "L1+L4+U0", "L1+L3+U0", "L1+L2+U1", "L0+L1+U1",
            "L1+U0+U1", "G0+G1+L1",
        ])
        l1 = list(range(16, 32)); l2 = list(range(32, 48))
        l4 = list(range(64, 80)); l0 = list(range(16)) + [96]
        l3 = list(range(48, 64)) + [97]
        shared_three = l1 + l2 + list(range(1, 16))
        shared_two = l1 + list(range(1, 16)) + list(range(34, 48))
        u1 = list(range(49, 64)) + list(range(66, 80)) + list(range(82, 96))
        self.assertEqual(cases["L1+L2+U0"], [(104, [104, 99, 100], shared_three)])
        self.assertEqual(cases["L1+L2+L4"], [(99, [99], l1), (100, [100], l2), (102, [102], l4)])
        self.assertEqual(cases["L0+L1+L2"], [(98, [98], l0), (99, [99], l1), (100, [100], l2)])
        self.assertEqual(cases["L0+L1+L3"], [(98, [98], l0), (99, [99], l1), (101, [101], l3)])
        self.assertEqual(cases["L1+L4+U0"], [(104, [104, 99], shared_two), (102, [102], l4)])
        self.assertEqual(cases["L1+L3+U0"], [(104, [104, 99], shared_two), (101, [101], l3)])
        self.assertEqual(cases["L1+L2+U1"], [(99, [99], l1), (100, [100], l2), (105, [105], u1)])
        self.assertEqual(cases["L0+L1+U1"], [(98, [98], l0), (99, [99], l1), (105, [105], u1)])
        self.assertEqual(cases["L1+U0+U1"], [(104, [104, 99], shared_two), (105, [105], u1)])
        self.assertEqual(cases["G0+G1+L1"], [(96, [96, 97, 99], list(range(96)))])
        for overrides in ({"r": 3}, {"z": 6}):
            result = self.run_plan("HLRC", 96, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_hlrc_k96_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 96, "double"),
                                "HLRC", 96, "double")
        self.assertEqual(list(cases), [
            "L1+L2", "L0+L1", "L0+L3", "L1+U0",
            "L4+U0", "L3+U0", "U0+U1", "G0+L0",
        ])
        l0 = list(range(16)) + [96]
        l3 = list(range(48, 64)) + [97]
        u0 = list(range(1, 16)) + list(range(18, 32)) + list(range(34, 48))
        u1 = list(range(49, 64)) + list(range(66, 80)) + list(range(82, 96))
        shared = list(range(16, 32)) + list(range(1, 16)) + list(range(34, 48))
        self.assertEqual(cases["L1+L2"], [
            (99, [99], list(range(16, 32))), (100, [100], list(range(32, 48))),
        ])
        self.assertEqual(cases["L0+L1"], [
            (98, [98], l0), (99, [99], list(range(16, 32))),
        ])
        self.assertEqual(cases["L0+L3"], [(98, [98], l0), (101, [101], l3)])
        self.assertEqual(cases["L1+U0"], [(104, [104, 99], shared)])
        self.assertEqual(cases["L4+U0"], [
            (102, [102], list(range(64, 80))), (104, [104], u0),
        ])
        self.assertEqual(cases["L3+U0"], [(101, [101], l3), (104, [104], u0)])
        self.assertEqual(cases["U0+U1"], [(104, [104], u0), (105, [105], u1)])
        self.assertEqual(cases["G0+L0"], [(96, [96, 98], list(range(96)))])
        for overrides in ({"r": 3}, {"z": 6}):
            result = self.run_plan("HLRC", 96, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_hlrc_k64_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 64, "double"),
                                "HLRC", 64, "double")
        self.assertEqual(list(cases), [
            "L1+U0", "L1+L3", "L0+L1", "L0+L2",
            "L1+U1", "L0+U1", "U0+U1", "G0+L0",
        ])
        self.assertEqual(cases["L1+U0"], [
            (70, [70, 67], list(range(16, 32)) + list(range(1, 16))),
        ])
        self.assertEqual(cases["L1+L3"], [
            (67, [67], list(range(16, 32))),
            (69, [69], list(range(48, 64))),
        ])
        self.assertEqual(cases["L0+L1"], [
            (66, [66], list(range(16)) + [64]),
            (67, [67], list(range(16, 32))),
        ])
        self.assertEqual(cases["L0+L2"], [
            (66, [66], list(range(16)) + [64]),
            (68, [68], list(range(32, 48)) + [65]),
        ])
        u1_sources = list(range(33, 48)) + list(range(50, 64))
        self.assertEqual(cases["L1+U1"], [
            (67, [67], list(range(16, 32))), (71, [71], u1_sources),
        ])
        self.assertEqual(cases["L0+U1"], [
            (66, [66], list(range(16)) + [64]), (71, [71], u1_sources),
        ])
        self.assertEqual(cases["U0+U1"], [
            (70, [70], list(range(1, 16)) + list(range(18, 32))),
            (71, [71], u1_sources),
        ])
        self.assertEqual(cases["G0+L0"],
                         [(64, [64, 66], list(range(64)))])
        for overrides in ({"r": 3}, {"z": 5}):
            result = self.run_plan("HLRC", 64, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_hlrc_k64_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("HLRC", 64, "triple"),
                                "HLRC", 64, "triple")
        self.assertEqual(list(cases), [
            "L0+L1+U0", "L0+L3+U0", "L0+L2+U0", "L0+L1+L3",
            "L0+L1+L2", "L0+U0+U1", "L0+L1+U1", "G0+G1+L1",
        ])
        shared = list(range(18, 32)) + list(range(16)) + [64]
        shared_three = list(range(16, 32)) + list(range(16)) + [64]
        l0 = list(range(16)) + [64]
        l2 = list(range(32, 48)) + [65]
        u1 = list(range(33, 48)) + list(range(50, 64))
        self.assertEqual(cases["L0+L1+U0"], [(70, [70, 66, 67], shared_three)])
        self.assertEqual(cases["L0+L3+U0"], [
            (70, [70, 66], shared), (69, [69], list(range(48, 64))),
        ])
        self.assertEqual(cases["L0+L2+U0"], [
            (70, [70, 66], shared), (68, [68], l2),
        ])
        self.assertEqual(cases["L0+L1+L3"], [
            (66, [66], l0), (67, [67], list(range(16, 32))),
            (69, [69], list(range(48, 64))),
        ])
        self.assertEqual(cases["L0+L1+L2"], [
            (66, [66], l0), (67, [67], list(range(16, 32))), (68, [68], l2),
        ])
        self.assertEqual(cases["L0+U0+U1"], [
            (70, [70, 66], shared), (71, [71], u1),
        ])
        self.assertEqual(cases["L0+L1+U1"], [
            (66, [66], l0), (67, [67], list(range(16, 32))), (71, [71], u1),
        ])
        self.assertEqual(cases["G0+G1+L1"],
                         [(64, [64, 65, 67], list(range(64)))])

    def test_nlrc_k128_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 128, "quadruple"),
                                "NLRC", 128, "quadruple")
        self.assertEqual(list(cases), [
            "L0+L1+L2+L3", "L0+L1+L2+L4", "L0+L1+L2+L8",
            "L0+L2+L4+L6", "L0+L2+L4+L8", "G0+G1+G2+L8",
        ])
        l8_sources = list(range(104, 128)) + [128, 129, 130]
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (131, [131, 132], list(range(26))),
            (133, [133, 134], list(range(26, 52))),
        ])
        self.assertEqual(cases["L0+L1+L2+L4"], [
            (131, [131, 132], list(range(26))),
            (133, [133], list(range(26, 52))), (135, [135], list(range(52, 78))),
        ])
        self.assertEqual(cases["L0+L1+L2+L8"], [
            (131, [131, 132], list(range(26))),
            (133, [133], list(range(26, 52))), (139, [139], l8_sources),
        ])
        self.assertEqual(cases["L0+L2+L4+L6"], [
            (131, [131], list(range(26))), (133, [133], list(range(26, 52))),
            (135, [135], list(range(52, 78))), (137, [137], list(range(78, 104))),
        ])
        self.assertEqual(cases["L0+L2+L4+L8"], [
            (131, [131], list(range(26))), (133, [133], list(range(26, 52))),
            (135, [135], list(range(52, 78))), (139, [139], l8_sources),
        ])
        self.assertEqual(cases["G0+G1+G2+L8"],
                         [(128, [128, 129, 130, 139], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 7}):
            result = self.run_plan("NLRC", 128, "quadruple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_nlrc_k128_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 128, "triple"),
                                "NLRC", 128, "triple")
        self.assertEqual(list(cases), [
            "L0+L1+L2", "L0+L1+L8", "L0+L2+L4", "L0+L2+L8", "G0+G1+G2",
        ])
        self.assertEqual(cases["L0+L1+L2"], [
            (131, [131, 132], list(range(26))),
            (133, [133], list(range(26, 52))),
        ])
        l8_sources = list(range(104, 128)) + [128, 129, 130]
        self.assertEqual(cases["L0+L1+L8"], [
            (131, [131, 132], list(range(26))), (139, [139], l8_sources),
        ])
        self.assertEqual(cases["L0+L2+L4"], [
            (131, [131], list(range(26))),
            (133, [133], list(range(26, 52))),
            (135, [135], list(range(52, 78))),
        ])
        self.assertEqual(cases["L0+L2+L8"], [
            (131, [131], list(range(26))),
            (133, [133], list(range(26, 52))), (139, [139], l8_sources),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(128, [128, 129, 130], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 7}):
            result = self.run_plan("NLRC", 128, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_nlrc_k128_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 128, "double"),
                                "NLRC", 128, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "L0+L8", "G0+G1"])
        self.assertEqual(cases["L0+L1"],
                         [(131, [131, 132], list(range(26)))])
        self.assertEqual(cases["L0+L2"], [
            (131, [131], list(range(26))),
            (133, [133], list(range(26, 52))),
        ])
        self.assertEqual(cases["L0+L8"], [
            (131, [131], list(range(26))),
            (139, [139], list(range(104, 128)) + [128, 129, 130]),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(128, [128, 129], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 7}):
            result = self.run_plan("NLRC", 128, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_nlrc_k96_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 96, "quadruple"),
                                "NLRC", 96, "quadruple")
        self.assertEqual(list(cases), [
            "L0+L1+L2+L3", "L2+L3+L4+L5", "L0+L2+L4+L5",
            "L2+L3+L4+L6", "G0+G1+G2+L6",
        ])
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (99, [99, 100], list(range(24))),
            (101, [101, 102], list(range(24, 49))),
        ])
        self.assertEqual(cases["L2+L3+L4+L5"], [
            (101, [101, 102], list(range(24, 49))),
            (103, [103, 104], list(range(49, 74))),
        ])
        self.assertEqual(cases["L0+L2+L4+L5"], [
            (99, [99], list(range(24))),
            (101, [101], list(range(24, 49))),
            (103, [103, 104], list(range(49, 74))),
        ])
        self.assertEqual(cases["L2+L3+L4+L6"], [
            (101, [101, 102], list(range(24, 49))),
            (103, [103], list(range(49, 74))),
            (105, [105], list(range(74, 96)) + [96, 97, 98]),
        ])
        self.assertEqual(cases["G0+G1+G2+L6"],
                         [(96, [96, 97, 98, 105], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("NLRC", 96, "quadruple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_nlrc_k96_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 96, "triple"),
                                "NLRC", 96, "triple")
        self.assertEqual(list(cases), [
            "L0+L1+L2", "L2+L3+L4", "L0+L2+L4", "L2+L4+L6", "G0+G1+G2",
        ])
        self.assertEqual(cases["L0+L1+L2"], [
            (99, [99, 100], list(range(24))),
            (101, [101], list(range(24, 49))),
        ])
        self.assertEqual(cases["L2+L3+L4"], [
            (101, [101, 102], list(range(24, 49))),
            (103, [103], list(range(49, 74))),
        ])
        self.assertEqual(cases["L0+L2+L4"], [
            (99, [99], list(range(24))),
            (101, [101], list(range(24, 49))),
            (103, [103], list(range(49, 74))),
        ])
        self.assertEqual(cases["L2+L4+L6"], [
            (101, [101], list(range(24, 49))),
            (103, [103], list(range(49, 74))),
            (105, [105], list(range(74, 96)) + [96, 97, 98]),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(96, [96, 97, 98], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("NLRC", 96, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_nlrc_k96_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("NLRC", 96, "double"),
                                "NLRC", 96, "double")
        self.assertEqual(list(cases), ["L0+L1", "L2+L3", "L0+L2", "L2+L4", "G0+G1"])
        self.assertEqual(cases["L0+L1"],
                         [(99, [99, 100], list(range(24)))])
        self.assertEqual(cases["L2+L3"],
                         [(101, [101, 102], list(range(24, 49)))])
        self.assertEqual(cases["L0+L2"], [
            (99, [99], list(range(24))),
            (101, [101], list(range(24, 49))),
        ])
        self.assertEqual(cases["L2+L4"], [
            (101, [101], list(range(24, 49))),
            (103, [103], list(range(49, 74))),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(96, [96, 97], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("NLRC", 96, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

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

    def test_azure_k96_single_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 96, "single"),
                                "AzureLRC", 96, "single")
        self.assertEqual(list(cases), ["L0", "G0"])
        self.assertEqual(cases["L0"], [(99, [99], list(range(24)))])
        self.assertEqual(cases["G0"], [(96, [96], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("AzureLRC", 96, "single", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k128_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 128, "double"),
                                "AzureLRC", 128, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "L2+L3", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (131, [131], list(range(25))),
            (132, [132], list(range(25, 50))),
        ])
        self.assertEqual(cases["L0+L2"], [
            (131, [131], list(range(25))),
            (133, [133], list(range(50, 76))),
        ])
        self.assertEqual(cases["L2+L3"], [
            (133, [133], list(range(50, 76))),
            (134, [134], list(range(76, 102))),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(128, [128, 129], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("AzureLRC", 128, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k128_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 128, "quadruple"),
                                "AzureLRC", 128, "quadruple")
        self.assertEqual(list(cases), [
            "L0+L1+L2+L3", "L1+L2+L3+L4", "G0+G1+G2+L0",
        ])
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (131, [131], list(range(25))),
            (132, [132], list(range(25, 50))),
            (133, [133], list(range(50, 76))),
            (134, [134], list(range(76, 102))),
        ])
        self.assertEqual(cases["L1+L2+L3+L4"], [
            (132, [132], list(range(25, 50))),
            (133, [133], list(range(50, 76))),
            (134, [134], list(range(76, 102))),
            (135, [135], list(range(102, 128))),
        ])
        self.assertEqual(cases["G0+G1+G2+L0"],
                         [(128, [128, 129, 130, 131], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("AzureLRC", 128, "quadruple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k128_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 128, "triple"),
                                "AzureLRC", 128, "triple")
        self.assertEqual(list(cases), [
            "L0+L1+L2", "L1+L2+L3", "L2+L3+L4", "G0+G1+G2",
        ])
        self.assertEqual(cases["L0+L1+L2"], [
            (131, [131], list(range(25))),
            (132, [132], list(range(25, 50))),
            (133, [133], list(range(50, 76))),
        ])
        self.assertEqual(cases["L1+L2+L3"], [
            (132, [132], list(range(25, 50))),
            (133, [133], list(range(50, 76))),
            (134, [134], list(range(76, 102))),
        ])
        self.assertEqual(cases["L2+L3+L4"], [
            (133, [133], list(range(50, 76))),
            (134, [134], list(range(76, 102))),
            (135, [135], list(range(102, 128))),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(128, [128, 129, 130], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("AzureLRC", 128, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k96_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 96, "triple"),
                                "AzureLRC", 96, "triple")
        self.assertEqual(list(cases), ["L0+L1+L2", "G0+G1+G2"])
        self.assertEqual(cases["L0+L1+L2"], [
            (99, [99], list(range(24))),
            (100, [100], list(range(24, 48))),
            (101, [101], list(range(48, 72))),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(96, [96, 97, 98], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("AzureLRC", 96, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k96_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 96, "double"),
                                "AzureLRC", 96, "double")
        self.assertEqual(list(cases), ["L0+L1", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (99, [99], list(range(24))),
            (100, [100], list(range(24, 48))),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(96, [96, 97], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("AzureLRC", 96, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_azure_k64_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("AzureLRC", 64, "double"),
                                "AzureLRC", 64, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (67, [67], list(range(21))),
            (68, [68], list(range(21, 42))),
        ])
        self.assertEqual(cases["L0+L2"], [
            (67, [67], list(range(21))),
            (69, [69], list(range(42, 64))),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(64, [64, 65], list(range(64)))])
        for overrides in ({"r": 2}, {"z": 5}):
            result = self.run_plan("AzureLRC", 64, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k128_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 128, "quadruple"),
                                "OptimalLRC", 128, "quadruple")
        globals_ = [128, 129, 130]
        self.assertEqual(list(cases), [
            "L0+L1+L2+L3", "L1+L2+L3+L4", "G0+G1+G2+L0",
        ])
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (131, [131, 132, 133, 134], list(range(102)) + globals_),
        ])
        self.assertEqual(cases["L1+L2+L3+L4"], [
            (132, [132, 133, 134, 135], list(range(25, 128)) + globals_),
        ])
        self.assertEqual(cases["G0+G1+G2+L0"],
                         [(128, [128, 129, 130, 131], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("OptimalLRC", 128, "quadruple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k128_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 128, "triple"),
                                "OptimalLRC", 128, "triple")
        globals_ = [128, 129, 130]
        self.assertEqual(list(cases), [
            "L0+L1+L2", "L1+L2+L3", "L2+L3+L4", "G0+G1+G2",
        ])
        self.assertEqual(cases["L0+L1+L2"], [
            (131, [131, 132, 133], list(range(76)) + globals_),
        ])
        self.assertEqual(cases["L1+L2+L3"], [
            (132, [132, 133, 134], list(range(25, 102)) + globals_),
        ])
        self.assertEqual(cases["L2+L3+L4"], [
            (133, [133, 134, 135], list(range(50, 128)) + globals_),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(128, [128, 129, 130], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("OptimalLRC", 128, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k128_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 128, "double"),
                                "OptimalLRC", 128, "double")
        globals_ = [128, 129, 130]
        self.assertEqual(list(cases), ["L0+L1", "L0+L2", "L2+L3", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (131, [131, 132], list(range(50)) + globals_),
        ])
        self.assertEqual(cases["L0+L2"], [
            (131, [131, 133], list(range(25)) + list(range(50, 76)) + globals_),
        ])
        self.assertEqual(cases["L2+L3"], [
            (133, [133, 134], list(range(50, 102)) + globals_),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(128, [128, 129], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("OptimalLRC", 128, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k96_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 96, "triple"),
                                "OptimalLRC", 96, "triple")
        globals_ = [96, 97, 98]
        self.assertEqual(list(cases), ["L0+L1+L2", "G0+G1+G2"])
        self.assertEqual(cases["L0+L1+L2"], [
            (99, [99], list(range(24)) + globals_),
            (100, [100], list(range(24, 48)) + globals_),
            (101, [101], list(range(48, 72)) + globals_),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(96, [96, 97, 98], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("OptimalLRC", 96, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_optimal_k96_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("OptimalLRC", 96, "double"),
                                "OptimalLRC", 96, "double")
        globals_ = [96, 97, 98]
        self.assertEqual(list(cases), ["L0+L1", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (99, [99], list(range(24)) + globals_),
            (100, [100], list(range(24, 48)) + globals_),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(96, [96, 97], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("OptimalLRC", 96, "double", **overrides)
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

    def test_uniform_k128_quadruple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 128, "quadruple"),
                                "UniformLRC", 128, "quadruple")
        self.assertEqual(list(cases), [
            "L0+L1+L2+L3", "L0+L1+L2+L4", "G0+G1+G2+L4",
        ])
        self.assertEqual(cases["L0+L1+L2+L3"], [
            (131, [131], list(range(26))),
            (132, [132], list(range(26, 52))),
            (133, [133], list(range(52, 78))),
            (134, [134], list(range(78, 104))),
        ])
        self.assertEqual(cases["L0+L1+L2+L4"], [
            (131, [131], list(range(26))),
            (132, [132], list(range(26, 52))),
            (133, [133], list(range(52, 78))),
            (135, [135], list(range(104, 128)) + [128, 129, 130]),
        ])
        self.assertEqual(cases["G0+G1+G2+L4"],
                         [(128, [128, 129, 130, 135], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("UniformLRC", 128, "quadruple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_uniform_k128_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 128, "triple"),
                                "UniformLRC", 128, "triple")
        self.assertEqual(list(cases), ["L0+L1+L2", "L0+L1+L4", "G0+G1+G2"])
        self.assertEqual(cases["L0+L1+L2"], [
            (131, [131], list(range(26))),
            (132, [132], list(range(26, 52))),
            (133, [133], list(range(52, 78))),
        ])
        self.assertEqual(cases["L0+L1+L4"], [
            (131, [131], list(range(26))),
            (132, [132], list(range(26, 52))),
            (135, [135], list(range(104, 128)) + [128, 129, 130]),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(128, [128, 129, 130], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("UniformLRC", 128, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_uniform_k128_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 128, "double"),
                                "UniformLRC", 128, "double")
        self.assertEqual(list(cases), ["L0+L1", "L0+L4", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (131, [131], list(range(26))),
            (132, [132], list(range(26, 52))),
        ])
        self.assertEqual(cases["L0+L4"], [
            (131, [131], list(range(26))),
            (135, [135], list(range(104, 128)) + [128, 129, 130]),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(128, [128, 129], list(range(128)))])
        for overrides in ({"r": 2}, {"z": 4}):
            result = self.run_plan("UniformLRC", 128, "double", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_uniform_k96_triple_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 96, "triple"),
                                "UniformLRC", 96, "triple")
        self.assertEqual(list(cases), ["L0+L1+L2", "L1+L2+L3", "G0+G1+G2"])
        self.assertEqual(cases["L0+L1+L2"], [
            (99, [99], list(range(24))),
            (100, [100], list(range(24, 49))),
            (101, [101], list(range(49, 74))),
        ])
        self.assertEqual(cases["L1+L2+L3"], [
            (100, [100], list(range(24, 49))),
            (101, [101], list(range(49, 74))),
            (102, [102], list(range(74, 96)) + [96, 97, 98]),
        ])
        self.assertEqual(cases["G0+G1+G2"],
                         [(96, [96, 97, 98], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("UniformLRC", 96, "triple", **overrides)
            self.assertNotEqual(result.returncode, 0)

    def test_uniform_k96_double_exact_sources_and_owners(self):
        cases = self.parse_plan(self.run_plan("UniformLRC", 96, "double"),
                                "UniformLRC", 96, "double")
        self.assertEqual(list(cases), ["L0+L1", "L1+L2", "G0+G1"])
        self.assertEqual(cases["L0+L1"], [
            (99, [99], list(range(24))),
            (100, [100], list(range(24, 49))),
        ])
        self.assertEqual(cases["L1+L2"], [
            (100, [100], list(range(24, 49))),
            (101, [101], list(range(49, 74))),
        ])
        self.assertEqual(cases["G0+G1"],
                         [(96, [96, 97], list(range(96)))])
        for overrides in ({"r": 2}, {"z": 3}):
            result = self.run_plan("UniformLRC", 96, "double", **overrides)
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
