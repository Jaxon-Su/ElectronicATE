"""Run one registered QtTest selection and reject missing, failed or skipped methods."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import uuid
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
from ctest_gate import GateError, check_qt_xml, load_manifest, require


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--suite', choices=('main', 'ocr', 'wfm'), required=True)
    parser.add_argument('--profile', choices=('local-full', 'ci-offline'), default='local-full')
    parser.add_argument('--case', required=True)
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        case = next(c for c in load_manifest(args.manifest, args.profile)['suites'][args.suite] if c['name'] == args.case)
        require(args.command and args.command[0] == '--' and len(args.command) > 1, 'Missing QtTest command')
        folder = Path(os.environ.get('ELECTRONICATE_QT_REPORT_DIR', Path.cwd()/'qt-reports'))
        folder.mkdir(parents=True, exist_ok=True)
        report = folder / (args.case + '-' + uuid.uuid4().hex + '.xml')
        command = [*args.command[1:], '-o', str(report) + ',xml']
        print(f'QtTest evidence: {report}', flush=True)
        # Leave time for XML verification before the outer CTest TIMEOUT expires.
        completed = subprocess.run(command, timeout=case['timeout'] - 2)
        require(completed.returncode == 0, f'QtTest exit {completed.returncode}')
        check_qt_xml(report, case['qt_functions'], case.get('qt_cases'))
    except (GateError, OSError, ValueError, KeyError, StopIteration, ET.ParseError, subprocess.TimeoutExpired) as error:
        print(f'QtTest FAILED: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
