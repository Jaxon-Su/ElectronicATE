"""Configure, build and verify all software suites; only a complete run can report PASS."""
import argparse
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time
import uuid
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from ctest_gate import GateError, check_inventory, check_junit, check_qt_xml, load_manifest, require
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'ci'))
from prepare_test_dependencies import validate_wfm

DEFAULT_VISA_INCLUDE = Path('C:/Program Files (x86)/IVI Foundation/VISA/WinNT/Include')


class Run:
    def __init__(self, folder, env):
        self.folder = folder
        self.env = env
        self.commands = []

    def step(self, name, command, timeout=1800):
        log = self.folder / (name + '.log')
        start = time.monotonic()
        code = -1
        error = None
        print(f'{name} ...', flush=True)
        try:
            with log.open('wb') as stream:
                result = subprocess.run([str(x) for x in command], env=self.env, stdout=stream,
                                        stderr=subprocess.STDOUT, timeout=timeout)
                code = result.returncode
        except (OSError, subprocess.TimeoutExpired) as exc:
            error = str(exc)
        finally:
            entry = {'phase': name, 'command': [str(x) for x in command], 'exit_code': code,
                     'seconds': round(time.monotonic()-start, 3), 'log': str(log), 'error': error}
            self.commands.append(entry)
            with (self.folder/'commands.jsonl').open('a', encoding='utf-8') as stream:
                stream.write(json.dumps(entry, ensure_ascii=False)+'\n')
        require(code == 0, f'{name} failed: exit={code}, {error or log}')
        return log


def group_reports(cases, junit, folder, suite):
    tests = ET.parse(junit).getroot().findall('testcase')
    # Duplicate/unexpected results remain visible here and are rejected by check_junit.
    by_name = {case['name']: case for case in cases}
    groups = {}
    for case in cases:
        for label in case['labels']:
            if label.startswith('domain.'):
                groups.setdefault(label, [])
    summary = {}
    for label in groups:
        members = [t for t in tests if label in by_name.get(t.get('name'), {}).get('labels', [])]
        failed = sum(t.find('failure') is not None or t.find('error') is not None for t in members)
        skipped = sum(t.get('status') != 'run' or t.find('skipped') is not None for t in members)
        expected = sum(label in c['labels'] for c in cases)
        summary[label] = {'expected': expected, 'reported': len(members), 'failures': failed, 'skipped': skipped}
        report = ET.Element('testsuite', name=suite+'.'+label, tests=str(len(members)), failures=str(failed), skipped=str(skipped))
        for member in members:
            report.append(copy.deepcopy(member))
        ET.ElementTree(report).write(folder/(suite+'-'+label+'.xml'), encoding='utf-8', xml_declaration=True)
    return summary


def run_suite(run, cases, suite, source, build, root, manifest, cmake, ctest, configure, parallel, profile='local-full'):
    result = {'status': 'FAIL', 'expected': len(cases), 'reported': 0}
    try:
        run.step(suite+'-configure', [cmake, '-S', source, '-B', build, *configure], timeout=180)
        run.step(suite+'-build', [cmake, '--build', build, '--parallel', str(parallel)], timeout=3600)
        inventory_log = run.step(suite+'-inventory', [ctest, '--test-dir', build, '--show-only=json-v1'])
        inventory = json.loads(inventory_log.read_text('utf-8'))
        check_inventory(cases, inventory, root, build, cmake, manifest, suite, profile)
        junit = run.folder / (suite+'-tests.xml')
        require(not junit.exists(), 'Fresh JUnit path already exists')
        test_error = None
        try:
            run.step(suite+'-test', [ctest, '--test-dir', build, '--output-on-failure', '--parallel', str(parallel),
                                   '--no-tests=error', '--output-junit', junit])
        except GateError as exc:
            test_error = str(exc)
        if junit.exists():
            result['groups'] = group_reports(cases, junit, run.folder, suite)
            result['reported'] = len(ET.parse(junit).getroot().findall('testcase'))
        # Parse even when CTest fails so the failed raw/per-domain evidence is retained.
        check_junit(cases, junit)
        require(test_error is None, test_error)
        result['status'] = 'PASS'
    except (GateError, OSError, ValueError, KeyError, TypeError, ET.ParseError) as exc:
        result['error'] = str(exc)
    return result


def fingerprint(root):
    tracked = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z']).decode('utf-8').split('\0')
    files = {root/p for p in tracked if p}
    files.update(p for p in (root/'tests').rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    return {p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root', type=Path, required=True)
    parser.add_argument('--qt-prefix', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--cmake', type=Path, default=shutil.which('cmake'))
    parser.add_argument('--ctest', type=Path)
    parser.add_argument('--ninja', type=Path, default=shutil.which('ninja'))
    parser.add_argument('--parallel', type=int, default=4)
    parser.add_argument('--profile', choices=('local-full', 'ci-offline'), default='local-full')
    parser.add_argument('--visa-include', type=Path, help='Official SDK header directory')
    parser.add_argument('--wfm-fixtures', type=Path, required=True, help='Pinned official WFM fixtures')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    build_root = args.build_root.resolve()
    folder = build_root/'runs'/(datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:8])
    folder.mkdir(parents=True, exist_ok=False)
    summary = {'status': 'FAIL', 'profile': args.profile, 'folder': str(folder), 'suites': {}}
    summary_path = folder/'summary.json'
    def save():
        summary_path.write_text(json.dumps(summary, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    save()  # An interrupted run has no success marker.
    try:
        require(args.parallel > 0 and args.cmake and args.ninja, 'Positive parallel count, cmake and ninja required')
        cmake = args.cmake.resolve()
        ctest = (args.ctest or cmake.with_name('ctest'+cmake.suffix)).resolve()
        compiler = args.compiler.resolve()
        qt = args.qt_prefix.resolve()
        ninja = args.ninja.resolve()
        env = os.environ.copy()
        env['PATH'] = os.pathsep.join([str(compiler.parent), str(qt/'bin'), str(ninja.parent), env.get('PATH','')])
        env['QT_QPA_PLATFORM'] = 'offscreen'
        env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(qt/'plugins/platforms')
        env['ELECTRONICATE_QT_REPORT_DIR'] = str(folder/'qt')
        env['ELECTRONICATE_TEST_EVIDENCE_DIR'] = str(folder/'test-evidence')
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        run = Run(folder, env)
        marker = build_root/'offline-profile.txt'
        require(not marker.exists() or marker.read_text('utf-8').strip() == args.profile,
                'Build root belongs to a different profile; use a separate directory')
        marker.write_text(args.profile+'\n', encoding='utf-8')
        summary['head'] = subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'], text=True).strip()
        summary['git_status'] = subprocess.check_output(['git','-C',str(root),'status','--porcelain=v1'], text=True).strip()
        before = fingerprint(root)
        (folder/'source-hashes.json').write_text(json.dumps(before, indent=2)+'\n', encoding='utf-8')
        manifest = root/'tests/ctest-manifest.json'
        shutil.copy2(manifest, folder/manifest.name)
        data = load_manifest(manifest, args.profile)
        summary['wfm_fixture_hashes'] = validate_wfm(args.wfm_fixtures)
        visa = (args.visa_include or DEFAULT_VISA_INCLUDE).resolve()
        summary['visa_header_hashes'] = {name:hashlib.sha256((visa/name).read_bytes()).hexdigest()
                                        for name in ('visa.h', 'visatype.h', 'vpptype.h')}
        summary['excluded'] = data['excluded']
        summary['local_supplement'] = {'required': bool(data['excluded']), 'status': 'NOT_RUN' if data['excluded'] else 'INCLUDED',
                                       'cases': data['excluded']}
        summary['expected'] = sum(len(cases) for cases in data['suites'].values())
        summary['environment'] = {'platform': platform.platform(), 'python': sys.version, 'qt_prefix': str(qt),
                                  'qpa': env['QT_QPA_PLATFORM'], 'compiler': str(compiler)}
        for name, command in [('cmake', [cmake,'--version']), ('ctest', [ctest,'--version']),
                              ('compiler', [compiler,'--version']), ('ninja', [ninja,'--version']),
                              ('qt', [qt/'bin/qmake.exe','-query','QT_VERSION'])]:
            log = run.step('version-'+name, command, timeout=30)
            summary['environment'][name] = log.read_text('utf-8', errors='replace').strip()
        configure = ['-G','Ninja','-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_PREFIX_PATH='+qt.as_posix(),
                     '-DCMAKE_CXX_COMPILER='+compiler.as_posix(), '-DCMAKE_MAKE_PROGRAM='+ninja.as_posix(),
                     '-DPython3_EXECUTABLE='+Path(sys.executable).as_posix(),
                     '-DELECTRONICATE_TEST_PROFILE='+args.profile]
        for suite, source in [('main',root/'tests'),('ocr',root/'tests/waveformocr'),('wfm',root/'tests/wfmviewer')]:
            suite_configure = configure + (['-DELECTRONICATE_VISA_INCLUDE='+visa.as_posix()]
                                           if suite=='main' else [])
            if suite == 'wfm':
                suite_configure += ['-DELECTRONICATE_WFM_FIXTURES='+args.wfm_fixtures.resolve().as_posix()]
            summary['suites'][suite] = run_suite(run, data['suites'][suite], suite, source, build_root/suite,
                                               root, manifest, cmake, ctest, suite_configure, args.parallel, args.profile)
            save()
        require(fingerprint(root) == before, 'Sources changed during validation; results do not describe one snapshot')
        require(validate_wfm(args.wfm_fixtures) == summary['wfm_fixture_hashes'], 'WFM fixtures changed during run')
        require({name:hashlib.sha256((visa/name).read_bytes()).hexdigest() for name in summary['visa_header_hashes']}
                == summary['visa_header_hashes'], 'VISA headers changed during run')
        summary['qt_results'] = {}
        for suite, cases in data['suites'].items():
            for case in cases:
                if 'qt_cases' not in case:
                    continue
                reports = list((folder/'qt').glob(case['name']+'-*.xml'))
                require(len(reports) == 1, 'Expected one fresh QtTest report: '+case['name'])
                count = check_qt_xml(reports[0], case['qt_functions'], case['qt_cases'])
                summary['qt_results'][suite+'/'+case['name']] = {
                    'expected':len(case['qt_cases']), 'reported':count, 'report':reports[0].relative_to(folder).as_posix()}
        summary['expected'] = sum(len(c) for c in data['suites'].values())
        summary['reported'] = sum(s['reported'] for s in summary['suites'].values())
        require(all(s['status']=='PASS' for s in summary['suites'].values()), 'At least one required suite failed')
        summary['status'] = 'PASS'
    except (GateError, OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, KeyboardInterrupt) as exc:
        summary['error'] = str(exc) or type(exc).__name__
    finally:
        save()
    print(f"{summary['status']} ({args.profile}): {summary_path}", flush=True)
    return 0 if summary['status']=='PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
