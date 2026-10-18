"""Exercise the gate against real CTest processes and QtTest skip, without product changes."""
import argparse
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
import uuid
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from ctest_gate import GateError, check_inventory, check_junit, check_qt_xml, load_manifest, write_policy
from run_offline_tests import Run, group_reports, run_suite
import run_offline_tests as runner

HERE = Path(__file__).resolve().parent


def case(name='gate_case'):
    return {'name': name, 'command': ['{cmake}', '-E', 'true'],
            'labels': ['domain.fixture', 'env.offline'], 'timeout': 3,
            'properties': {}, 'working_directory': '{build}'}


class GateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.output = OPTIONS.output.resolve()/uuid.uuid4().hex[:10]
        cls.output.mkdir(parents=True)
        cls.env = os.environ.copy()
        cls.env['PATH'] = os.pathsep.join([str(OPTIONS.qt_prefix/'bin'),str(OPTIONS.compiler.parent), str(OPTIONS.ninja.parent), cls.env.get('PATH','')])
        cls.env['PYTHONDONTWRITEBYTECODE'] = '1'
        cls.qt_build = cls.output/'qt-build'
        run = Run(cls.output, cls.env)
        run.step('qt-probe-configure', [OPTIONS.cmake,'-S',HERE/'fixtures','-B',cls.qt_build,'-G','Ninja',
                 '-DCMAKE_PREFIX_PATH='+OPTIONS.qt_prefix.as_posix(),'-DCMAKE_CXX_COMPILER='+OPTIONS.compiler.as_posix(),
                 '-DCMAKE_MAKE_PROGRAM='+OPTIONS.ninja.as_posix()])
        run.step('qt-probe-build',[OPTIONS.cmake,'--build',cls.qt_build])

    def setUp(self):
        self.folder = self.output/self._testMethodName
        self.folder.mkdir()
        self.build = self.folder/'build'
        self.source = self.folder/'source'
        self.source.mkdir()
        self.manifest = self.folder/'manifest.json'
        self.data = {'version':3, 'profiles':{'local-full':{'exclude':[]},'ci-offline':{'exclude':[]}},
                     'suites':{'main':[case()], 'ocr':[case('ocr_case')], 'wfm':[case('wfm_case')]}}
        self.save_manifest()

    def save_manifest(self):
        self.manifest.write_text(json.dumps(self.data), encoding='utf-8')

    def configure(self, body='', command='true', name='gate_case', extra=''):
        test = f'add_test(NAME {name} COMMAND "${{CMAKE_COMMAND}}" -E {command})\nset_tests_properties({name} PROPERTIES LABELS "domain.fixture;env.offline" TIMEOUT 3)\n' if name else ''
        (self.source/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.21)\nproject(GateFixture NONE)\nenable_testing()\n'+test+body+'\n'+extra, encoding='utf-8')
        result = subprocess.run([str(OPTIONS.cmake),'-S',str(self.source),'-B',str(self.build),'-G','Ninja','-DCMAKE_MAKE_PROGRAM='+OPTIONS.ninja.as_posix()],env=self.env,capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)

    def inventory(self):
        result = subprocess.run([str(OPTIONS.ctest),'--test-dir',str(self.build),'--show-only=json-v1'],capture_output=True)
        self.assertEqual(result.returncode,0)
        inventory = json.loads(result.stdout)
        (self.folder/'inventory.json').write_text(json.dumps(inventory,indent=2), encoding='utf-8')
        return inventory

    def check_inventory(self, inventory):
        check_inventory(self.data['suites']['main'],inventory,self.folder,self.build,OPTIONS.cmake,self.manifest,'main')

    def execute(self):
        self.junit = self.folder/'results.xml'
        result = subprocess.run([str(OPTIONS.ctest),'--test-dir',str(self.build),'--no-tests=error','--output-junit',str(self.junit)],env=self.env,capture_output=True)
        (self.folder/'ctest.log').write_bytes(result.stdout+result.stderr)
        return result

    def test_valid_complete_run(self):
        self.configure()
        self.check_inventory(self.inventory())
        self.assertEqual(self.execute().returncode,0)
        self.assertEqual(len(check_junit(self.data['suites']['main'],self.junit)),1)

    def test_default_sdk_replaces_cached_custom_sdk(self):
        # Exercise the real runner's resolution and CMake cache, without compiling
        # a product or interpreting these deliberately different bytes as headers.
        custom, default = self.folder/'custom-sdk', self.folder/'default-sdk'
        for directory, content in ((custom, b'custom provenance'), (default, b'default provenance')):
            directory.mkdir()
            for name in ('visa.h', 'visatype.h', 'vpptype.h'):
                (directory/name).write_bytes(content)
        (self.source/'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.21)\nproject(SdkCache NONE)\n'
            'set(ELECTRONICATE_VISA_INCLUDE "" CACHE PATH "SDK include")\n'
            'file(READ "${ELECTRONICATE_VISA_INCLUDE}/visa.h" sdk)\n'
            'file(WRITE "${CMAKE_BINARY_DIR}/configured-header.txt" "${sdk}")\n', encoding='utf-8')
        args = ['runner', '--build-root', str(self.build), '--qt-prefix', str(OPTIONS.qt_prefix),
                '--compiler', str(OPTIONS.compiler), '--cmake', str(OPTIONS.cmake),
                '--ctest', str(OPTIONS.ctest), '--ninja', str(OPTIONS.ninja),
                '--wfm-fixtures', str(self.folder)]
        def versions(run, name, command, timeout=1800):
            log = run.folder/(name+'.log')
            log.write_text('version probe stub; real CMake is exercised below\n', encoding='utf-8')
            return log
        def configure(run, cases, suite, source, build, root, manifest, cmake, ctest, options, parallel, profile):
            if suite == 'main':
                result = subprocess.run([str(cmake), '-S', str(self.source), '-B', str(build), *options],
                                        env=run.env, capture_output=True)
                (run.folder/'sdk-configure.log').write_bytes(result.stdout+result.stderr)
                self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            return {'status':'PASS', 'expected':0, 'reported':0}
        with patch.object(runner, 'DEFAULT_VISA_INCLUDE', default), \
             patch.object(runner, 'load_manifest', return_value={
                 'suites':{'main':[], 'ocr':[], 'wfm':[]}, 'excluded':[]}), \
             patch.object(runner, 'validate_wfm', return_value={}), \
             patch.object(runner, 'run_suite', side_effect=configure), \
             patch.object(Run, 'step', versions):
            for explicit, expected in ((True, custom), (False, default)):
                with self.subTest(explicit=explicit), patch.object(sys, 'argv',
                        args+(['--visa-include', str(custom)] if explicit else [])):
                    self.assertEqual(runner.main(), 0)
                    configured = (self.build/'main/configured-header.txt').read_bytes()
                    self.assertEqual(configured, (expected/'visa.h').read_bytes())
                    reports = sorted((self.build/'runs').glob('*/summary.json'),
                                     key=lambda path: path.stat().st_mtime_ns)
                    report = json.loads(reports[-1].read_text('utf-8'))
                    self.assertEqual(report['visa_header_hashes']['visa.h'],
                                     hashlib.sha256(configured).hexdigest())

    def test_missing_and_extra_case(self):
        self.configure(name='unexpected')
        self.assertEqual(self.execute().returncode,0)  # CTest alone is green.
        with self.assertRaises(GateError): self.check_inventory(self.inventory())
        with self.assertRaises(GateError): check_junit(self.data['suites']['main'],self.junit)

    def test_zero_tests(self):
        self.configure(name=None)
        self.assertNotEqual(self.execute().returncode,0)
        with self.assertRaises(GateError): self.check_inventory(self.inventory())

    def test_failed_process(self):
        self.configure(command='false')
        self.assertNotEqual(self.execute().returncode,0)
        with self.assertRaises(GateError): check_junit(self.data['suites']['main'],self.junit)

    def test_timeout(self):
        self.configure(command='sleep 5')
        self.assertNotEqual(self.execute().returncode,0)
        self.assertIn('Timeout', (self.folder/'ctest.log').read_text())
        with self.assertRaises(GateError): check_junit(self.data['suites']['main'],self.junit)

    def test_ctest_skip_exit_zero(self):
        self.configure(command='false',body='set_tests_properties(gate_case PROPERTIES SKIP_RETURN_CODE 1)')
        self.assertEqual(self.execute().returncode,0)
        with self.assertRaises(GateError): self.check_inventory(self.inventory())
        with self.assertRaises(GateError): check_junit(self.data['suites']['main'],self.junit)

    def test_disabled(self):
        self.configure(body='set_tests_properties(gate_case PROPERTIES DISABLED TRUE)')
        self.execute()
        with self.assertRaises(GateError): self.check_inventory(self.inventory())
        with self.assertRaises(GateError): check_junit(self.data['suites']['main'],self.junit)

    def test_changed_command_labels_timeout(self):
        self.configure()
        original = self.inventory()
        for mutation in ('command','LABELS','TIMEOUT','WILL_FAIL'):
            with self.subTest(mutation=mutation):
                data = copy.deepcopy(original)
                if mutation=='command': data['tests'][0]['command'].append('unreviewed')
                else:
                    data['tests'][0]['properties'] = [p for p in data['tests'][0]['properties'] if p['name']!=mutation]
                    data['tests'][0]['properties'].append({'name':mutation,'value':True if mutation=='WILL_FAIL' else 0})
                with self.assertRaises(GateError): self.check_inventory(data)

    def test_duplicate_inventory(self):
        self.configure()
        data = self.inventory()
        data['tests'].append(data['tests'][0])
        with self.assertRaises(GateError): self.check_inventory(data)

    def test_duplicate_missing_partial_or_corrupt_results(self):
        self.configure()
        self.assertEqual(self.execute().returncode,0)
        original = self.junit.read_bytes()
        for mutation in ('duplicate','missing','notrun','corrupt'):
            with self.subTest(mutation=mutation):
                root = ET.fromstring(original)
                if mutation=='duplicate': root.append(copy.deepcopy(root.find('testcase')))
                elif mutation=='missing': root.remove(root.find('testcase'))
                elif mutation=='notrun': root.find('testcase').set('status','notrun')
                self.junit.write_bytes(b'broken' if mutation=='corrupt' else ET.tostring(root))
                with self.assertRaises((GateError,ET.ParseError)): check_junit(self.data['suites']['main'],self.junit)

    def test_policy_requires_sdk_case_and_nonempty_suite(self):
        names = self.folder/'registered.txt'
        names.write_text('gate_case\n')
        self.data['suites']['main'].append(case('review_fix_visa_partial'))
        self.save_manifest()
        with self.assertRaisesRegex(GateError,'review_fix_visa_partial'):
            write_policy(self.manifest,'main',names,self.folder/'properties.cmake')
        self.data['suites']['main']=[]
        self.save_manifest()
        with self.assertRaises(GateError): load_manifest(self.manifest)

    def test_groups_overlap_without_total_inflation(self):
        self.configure()
        self.execute()
        self.data['suites']['main'][0]['labels'].append('domain.second')
        groups=group_reports(self.data['suites']['main'],self.junit,self.folder,'main')
        self.assertEqual({k:v['reported'] for k,v in groups.items()},{'domain.fixture':1,'domain.second':1})
        self.assertEqual(len(check_junit(self.data['suites']['main'],self.junit)),1)

    def run_pipeline(self):
        return run_suite(Run(self.folder,self.env),self.data['suites']['main'],'main',self.source,self.build,
                         self.folder,self.manifest,OPTIONS.cmake,OPTIONS.ctest,
                         ['-G','Ninja','-DCMAKE_MAKE_PROGRAM='+OPTIONS.ninja.as_posix()],1)

    def test_pipeline_configure_failure(self):
        (self.source/'CMakeLists.txt').write_text('message(FATAL_ERROR "Intentional configure error")')
        self.assertEqual(self.run_pipeline()['status'],'FAIL')
        self.assertFalse((self.folder/'main-test.log').exists())

    def test_pipeline_build_failure(self):
        self.configure(extra='add_custom_target(broken ALL COMMAND "${CMAKE_COMMAND}" -E false)')
        self.assertEqual(self.run_pipeline()['status'],'FAIL')
        self.assertFalse((self.folder/'main-test.log').exists())

    def test_pipeline_pass_and_stale_report_rejected(self):
        self.configure()
        self.assertEqual(self.run_pipeline()['status'],'PASS')
        # Same output folder cannot use its existing successful report again.
        self.assertEqual(self.run_pipeline()['status'],'FAIL')

    def qt_command(self, method, report):
        return [str(self.qt_build/'qt_gate_probe.exe'),method,'-o',str(report)+',xml']

    def test_qttest_skip_zero_exit_is_rejected(self):
        report=self.folder/'raw-skip.xml'
        result=subprocess.run(self.qt_command('skips',report),env=self.env,capture_output=True)
        self.assertEqual(result.returncode,0)
        with self.assertRaises(GateError): check_qt_xml(report,['initTestCase','skips','cleanupTestCase'])
        self.data['suites']['main'][0]['qt_functions']=['initTestCase','skips','cleanupTestCase']
        self.save_manifest()
        env=dict(self.env,ELECTRONICATE_QT_REPORT_DIR=str(self.folder/'qt'))
        result=subprocess.run([sys.executable,str(HERE/'run_qttest.py'),'--manifest',str(self.manifest),'--suite','main','--case','gate_case','--',str(self.qt_build/'qt_gate_probe.exe'),'skips'],env=env,capture_output=True)
        (self.folder/'wrapper.log').write_bytes(result.stdout+result.stderr)
        self.assertNotEqual(result.returncode,0)

    def test_qttest_pass_and_missing_method(self):
        report=self.folder/'pass.xml'
        result=subprocess.run(self.qt_command('passes',report),env=self.env,capture_output=True)
        self.assertEqual(result.returncode,0)
        check_qt_xml(report,['initTestCase','passes','cleanupTestCase'])
        with self.assertRaises(GateError): check_qt_xml(report,['initTestCase','passes','missing','cleanupTestCase'])


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('cmake','ctest','qt-prefix','compiler','ninja','output'):
        parser.add_argument('--'+name,required=True,type=Path)
    OPTIONS=parser.parse_args()
    result=unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromTestCase(GateTests))
    if hasattr(GateTests,'output'):
        (GateTests.output/'summary.json').write_text(json.dumps({'tests':result.testsRun,'failures':len(result.failures),'errors':len(result.errors),'skipped':len(result.skipped),'status':'PASS' if result.wasSuccessful() else 'FAIL'},indent=2)+'\n',encoding='utf-8')
        print('Evidence:',GateTests.output)
    sys.exit(0 if result.wasSuccessful() else 1)
