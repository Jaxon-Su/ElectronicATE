"""Verify current CI evidence before publishing a successful workflow output."""
import argparse
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from ctest_gate import check_junit, check_qt_xml, load_manifest, require
from run_offline_tests import fingerprint
from prepare_test_dependencies import read_lock


def verify_run(folder, manifest, expected_sha):
    data = load_manifest(manifest, 'ci-offline')
    summary = json.loads((folder/'summary.json').read_text('utf-8'))
    require(summary.get('status') == 'PASS' and summary.get('profile') == 'ci-offline', 'CI profile did not pass')
    require(bool(expected_sha) and summary.get('head') == expected_sha, 'Tested SHA mismatch')
    require((folder/'ctest-manifest.json').read_bytes() == Path(manifest).read_bytes(), 'Manifest changed')
    require(summary.get('excluded') == data['excluded'], 'Exclusion list mismatch')
    require(summary.get('local_supplement') == {'required':False, 'status':'INCLUDED', 'cases':[]},
            'VISA must be included in the complete run')
    lock = read_lock()
    require(summary.get('wfm_fixture_hashes') == {x['path']:x['sha256'] for x in lock['wfm']['files']},
            'WFM fixture evidence mismatch')
    require(summary.get('visa_header_hashes') == {k:v['sha256'] for k,v in lock['visa']['headers'].items()},
            'Official VISA header evidence mismatch')
    require(set(summary['suites']) == set(data['suites']), 'Required suite missing/extra')
    total = 0
    inner_results = {}
    for suite,cases in data['suites'].items():
        result = summary['suites'][suite]
        require(result.get('status') == 'PASS' and result.get('reported') == result.get('expected') == len(cases),
                f'{suite}: incomplete summary')
        total += len(check_junit(cases, folder/(suite+'-tests.xml')))
        for case in cases:
            if 'qt_functions' not in case:
                continue
            reports = list((folder/'qt').glob(case['name']+'-*.xml'))
            require(len(reports) == 1, 'Expected exactly one QtTest report: '+case['name'])
            count = check_qt_xml(reports[0], case['qt_functions'], case.get('qt_cases'))
            if 'qt_cases' in case:
                inner_results[suite+'/'+case['name']] = {
                    'expected':len(case['qt_cases']), 'reported':count,
                    'report':reports[0].relative_to(folder).as_posix()}
    require(summary.get('qt_results') == inner_results, 'QtTest nested summary mismatch')
    require(summary.get('expected') == summary.get('reported') == total, 'Wrong total')
    return summary


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runs',required=True,type=Path)
    parser.add_argument('--expected-sha',required=True)
    parser.add_argument('--output-file',required=True,type=Path)
    parser.add_argument('--summary-file',required=True,type=Path)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    candidates=list(args.runs.glob('*/summary.json'))
    require(len(candidates)==1,'Expected exactly one fresh run')
    folder=candidates[0].parent
    summary=verify_run(folder,root/'tests/ctest-manifest.json',args.expected_sha)
    require(json.loads((folder/'source-hashes.json').read_text('utf-8'))==fingerprint(root),'Sources changed after run')
    # Emit outputs only after the independent checks pass. A missing output is failure.
    with args.output_file.open('a',encoding='utf-8') as stream:
        stream.write('verified=ci-offline\ntested_sha='+args.expected_sha+'\n')
    with args.summary_file.open('a',encoding='utf-8') as stream:
        stream.write(f'## Software tests: {summary["reported"]}/{summary["expected"]} PASS\n\nTested SHA: `{args.expected_sha}`\n\n')
        stream.write('Main includes the VISA fake-I/O test with official headers; OCR and WFM are also required.\n\n')
        for name, result in summary['qt_results'].items():
            stream.write(f'{name}: **{result["reported"]}/{result["expected"]} QtTest results**, '
                         'including init/cleanup, inside its CTest entry. Do not add nested counts to the CTest total.\n\n')
        stream.write('| Suite | Domain | Reported | Expected |\n| --- | --- | --- | --- |\n')
        for suite,result in summary['suites'].items():
            for domain,group in result['groups'].items():
                stream.write(f'| {suite} | {domain} | {group["reported"]} | {group["expected"]} |\n')
        stream.write('\nDomain rows overlap; do not add them to calculate the total.\n')
    return 0


if __name__=='__main__':
    try:
        sys.exit(main())
    except (ValueError,OSError,KeyError,TypeError) as error:
        print('CI evidence FAILED:',error,file=sys.stderr)
        sys.exit(1)
