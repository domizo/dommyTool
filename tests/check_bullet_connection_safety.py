#!/usr/bin/env python3
"""Production rigid dispatch and DAG refresh, with declared host/world boundaries."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract

ROOT=Path(__file__).resolve().parent.parent
RIGID='src/modules/dynamics/DBullet/RigidBody/DBRigidBodyNode.cpp'
SOLVER='src/modules/dynamics/DBullet/Solver/DBSolverNode.cpp'

def bodies(overrides=None):
    overrides=overrides or {}
    guard=(ROOT/'src/core/common/utility/DComputeGuard.h').read_text()
    return '\n'.join((
        guard[guard.index('#define DOMMY_COMPUTE_CATCH'):guard.index('#endif')],
        extract(overrides.get(RIGID,(ROOT/RIGID).read_text()),'MStatus DBRigidBodyNode::compute('),
        'DOMMY_COMPUTE_CATCH("DBRigidBodyNode")',
        extract(overrides.get(SOLVER,(ROOT/SOLVER).read_text()),'MStatus DBSolverNode::updateAllRigidBodies(')))

def run(overrides=None,capture=False,sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-connection-') as directory:
        temp=Path(directory);(temp/'bullet_connection_methods.inc').write_text(bodies(overrides))
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-parameter',
               '-I'+str(temp),'-I'+str(ROOT/'tests/cpp')]
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "bullet_connection_doubles.h"\n#include "bullet_connection_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Bullet dispatch/DAG refresh: real Maya SDK syntax/analysis passed; body/world boundaries declared')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/bullet_connection_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary)],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

def mutations():
    variants=[]
    for path in (RIGID,SOLVER):
        source=(ROOT/path).read_text()
        signature='MStatus DBRigidBodyNode::compute(' if path==RIGID else 'MStatus DBSolverNode::updateAllRigidBodies('
        method=extract(source,signature)
        checks=list(re.finditer(r'isConnected\(&(\w+)\);\n[ \t]*if \(!\1\) return \1;',method))
        assert len(checks)==(2 if path==RIGID else 11)
        for index,check in enumerate(checks):
            candidate=method[:check.start()]+check.group().split('\n')[0]+method[check.end():]
            variants.append((Path(path).stem+' connection '+str(index),{path:source.replace(method,candidate,1)}))
    source=(ROOT/RIGID).read_text()
    for query in ('isElement','array','logicalIndex'):
        pattern=re.compile(query+r'\(&status\);\n[ \t]*if \(!status\) return status;')
        match=pattern.search(source);assert match
        candidate=source[:match.start()]+match.group().split('\n')[0]+source[match.end():]
        variants.append(('rigid dispatch '+query,{RIGID:candidate}))
    for label,overrides in variants:
        result=run(overrides,capture=True)
        assert result.returncode!=0,'Undetected mutation: '+label
        print('Detected Bullet connection mutation:',label,flush=True)
    print('Bullet runtime mutations detected:',len(variants))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    else:
        result=run();assert result.returncode==0
        print('Production rigid dispatch and complete DAG refresh: query failures, dispatch, skips and retry passed')
        if '--mutations' in sys.argv:mutations()
