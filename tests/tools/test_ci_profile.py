"""Regression contracts for the CI profile and workflow evidence gate."""
from pathlib import Path
import copy
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from ctest_gate import GateError, load_manifest
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'ci'))
from verify_ci import verify_run
from prepare_test_dependencies import read_lock

TESTS = Path(__file__).resolve().parents[1]


class Fixture(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)
        self.manifest = self.folder/'manifest.json'
        self.raw = json.loads((TESTS/'ctest-manifest.json').read_text('utf-8'))

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.raw), encoding='utf-8')


class ProfileTests(Fixture):
    def test_project_profiles_include_same_complete_scope(self):
        path = Path(__file__).resolve().parents[1]/'ctest-manifest.json'
        full = load_manifest(path, profile='local-full')
        ci = load_manifest(path, profile='ci-offline')
        def names(data):
            return {(suite,c['name']) for suite,cases in data['suites'].items() for c in cases}
        self.assertEqual(len(names(full)), 836)
        self.assertEqual(names(ci), names(full))
        self.assertIn(('main','review_fix_visa_partial'), names(ci))
        for suite,cases in ci['suites'].items():
            self.assertEqual(cases, full['suites'][suite])

    def test_unknown_profile_rejected(self):
        self.save_manifest()
        with self.assertRaises(GateError):
            load_manifest(self.manifest, 'automatic')

    def test_full_cannot_exclude(self):
        self.raw['profiles']['local-full']['exclude'] = [{'suite':'main','name':'review_fix_visa_partial'}]
        self.save_manifest()
        with self.assertRaises(GateError):
            load_manifest(self.manifest)

    def test_any_ci_exclusion_rejected(self):
        original = copy.deepcopy(self.raw)
        for exclusions in ([{'suite':'main','name':'review_fix_visa_partial'}], [{'suite':'main','name':'unrelated'}]):
            with self.subTest(exclusions=exclusions):
                self.raw = copy.deepcopy(original)
                self.raw['profiles']['ci-offline']['exclude'] = exclusions
                self.save_manifest()
                with self.assertRaises(GateError):
                    load_manifest(self.manifest, 'ci-offline')

    def test_missing_required_suite_rejected(self):
        for suite in ('main','ocr','wfm'):
            with self.subTest(suite=suite):
                cases = self.raw['suites'].pop(suite)
                self.save_manifest()
                with self.assertRaises(GateError):
                    load_manifest(self.manifest, 'ci-offline')
                self.raw['suites'][suite] = cases


class EvidenceTests(Fixture):
    def setUp(self):
        super().setUp()
        self.save_manifest()
        shutil.copyfile(self.manifest, self.folder/'ctest-manifest.json')
        data = load_manifest(self.manifest, 'ci-offline')
        self.summary = {'status':'PASS', 'profile':'ci-offline', 'head':'tested-sha',
                        'expected':836, 'reported':836, 'excluded':[],
                        'local_supplement':{'required':False,'status':'INCLUDED','cases':[]},
                        'suites':{}, 'qt_results':{}}
        lock = read_lock()
        self.summary['wfm_fixture_hashes'] = {x['path']:x['sha256'] for x in lock['wfm']['files']}
        self.summary['visa_header_hashes'] = {k:v['sha256'] for k,v in lock['visa']['headers'].items()}
        (self.folder/'qt').mkdir()
        for suite,cases in data['suites'].items():
            self.summary['suites'][suite] = {'status':'PASS','expected':len(cases),'reported':len(cases)}
            tree = ET.Element('testsuite', tests=str(len(cases)))
            for case in cases:
                ET.SubElement(tree, 'testcase', name=case['name'], status='run')
                if 'qt_functions' in case:
                    qt = ET.Element('TestCase')
                    rows = case.get('qt_cases', [{'function':f,'tag':''} for f in case['qt_functions']])
                    for function in case['qt_functions']:
                        element = ET.SubElement(qt, 'TestFunction', name=function)
                        for row in rows:
                            if row['function'] == function:
                                incident = ET.SubElement(element, 'Incident', type='pass')
                                ET.SubElement(incident, 'DataTag').text = row['tag']
                    report = 'qt/'+case['name']+'-fixture.xml'
                    ET.ElementTree(qt).write(self.folder/report)
                    if 'qt_cases' in case:
                        self.summary['qt_results'][suite+'/'+case['name']] = {
                            'expected':len(rows),'reported':len(rows),'report':report}
            ET.ElementTree(tree).write(self.folder/(suite+'-tests.xml'))
        self.save_summary()

    def save_summary(self):
        (self.folder/'summary.json').write_text(json.dumps(self.summary), encoding='utf-8')

    def verify(self):
        return verify_run(self.folder, self.manifest, 'tested-sha')

    def test_complete_ci_evidence_accepted(self):
        self.assertEqual(self.verify()['reported'], 836)

    def test_wrong_profile_sha_status_or_total_rejected(self):
        for field,value in [('profile','local-full'),('head','stale-sha'),('status','FAIL'),('reported',832)]:
            with self.subTest(field=field):
                old = self.summary[field]
                self.summary[field] = value
                self.save_summary()
                with self.assertRaises(GateError):
                    self.verify()
                self.summary[field] = old

    def test_missing_report_rejected(self):
        (self.folder/'ocr-tests.xml').unlink()
        with self.assertRaises(OSError):
            self.verify()

    def test_skipped_case_cannot_be_hidden_by_pass_summary(self):
        path = self.folder/'ocr-tests.xml'
        tree = ET.parse(path)
        ET.SubElement(tree.getroot().find('testcase'), 'skipped')
        tree.write(path)
        with self.assertRaises(GateError):
            self.verify()

    def test_missing_case_cannot_be_hidden_by_pass_summary(self):
        path = self.folder/'ocr-tests.xml'
        tree = ET.parse(path)
        tree.getroot().remove(tree.getroot().find('testcase'))
        tree.write(path)
        with self.assertRaises(GateError):
            self.verify()

    def test_visa_cannot_be_left_as_pending(self):
        self.summary['local_supplement']['status'] = 'NOT_RUN'
        self.save_summary()
        with self.assertRaises(GateError):
            self.verify()

    def test_changed_manifest_rejected(self):
        self.manifest.write_text(self.manifest.read_text('utf-8')+'\n', encoding='utf-8')
        with self.assertRaises(GateError):
            self.verify()

    def test_inner_wfm_rows_cannot_be_hidden_by_pass_summary(self):
        path = self.folder/'qt/wfmviewer.all-fixture.xml'
        tree = ET.parse(path)
        group = tree.getroot().find("./TestFunction[@name='encodings']")
        group.remove(group.find('Incident'))
        tree.write(path)
        with self.assertRaises(GateError):
            self.verify()

    def test_missing_or_duplicate_qt_report_rejected(self):
        path = self.folder/'qt/wfmviewer.all-fixture.xml'
        original = path.read_bytes()
        path.unlink()
        with self.assertRaises(GateError):
            self.verify()
        path.write_bytes(original)
        (path.parent/'wfmviewer.all-extra.xml').write_bytes(original)
        with self.assertRaises(GateError):
            self.verify()

    def test_wrong_dependency_hash_rejected(self):
        for name in ('wfm_fixture_hashes', 'visa_header_hashes'):
            with self.subTest(name=name):
                original = self.summary[name]
                self.summary[name] = {}
                self.save_summary()
                with self.assertRaises(GateError):
                    self.verify()
                self.summary[name] = original


class WorkflowGateTests(unittest.TestCase):
    def run_gate(self, **overrides):
        env = dict(os.environ, OFFLINE_JOB_RESULT='success', OFFLINE_VERIFIED='ci-offline',
                   OFFLINE_TESTED_SHA='tested-sha', OFFLINE_EXPECTED_SHA='tested-sha')
        env.update(overrides)
        shell = shutil.which('pwsh') or shutil.which('powershell')
        self.assertIsNotNone(shell, 'PowerShell required; cannot skip workflow gate tests')
        result = subprocess.run([shell,'-NoProfile','-NonInteractive','-File', str(TESTS/'ci/finish-gate.ps1')],
                                env=env, capture_output=True, timeout=30)
        # Keep reversible byte representations in the captured regression log.
        print(json.dumps({'workflow_gate': {'overrides': overrides, 'returncode': result.returncode,
                          'stdout_bytes': repr(result.stdout), 'stderr_bytes': repr(result.stderr)}}), flush=True)
        return result

    def test_success_requires_complete_evidence(self):
        result = self.run_gate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn(b'ci-check PASS', result.stdout)

    def test_failure_cancelled_skipped_and_missing_job_rejected(self):
        for status in ('failure','cancelled','skipped',''):
            with self.subTest(status=status):
                result = self.run_gate(OFFLINE_JOB_RESULT=status)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'ci-check failed:', result.stderr)

    def test_missing_wrong_profile_or_stale_evidence_rejected(self):
        for changes in ({'OFFLINE_VERIFIED':''},{'OFFLINE_VERIFIED':'local-full'},
                        {'OFFLINE_TESTED_SHA':''},{'OFFLINE_TESTED_SHA':'stale-sha'}):
            with self.subTest(changes=changes):
                result = self.run_gate(**changes)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b'ci-check failed:', result.stderr)


if __name__ == '__main__':
    unittest.main(verbosity=2)
