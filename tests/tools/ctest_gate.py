"""Fixed inventory and result contracts for the local offline test gate (stdlib only)."""
import argparse
from collections import Counter
import json
import math
from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


class GateError(ValueError):
    pass


def require(condition, message):
    if not condition:
        raise GateError(message)


def exact_names(expected, actual, context):
    counts = Counter(actual)
    missing = sorted(set(expected) - counts.keys())
    extra = sorted(counts.keys() - set(expected))
    duplicate = sorted(name for name, count in counts.items() if count != 1)
    require(bool(expected) and not (missing or extra or duplicate),
            f'{context}: missing={missing}, extra={extra}, duplicate={duplicate}, expected={len(expected)}')


def load_manifest(path, profile='local-full'):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    require(data.get('version') == 3, 'Unsupported manifest')
    require(set(data['profiles']) == {'local-full', 'ci-offline'}, 'Both profiles must be declared')
    require(profile in data['profiles'], f'Unknown profile: {profile}')
    require(all(p['exclude'] == [] for p in data['profiles'].values()), 'Complete profiles cannot exclude cases')
    require(set(data['suites']) == {'main', 'ocr', 'wfm'}, 'main, ocr and wfm suites are required')
    for suite, cases in data['suites'].items():
        require(bool(cases), f'{suite}: empty expected suite')
        names = [case['name'] for case in cases]
        exact_names(names, names, suite)
        for case in cases:
            require(re.fullmatch(r'[A-Za-z0-9_.-]+', case['name']), 'Unsafe case name')
            labels = case['labels']
            require('env.offline' in labels and any(x.startswith('domain.') for x in labels),
                    f"{case['name']}: missing domain/offline label")
            require(len(set(labels)) == len(labels) and all(re.fullmatch(r'(domain|env)\.[a-z0-9_.-]+', x) for x in labels), 'Invalid labels')
            timeout = case['timeout']
            require(isinstance(timeout, (int, float)) and math.isfinite(timeout) and timeout > 2,
                    f"{case['name']}: timeout must be finite and greater than 2 seconds")
            require(case['command'] and all(isinstance(x, str) for x in case['command']), 'Empty/invalid command')
            require(set(case['properties']) <= {'ENVIRONMENT'}, 'Unreviewed test properties in manifest')
            if 'qt_functions' in case:
                methods = case['qt_functions']
                exact_names(methods, methods, case['name'])
                require({'initTestCase', 'cleanupTestCase'} < set(methods), 'QtTest needs actual methods')
            if 'qt_cases' in case:
                rows = case['qt_cases']
                keys = [(r['function'], r['tag']) for r in rows]
                exact_names(keys, keys, case['name'] + ' data rows')
                require(set(r['function'] for r in rows) == set(case.get('qt_functions', [])),
                        'QtTest rows must cover exactly the expected functions')
    data['profile'] = profile
    data['excluded'] = data['profiles'][profile]['exclude']
    selected_exclusions = {(item['suite'],item['name']) for item in data['excluded']}
    data['suites'] = {suite:[c for c in cases if (suite,c['name']) not in selected_exclusions]
                      for suite,cases in data['suites'].items()}
    require(all(data['suites'].values()), 'Profile cannot leave an empty suite')
    return data


def write_policy(manifest, suite, names_path, output, profile='local-full'):
    cases = load_manifest(manifest, profile)['suites'][suite]
    names = Path(names_path).read_text('utf-8').splitlines()
    exact_names([c['name'] for c in cases], names, f'{profile}/{suite} registration (official VISA SDK required)')
    lines = [f'set_tests_properties({c["name"]} PROPERTIES LABELS "{";".join(c["labels"])}" TIMEOUT {c["timeout"]})' for c in cases]
    Path(output).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def expand(value, root, build, cmake):
    return value.replace('{root}', Path(root).as_posix()).replace('{build}', Path(build).as_posix()).replace('{cmake}', Path(cmake).as_posix())


def check_inventory(cases, inventory, root, build, cmake, manifest, suite, profile='local-full'):
    tests = inventory['tests']
    exact_names([c['name'] for c in cases], [t['name'] for t in tests], suite + ' inventory')
    actual = {t['name']: t for t in tests}
    for case in cases:
        test = actual[case['name']]
        command = [expand(x, root, build, cmake) for x in case['command']]
        if 'qt_functions' in case:
            command = [Path(sys.executable).as_posix(), (Path(root)/'tests/tools/run_qttest.py').as_posix(),
                       '--manifest', Path(manifest).as_posix(), '--profile', profile,
                       '--suite', suite, '--case', case['name'], '--', *command]
        require([x.replace('\\', '/') for x in test.get('command', [])] == command,
                f"{suite}/{case['name']}: command/arguments changed or executable missing")
        properties = {p['name']: p['value'] for p in test.get('properties', [])}
        expected = dict(case['properties'], LABELS=sorted(case['labels']), TIMEOUT=case['timeout'],
                        WORKING_DIRECTORY=expand(case['working_directory'], root, build, cmake))
        require(properties == expected, f"{suite}/{case['name']}: properties changed: {properties!r} != {expected!r}")


def check_junit(cases, path):
    root = ET.parse(path).getroot()
    require(root.tag == 'testsuite', 'Expected a CTest testsuite')
    tests = root.findall('testcase')
    exact_names([c['name'] for c in cases], [t.get('name') for t in tests], 'JUnit results')
    require(int(root.get('tests', '-1')) == len(cases), 'JUnit total mismatch')
    for field in ('failures', 'errors', 'disabled', 'skipped'):
        require(int(root.get(field, '0')) == 0, f'JUnit {field} is nonzero')
    for test in tests:
        require(test.get('status') == 'run' and not any(test.find(tag) is not None for tag in ('failure', 'error', 'skipped')),
                f"{test.get('name')}: did not run successfully")
    return tests


def check_qt_xml(path, methods, expected_rows=None):
    root = ET.parse(path).getroot()
    require(root.tag == 'TestCase', 'Not a QtTest XML report')
    functions = root.findall('TestFunction')
    exact_names(methods, [f.get('name') for f in functions], 'QtTest functions')
    require(not any(m.get('type') in ('skip', 'qfatal') for m in root.iter('Message')), 'QtTest skipped/fatal message')
    rows = []
    for function in functions:
        incidents = function.findall('Incident')
        require(incidents and all(i.get('type') == 'pass' for i in incidents),
                f"QtTest {function.get('name')}: no pass, failure, skip or expected failure")
        rows.extend((function.get('name'), i.findtext('DataTag', default='')) for i in incidents)
    if expected_rows is not None:
        exact_names([(r['function'], r['tag']) for r in expected_rows], rows, 'QtTest data rows')
    return len(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', required=True, type=Path)
    parser.add_argument('--suite', required=True, choices=('main', 'ocr', 'wfm'))
    parser.add_argument('--profile', default='local-full', choices=('local-full', 'ci-offline'))
    parser.add_argument('--names', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        write_policy(args.manifest, args.suite, args.names, args.output, args.profile)
    except (GateError, OSError, ValueError, KeyError, TypeError) as error:
        print(f'Offline inventory FAILED: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
