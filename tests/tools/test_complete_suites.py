"""Regression tests for complete software-test inventory and nested results."""
from pathlib import Path
import json
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from ctest_gate import GateError, check_qt_xml, load_manifest

TESTS = Path(__file__).resolve().parents[1]


class CompleteSuiteTests(unittest.TestCase):
    def test_both_profiles_include_visa_and_wfm(self):
        for profile in ("local-full", "ci-offline"):
            with self.subTest(profile=profile):
                data = load_manifest(TESTS/"ctest-manifest.json", profile)
                self.assertEqual(set(data["suites"]), {"main", "ocr", "wfm"})
                self.assertEqual(data["excluded"], [])
                self.assertEqual(sum(map(len, data["suites"].values())), 835)
                self.assertIn("review_fix_visa_partial", [c["name"] for c in data["suites"]["main"]])

    def test_wfm_locks_all_101_inner_results(self):
        data = json.loads((TESTS/"ctest-manifest.json").read_text("utf-8"))
        case = data["suites"]["wfm"][0]
        self.assertEqual(len(case["qt_cases"]), 101)
        self.assertEqual(len({(c["function"], c["tag"]) for c in case["qt_cases"]}), 101)

    def test_nested_results_reject_missing_extra_duplicate_skip_and_failure(self):
        case = load_manifest(TESTS/"ctest-manifest.json")["suites"]["wfm"][0]
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/"qt.xml"
            for mutation in ("valid", "missing", "extra", "duplicate", "skip", "failure"):
                root = ET.Element("TestCase")
                for function in case["qt_functions"]:
                    element = ET.SubElement(root, "TestFunction", name=function)
                    for row in case["qt_cases"]:
                        if row["function"] == function:
                            incident = ET.SubElement(element, "Incident", type="pass")
                            ET.SubElement(incident, "DataTag").text = row["tag"]
                group = root.find("./TestFunction[@name='encodings']")
                if mutation == "missing":
                    group.remove(group.find("Incident"))
                elif mutation in ("extra", "duplicate"):
                    incident = ET.SubElement(group, "Incident", type="pass")
                    ET.SubElement(incident, "DataTag").text = "unreviewed" if mutation == "extra" else "0-BE"
                elif mutation in ("skip", "failure"):
                    group.find("Incident").set("type", mutation)
                ET.ElementTree(root).write(path)
                with self.subTest(mutation=mutation):
                    if mutation == "valid":
                        self.assertEqual(check_qt_xml(path, case["qt_functions"], case["qt_cases"]), 101)
                    else:
                        with self.assertRaises(GateError):
                            check_qt_xml(path, case["qt_functions"], case["qt_cases"])

    def test_duplicate_expected_data_row_rejected(self):
        raw = json.loads((TESTS/"ctest-manifest.json").read_text("utf-8"))
        rows = raw["suites"]["wfm"][0]["qt_cases"]
        rows.append(rows[-1])
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp)/"manifest.json"
            path.write_text(json.dumps(raw), encoding="utf-8")
            with self.assertRaises(GateError):
                load_manifest(path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
