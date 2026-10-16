import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from generate_xml_from_ini import parse_code_parameters, validate_node_capacity


class ParameterConfigurationTest(unittest.TestCase):
    def parse(self, body):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "parameters.xml"
            path.write_text(f"<Configuration>{body}</Configuration>", encoding="utf-8")
            return parse_code_parameters(path)

    def test_lotus_108(self):
        self.assertEqual(self.parse("<CodeType>LotusLRC</CodeType><k>96</k><r>4</r><z>8</z>"),
                         ("LotusLRC", 96, 4, 8, 108))

    def test_smaller_non_uni(self):
        self.assertEqual(self.parse("<CodeType>AzureLRC</CodeType><k>8</k><r>2</r><z>2</z>"),
                         ("AzureLRC", 8, 2, 2, 12))

    def test_al_valid(self):
        self.assertEqual(self.parse("<CodeType>AL</CodeType><k>16</k><r>4</r><z>7</z>"),
                         ("AL", 16, 4, 7, 27))

    def test_al_even_z_fails(self):
        with self.assertRaisesRegex(ValueError, "positive odd"):
            self.parse("<CodeType>AL</CodeType><k>8</k><r>2</r><z>4</z>")

    def test_al_k_not_divisible_by_group_count_fails(self):
        with self.assertRaisesRegex(ValueError, "required divisible"):
            self.parse("<CodeType>AL</CodeType><k>10</k><r>2</r><z>7</z>")

    def test_uni_derivation(self):
        self.assertEqual(self.parse("<CodeType>UniLRC</CodeType><alpha>2</alpha><z>4</z>"),
                         ("UniLRC", 24, 8, 4, 36))

    def test_equal_capacity_succeeds(self):
        validate_node_capacity(12, "AzureLRC", 12)

    def test_insufficient_capacity_reports_context(self):
        with self.assertRaisesRegex(ValueError, r"actual=11, required=12"):
            validate_node_capacity(11, "AzureLRC", 12)

    def test_unknown_code_type_fails(self):
        with self.assertRaisesRegex(ValueError, "unsupported code type"):
            self.parse("<CodeType>Unknown</CodeType><k>8</k><r>2</r><z>2</z>")

    def test_missing_field_fails(self):
        with self.assertRaisesRegex(ValueError, "invalid r.*actual=None"):
            self.parse("<CodeType>OptimalLRC</CodeType><k>8</k><z>2</z>")

    def test_dynamic_parameter_channel_is_absent(self):
        project = Path(__file__).resolve().parents[1]
        sources = [
            project / "src/proto/coordinator.proto",
            project / "include/client.h",
            project / "include/coordinator.h",
            project / "src/client.cpp",
            project / "src/coordinator.cpp",
        ]
        combined = "\n".join(path.read_text(encoding="utf-8") for path in sources)
        for removed_name in ("setParameter", "SetParameterByGrpc", "m_encode_parameters"):
            self.assertNotIn(removed_name, combined)

    def test_generator_has_no_fixed_stripe_width(self):
        source = Path(__file__).with_name("generate_xml_from_ini.py").read_text(encoding="utf-8")
        self.assertNotIn("len(layout.nodes) < 108", source)
        self.assertIn("parse_code_parameters(PARAM_XML_PATH)", source)


if __name__ == "__main__":
    unittest.main()
