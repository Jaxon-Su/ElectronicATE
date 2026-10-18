"""Public bootstrap contracts; no network or external SDK is used by these tests."""
from pathlib import Path
import os
import json
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'ci'))
from bootstrap_toolchain import clean_environment, validate_versions, run_checked, install
from run_ci import load_toolchain


class BootstrapTests(unittest.TestCase):
    def test_existing_qt_python_and_cmake_paths_do_not_leak(self):
        source = dict(PATH='C:/Qt/bin;C:/old-python', PYTHONPATH='C:/old-python/lib',
                      PYTHONHOME='C:/old-python', QT_PLUGIN_PATH='C:/Qt/plugins',
                      QTDIR='C:/Qt', CMAKE_PREFIX_PATH='C:/Qt', VIRTUAL_ENV='C:/old-venv',
                      COMPILER_PATH='C:/Qt/libexec', CXXFLAGS='-IC:/Qt/include',
                      CFLAGS='-IC:/Qt/include', LDFLAGS='-LC:/Qt/lib',
                      CPPFLAGS='-IC:/Qt/include', PKG_CONFIG_PATH='C:/Qt/lib/pkgconfig')
        env = clean_environment(source, ['C:/isolated/bin'])
        self.assertEqual(env['PATH'], 'C:/isolated/bin')
        for key in ('PYTHONPATH','PYTHONHOME','QT_PLUGIN_PATH','QTDIR','CMAKE_PREFIX_PATH','VIRTUAL_ENV',
                    'COMPILER_PATH','CXXFLAGS','CFLAGS','LDFLAGS','CPPFLAGS','PKG_CONFIG_PATH'):
            self.assertNotIn(key, env)
        self.assertEqual(env['PYTHONNOUSERSITE'], '1')

    def test_wrong_compiler_or_qt_version_is_rejected(self):
        good = {'qt':'6.12.0','compiler':'13.1.0','cmake':'cmake version 3.30.5',
                'ctest':'ctest version 3.30.5','ninja':'1.13.2.git.kitware.jobserver-pipe-1'}
        validate_versions(good)
        for key,bad in [('qt','6.10.0'),('compiler','15.1.0'),('cmake','cmake version 3.21.0')]:
            with self.subTest(key=key), self.assertRaises(ValueError):
                validate_versions(dict(good, **{key:bad}))

    def test_failed_command_has_evidence_and_does_not_return_success(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            with self.assertRaises(ValueError):
                run_checked(folder, 'expected-failure', [sys.executable,'-c','raise SystemExit(7)'], os.environ.copy())
            entry=json.loads((folder/'commands.jsonl').read_text('utf-8').strip())
            self.assertEqual(entry['exit_code'],7)
            self.assertFalse((folder/'toolchain.json').exists())

    def test_existing_destination_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp);sentinel=folder/'toolchain.json';sentinel.write_text('old evidence')
            with self.assertRaises(ValueError):
                install(folder)
            self.assertEqual(sentinel.read_text(),'old evidence')

    def test_interrupted_command_preserves_evidence_and_propagates(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)
            with patch('bootstrap_toolchain.subprocess.run', side_effect=KeyboardInterrupt), self.assertRaises(KeyboardInterrupt):
                run_checked(folder, 'interrupted', [sys.executable,'-c','pass'], os.environ.copy())
            entry=json.loads((folder/'commands.jsonl').read_text('utf-8').strip())
            self.assertEqual(entry['exit_code'],-1)
            self.assertEqual(entry['error'],'KeyboardInterrupt')
            self.assertTrue((folder/'interrupted.log').exists())

    def test_bootstrap_failure_preserves_fail_without_success_marker(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp)/'new-toolchain'
            # Real process creation fails at the external interpreter boundary, before any network work.
            with patch.object(sys, 'executable', str(Path(tmp)/'absent-python.exe')):
                with self.assertRaises(ValueError):
                    install(folder)
            self.assertFalse((folder/'toolchain.json').exists())
            self.assertEqual(json.loads((folder/'evidence/bootstrap.json').read_text())['status'],'FAIL')

    def test_existing_destination_cli_exits_nonzero(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp);(folder/'keep.txt').write_text('keep')
            result=subprocess.run([sys.executable,'-B',str(Path(__file__).resolve().parents[1]/'ci/bootstrap_toolchain.py'),
                                   '--root',tmp],capture_output=True,text=True,timeout=20)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual((folder/'keep.txt').read_text(),'keep')

    def test_ci_rejects_unfinished_toolchain(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'toolchain.json';path.write_text('{"status":"FAIL"}')
            with self.assertRaises(ValueError):
                load_toolchain(path)

    def test_ci_rejects_external_tool_path(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'toolchain.json'
            data={'status':'PASS','tools':{'python':sys.executable},'versions':{
                'qt':'6.12.0','compiler':'13.1.0','cmake':'cmake version 3.30.5',
                'ctest':'ctest version 3.30.5','ninja':'1.13.2.git.kitware.jobserver-pipe-1'}}
            path.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                load_toolchain(path)


if __name__=='__main__':
    unittest.main(verbosity=2)
