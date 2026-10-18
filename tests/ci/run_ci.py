"""Run complete software suites using a bootstrapped toolchain and pinned test dependencies."""
import argparse
import json
import os
from pathlib import Path
import sys

sys.dont_write_bytecode=True
from bootstrap_toolchain import clean_environment, run_checked, validate_versions
from prepare_test_dependencies import validate_dependencies


def load_toolchain(path):
    path=path.resolve()
    data=json.loads(path.read_text('utf-8'))
    if data.get('status')!='PASS':
        raise ValueError('Toolchain did not finish successfully')
    validate_versions(data['versions'])
    for name,value in data['tools'].items():
        tool=Path(value).resolve()
        if not tool.exists() or not tool.is_relative_to(path.parent):
            raise ValueError(f'{name} is missing or outside the isolated toolchain')
    if set(data['tools'])!={'python','qt_prefix','compiler','cmake','ctest','ninja'}:
        raise ValueError('Incomplete toolchain')
    return data


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--toolchain',required=True,type=Path)
    parser.add_argument('--build-root',required=True,type=Path)
    args=parser.parse_args()
    data=load_toolchain(args.toolchain)
    build=args.build_root.resolve()
    if build.exists() and any(build.iterdir()):
        raise ValueError('Clean CI validation requires a new/empty build root')
    build.mkdir(parents=True,exist_ok=True)
    evidence=build/'environment';evidence.mkdir()
    env=clean_environment(os.environ,data['paths'])
    temp=build/'temp';temp.mkdir()
    env.update(TEMP=str(temp),TMP=str(temp))
    (evidence/'environment.json').write_text(json.dumps({'path':env['PATH'],'toolchain':str(args.toolchain.resolve()),
        'tools':data['tools'],'versions':data['versions'],
        'host':'github-actions' if env.get('GITHUB_ACTIONS')=='true' else 'local',
        'note':'Fresh packages/build; runner OS and image are recorded separately'},indent=2)+'\n',encoding='utf-8')
    tests=Path(__file__).resolve().parents[1];tools=data['tools'];python=tools['python']
    common=['--cmake',tools['cmake'],'--ctest',tools['ctest'],'--qt-prefix',tools['qt_prefix'],
            '--compiler',tools['compiler'],'--ninja',tools['ninja']]
    run_checked(evidence,'bootstrap-regressions',[python,'-B',tests/'tools/test_toolchain_bootstrap.py'],env)
    run_checked(evidence,'profile-regressions',[python,'-B',tests/'tools/test_ci_profile.py'],env)
    run_checked(evidence,'complete-regressions',[python,'-B',tests/'tools/test_complete_suites.py'],env)
    run_checked(evidence,'dependency-regressions',[python,'-B',tests/'tools/test_test_dependencies.py'],env)
    run_checked(evidence,'gate-regressions',[python,'-B',tests/'tools/test_ctest_gate.py',*common,'--output',build/'gate-selftests'],env)
    run_checked(evidence,'test-dependencies',[python,'-B',tests/'ci/prepare_test_dependencies.py','--root',build/'dependencies'],env)
    dependencies = validate_dependencies(build/'dependencies/dependencies.json')
    (evidence/'test-dependencies.json').write_text(json.dumps(dependencies,indent=2)+'\n',encoding='utf-8')
    run_checked(evidence,'software-tests',[python,'-B',tests/'tools/run_offline_tests.py','--profile','ci-offline',
        '--build-root',build/'tests','--visa-include',dependencies['visa_include'],
        '--wfm-fixtures',dependencies['wfm_dir'],*common,'--parallel','4'],env,timeout=2700)
    validate_dependencies(build/'dependencies/dependencies.json')
    print('CI complete: '+str(build/'tests/runs'))
    return 0


if __name__=='__main__':
    try:
        sys.exit(main())
    except (ValueError,OSError,KeyError,TypeError) as error:
        print('CI FAILED: '+str(error),file=sys.stderr)
        sys.exit(1)
