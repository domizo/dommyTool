#!/usr/bin/env python3
"""Execute the destination scene scan with explicit Maya/filesystem seams."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
SOURCE = 'src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp'

def run(source=None,capture=False,sdk=None):
    source=source or (ROOT/SOURCE).read_text()
    body=extract(source,'MStatus checkAlembicDestinationUnused(')
    caller=next(line.strip() for line in source.splitlines() if 'checkAlembicDestinationUnused(absoluteFile)' in line)
    start=source.index('MFileObject directory;')
    end=source.index('// check the file is used by any AlembicNode',start)
    path_body='MStatus pathAdmission(std::string& fileName,const MString& expandName){MStatus status;std::set<std::string> outputPaths;'+source[start:end]+'++admitted;return MS::kSuccess;}'
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-destination-') as directory:
        temp=Path(directory)
        (temp/'destination_methods.inc').write_text(body+'\nMStatus destinationAdmission(const MFileObject& absoluteFile){'+caller+'++admitted;return MS::kSuccess;}\n'+path_body)
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-I'+str(temp),
               '-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "alembic_destination_host.h"\n#include "destination_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-werror','-Xanalyzer','-analyzer-output=text','-o',os.devnull,str(unit)],check=True)
            print('Complete destination scan/caller: Maya SDK syntax/analysis passed; plug-reader runtime not linked')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined','-fno-sanitize-recover=all']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_destination_tests.cpp'),'-o',str(binary)],check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),str(temp/'fixture.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        source=(ROOT/SOURCE).read_text()
        for label,old,new in (
            ('directory path status','DOMMY_RETURN_IF_FAILURE(directory.setRawFullName(expandName));','(void)directory.setRawFullName(expandName);'),
            ('output path status','DOMMY_RETURN_IF_FAILURE(absoluteFile.setRawFullName(fileName.c_str()));','(void)absoluteFile.setRawFullName(fileName.c_str());'),
            ('relative path status','DOMMY_RETURN_IF_FAILURE(absoluteFile.setRawFullName(absoluteFileName));','(void)absoluteFile.setRawFullName(absoluteFileName);'),
            ('parent path status','DOMMY_RETURN_IF_FAILURE(absoluteFilePath.setRawFullName(absoluteFile.resolvedPath()));','(void)absoluteFilePath.setRawFullName(absoluteFile.resolvedPath());'),
            ('iterator status','DOMMY_RETURN_IF_FAILURE(status);','(void)status;'),
            ('plug lookup status','DOMMY_RETURN_IF_FAILURE(status);\n            if (plug.isNull())','(void)status;\n            if (plug.isNull())'),
            ('file path read','DOMMY_RETURN_IF_FAILURE(readPlugValue(plug, path));','readPlugValue(plug, path);'),
            ('path setup status','DOMMY_RETURN_IF_FAILURE(file.setRawFullName(path));','file.setRawFullName(path);'),
            ('advance status','DOMMY_RETURN_IF_FAILURE(nodes.next());','nodes.next();'),
            ('used-file comparison','file.resolvedFullName() == destination.resolvedFullName()','file.resolvedFullName() == destination.resolvedFullName() && false'),
            ('caller failure','DOMMY_RETURN_IF_FAILURE(checkAlembicDestinationUnused(absoluteFile));','checkAlembicDestinationUnused(absoluteFile);'),
        ):
            assert old in source,label
            result=run(source.replace(old,new,1),capture=True)
            assert result.returncode!=0,'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
    else:
        assert run().returncode==0
        print('Alembic destination scan: failed queries prevent commit, in-use/absent files, retry and retained destination: OK')
