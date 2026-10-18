"""Install the pinned Windows CI toolchain into a fresh, isolated directory."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
AQT_COMMIT = '076e1659807d0b362a3ed684d54c2e9c775eb9c7'
AQT_SOURCE = 'aqtinstall @ git+https://github.com/miurahr/aqtinstall.git@'+AQT_COMMIT


def clean_environment(source, paths):
    env = {k:v for k,v in source.items() if not k.upper().startswith(('QT_', 'CMAKE_', 'PIP_', 'PKG_CONFIG_'))
           and k.upper() not in {'PYTHONPATH','PYTHONHOME','VIRTUAL_ENV','QTDIR','QT_ROOT_DIR',
                                'CPATH','CPLUS_INCLUDE_PATH','C_INCLUDE_PATH','LIBRARY_PATH','GCC_EXEC_PREFIX','COMPILER_PATH',
                                'CFLAGS','CXXFLAGS','CPPFLAGS','LDFLAGS','FFLAGS','FCFLAGS','RCFLAGS',
                                'INCLUDE','LIB','LIBPATH','CC','CXX','CL','_CL_',
                                'QT_SELECT','QMAKEPATH','QMAKEFEATURES'} }
    env.update(PATH=os.pathsep.join(str(p) for p in paths), PYTHONNOUSERSITE='1',
               PYTHONDONTWRITEBYTECODE='1', PIP_CONFIG_FILE=os.devnull,
               PIP_DISABLE_PIP_VERSION_CHECK='1', PIP_NO_CACHE_DIR='1')
    return env


def run_checked(folder, name, command, env, timeout=1800):
    command=[str(x) for x in command]
    start=time.monotonic();code=-1;error=None
    log=folder/(name+'.log')
    print(name+' ...',flush=True)
    try:
        with log.open('wb') as stream:
            result=subprocess.run(command,cwd=folder,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=timeout)
            code=result.returncode
    except (OSError,subprocess.TimeoutExpired) as exc:
        error=str(exc)
    except KeyboardInterrupt:
        error='KeyboardInterrupt'
        raise
    finally:
        entry={'phase':name,'command':command,'exit_code':code,'error':error,'seconds':round(time.monotonic()-start,3)}
        with (folder/'commands.jsonl').open('a',encoding='utf-8') as stream:
            stream.write(json.dumps(entry)+'\n')
    if code!=0:
        raise ValueError(f'{name} failed (exit {code}); see {log}: {error or ""}')
    return log.read_text('utf-8',errors='replace').strip()


def validate_versions(versions):
    required={'qt':'6.12.0','compiler':'13.1.0','cmake':'cmake version 3.30.5',
              'ctest':'ctest version 3.30.5','ninja':'1.13.2.git.kitware.jobserver-pipe-1'}
    for key,expected in required.items():
        if versions.get(key,'').splitlines()[:1]!=[expected]:
            raise ValueError(f'Unexpected {key}: {versions.get(key)!r}; expected {expected}')


def host_paths():
    # Keep only explicitly selected host infrastructure. No inherited Qt/compiler/Python PATH.
    git=shutil.which('git');pwsh=shutil.which('pwsh')
    if not git or not pwsh:
        raise ValueError('Git and PowerShell 7 are required host tools')
    windows=Path(os.environ['SystemRoot'])
    return [Path(git).parent,Path(pwsh).parent,windows/'System32',windows,windows/'System32/Wbem']


def install(destination):
    destination=destination.resolve()
    if destination.exists() and any(destination.iterdir()):
        raise ValueError('Toolchain destination must be new/empty; prior evidence is never overwritten')
    if sys.platform!='win32' or sys.maxsize<=2**32 or sys.version_info[:3]!=(3,12,10):
        raise ValueError('Use Windows x64 and Python 3.12.10')
    base_paths=host_paths()
    destination.mkdir(parents=True,exist_ok=True)
    evidence=destination/'evidence';evidence.mkdir()
    status={'status':'FAIL','aqt_commit':AQT_COMMIT,'root':str(destination),'host_python':sys.executable}
    status_path=evidence/'bootstrap.json'
    status_path.write_text(json.dumps(status,indent=2)+'\n',encoding='utf-8')
    try:
        env=clean_environment(os.environ,base_paths)
        temp=destination/'temp';temp.mkdir()
        env.update(TEMP=str(temp),TMP=str(temp))
        venv=destination/'python'
        run_checked(evidence,'create-python',[sys.executable,'-m','venv',venv],env)
        python=venv/'Scripts/python.exe';scripts=python.parent
        qt_base=destination/'qt';qt=qt_base/'6.12.0/mingw_64'
        compiler=qt_base/'Tools/mingw1310_64/bin/g++.exe'
        env['PATH']=os.pathsep.join(str(p) for p in [scripts,compiler.parent,qt/'bin',*base_paths])
        requirements=HERE/'requirements-win64.txt'
        shutil.copy2(requirements,evidence/requirements.name)
        run_checked(evidence,'install-dependencies',[python,'-m','pip','install','--no-cache-dir','--report',evidence/'dependencies-report.json','-r',requirements],env)
        run_checked(evidence,'install-aqt',[python,'-m','pip','install','--no-cache-dir','--no-build-isolation','--no-deps',
                                         '--report',evidence/'aqt-report.json',AQT_SOURCE],env)
        run_checked(evidence,'pip-check',[python,'-m','pip','check'],env)
        run_checked(evidence,'pip-freeze',[python,'-m','pip','freeze','--all'],env)
        archives=destination/'archives';archives.mkdir()
        aqt=[python,HERE/'run_aqt.py']
        run_checked(evidence,'install-mingw',[*aqt,'install-tool','windows','desktop','tools_mingw1310','qt.tools.win64_mingw1310',
                                            '-O',qt_base,'--keep','--archive-dest',archives],env)
        run_checked(evidence,'install-qt',[*aqt,'install-qt','windows','desktop','6.12.0','win64_mingw',
                                         '-O',qt_base,'--keep','--archive-dest',archives,'-m','qtserialport'],env)
        # CMake records its native executable in generated CTest commands, not pip's launcher.
        cmake_bin=venv/'Lib/site-packages/cmake/data/bin'
        tools={'python':python,'qt_prefix':qt,'compiler':compiler,'cmake':cmake_bin/'cmake.exe',
               'ctest':cmake_bin/'ctest.exe','ninja':scripts/'ninja.exe'}
        for name,path in tools.items():
            if not path.exists() or not path.resolve().is_relative_to(destination):
                raise ValueError(f'{name} is missing or outside clean root: {path}')
        versions={}
        for name,command in [('qt',[qt/'bin/qmake.exe','-query','QT_VERSION']),('compiler',[compiler,'-dumpfullversion']),
                             ('cmake',[tools['cmake'],'--version']),('ctest',[tools['ctest'],'--version']),('ninja',[tools['ninja'],'--version'])]:
            versions[name]=run_checked(evidence,'version-'+name,command,env,timeout=30)
        validate_versions(versions)
        archive_hashes={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in archives.iterdir() if p.is_file()}
        if not archive_hashes:
            raise ValueError('No downloaded archive evidence')
        result={'status':'PASS','tools':{k:str(v) for k,v in tools.items()},'paths':[str(p) for p in [scripts,compiler.parent,qt/'bin',*base_paths]],
                'versions':versions,'python_version':sys.version.split()[0],'aqt_commit':AQT_COMMIT,'archives_sha256':archive_hashes}
        marker=destination/'toolchain.json.tmp'
        marker.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
        marker.replace(destination/'toolchain.json')
        status.update(result)
        return result
    except (ValueError,OSError,subprocess.SubprocessError,KeyboardInterrupt) as exc:
        status['error']=str(exc) or type(exc).__name__
        raise
    finally:
        status_path.write_text(json.dumps(status,indent=2)+'\n',encoding='utf-8')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',required=True,type=Path)
    parser.add_argument('--github-env',type=Path)
    args=parser.parse_args()
    result=install(args.root)
    if args.github_env:
        with args.github_env.open('a',encoding='utf-8') as stream:
            stream.write('OFFLINE_TOOLCHAIN='+str(args.root.resolve()/'toolchain.json')+'\n')
    print('Toolchain PASS: '+str(args.root.resolve()/'toolchain.json'))
    return 0


if __name__=='__main__':
    try:
        sys.exit(main())
    except (ValueError,OSError,subprocess.SubprocessError,KeyboardInterrupt) as error:
        print('Toolchain FAILED: '+str(error),file=sys.stderr)
        sys.exit(1)
