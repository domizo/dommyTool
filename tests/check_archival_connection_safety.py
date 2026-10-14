#!/usr/bin/env python3
"""Execute guarded archival integer conversion and unused POP traversal."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
ROOT=Path(__file__).resolve().parent.parent
ARRAY='src/core/DDataUtility/DataConvert/DPArrayToCompound.cpp'
POP='src/core/DDataUtility/Particle/nobject/DPnPopCommon.cpp'

def bodies(overrides=None):
    overrides=overrides or {}
    guard=(ROOT/'src/core/common/utility/DComputeGuard.h').read_text()
    return '\n'.join((
        guard[guard.index('#define DOMMY_COMPUTE_CATCH'):guard.index('#endif')],
        extract(overrides.get(ARRAY,(ROOT/ARRAY).read_text()),'MStatus DPArrayToCompound::compute('),
        'DOMMY_COMPUTE_CATCH("DPArrayToCompound")',
        extract(overrides.get(POP,(ROOT/POP).read_text()),'MStatus runPopOperation(')))

def run(overrides=None,capture=False,sdk=None):
    compiler=shlex.split(os.environ.get('CXX','clang++'))
    flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-parameter','-Wno-unknown-pragmas']
    if sdk:
        flags[0]='-std=c++14'
        flags+=['-Wno-deprecated-declarations','-isystem',str(Path(sdk)/'include')]
        flags+=['-I'+str(path) for path,_,_ in os.walk(ROOT/'src')]
        for path in (ARRAY,POP):
            subprocess.run(compiler+flags+['-fsyntax-only',str(ROOT/path)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(ROOT/path)],check=True)
        print('Both complete archival source units: Maya SDK syntax/analysis passed; no native execution')
        return
    with tempfile.TemporaryDirectory(prefix='dommy-archival-input-') as directory:
        temp=Path(directory);(temp/'archival_methods.inc').write_text(bodies(overrides))
        flags+=['-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'tests/stubs'),
                '-I'+str(ROOT/'src/core'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/archival_connection_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary)],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

def mutations():
    cases=(
        (ARRAY,'input connection','inputPlug.isConnected(&status);','inputPlug.isConnected();'),
        (ARRAY,'input acquisition','DOMMY_RETURN_IF_FAILURE(status);\n    // Preserve','// Preserve'),
        (ARRAY,'array binding','DOMMY_RETURN_IF_FAILURE(status);\n    const MIntArray','const MIntArray'),
        (ARRAY,'array extraction','DOMMY_RETURN_IF_FAILURE(status);\n    if (values.length()','if (values.length()'),
        (ARRAY,'builder acquisition','DOMMY_RETURN_IF_FAILURE(status);\n    for (unsigned int index','for (unsigned int index'),
        (ARRAY,'element acquisition','DOMMY_RETURN_IF_FAILURE(status);\n        element.set','element.set'),
        (POP,'POP connection','popPlug.isConnected(&status);','popPlug.isConnected();'),
        (POP,'POP graph','popConnections, true, true, &status','popConnections, true, true'),
        (POP,'POP source','popConnections[j].node(&status)','popConnections[j].node()'),
        (POP,'POP binding','fnNode(target, &status)','fnNode(target)'),
        (POP,'POP user node','fnNode.userNode(&status)','fnNode.userNode()'),
        (POP,'POP child failure','DOMMY_RETURN_IF_FAILURE(popNode->runProcess());','(void)popNode->runProcess();'))
    for path,label,old,new in cases:
        source=(ROOT/path).read_text();assert old in source,label
        result=run({path:source.replace(old,new,1)},capture=True)
        assert result.returncode!=0,'Undetected archival mutation: '+label
        print('Detected archival mutation:',label,flush=True)

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    else:
        result=run();assert result.returncode==0
        print('Archival integer compute and POP traversal: exact status, failed queries, inactive paths and retry passed')
        if '--mutations' in sys.argv:mutations()
