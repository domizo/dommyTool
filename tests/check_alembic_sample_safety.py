#!/usr/bin/env python3
"""Execute real attribute samplers/creation/write with explicit archive doubles."""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'src/modules/curve/DCurve/Cmd/Abc/MayaAttributeWriter.cpp'


def bodies(source=None):
    source = SOURCE.read_text() if source is None else source
    # Complete source body; Maya and archive behavior remain explicit doubles.
    return source[source.index('namespace Abc ='):]


def run(source=None, capture=False, sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-samples-') as directory:
        temp = Path(directory)
        (temp/'sample_methods.inc').write_text(bodies(source))
        flags = ['-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                 '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
                 '-I'+str(temp), '-I'+str(ROOT/'tests/cpp'), '-I'+str(ROOT/'src/core')]
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        if sdk:
            flags += ['-DDOMMY_SDK_CHECK=1', '-isystem', str(Path(sdk)/'include')]
            unit = temp/'sdk.cpp'
            unit.write_text('#include "alembic_sample_doubles.h"\n#include "sample_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only', str(unit)], check=True)
            subprocess.run(compiler+flags+['--analyze', '-Xanalyzer', '-analyzer-output=text',
                '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(unit)], check=True)
            print('Complete attribute writer source body: Maya SDK syntax/analysis passed; plug readers/Alembic backend declared')
            return
        flags += ['-I'+str(ROOT/'tests/stubs'), '-fsanitize=address,undefined,float-cast-overflow',
                  '-fno-sanitize-recover=all', '-fno-omit-frame-pointer']
        binary = temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_sample_tests.cpp'),
            '-o', str(binary)], check=True, capture_output=capture, text=True)
        return subprocess.run([str(binary), str(temp/'fixture.abc')], capture_output=capture,
            text=True, timeout=30, env=dict(os.environ,
                ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))


if __name__ == '__main__':
    if '--sdk' in sys.argv:
        run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    else:
        result = run()
        assert result.returncode == 0
        print('Alembic sample values, sparse arrays, query faults, budgets, creation and frame failure: OK')
        if '--mutations' in sys.argv:
            source=SOURCE.read_text()
            cases=[('status ignored','if (!status) throw','if (false) throw'),
                   ('element query ignored','elementByPhysicalIndex(index, &status)','elementByPhysicalIndex(index)'),
                   ('storage ignored','count > dommy::generation::kMaxGeneratedPointCount ||','false && count > dommy::generation::kMaxGeneratedPointCount ||')]
            # Remove the entire storage predicate instead of allowing a huge
            # allocation; direct boundary assertions detect the missing policy.
            method=extract(source,'void requireAttributeStorage(')
            start=method.index('if (count >');end=method.index('throw std::length_error',start)
            cases[-1]=('storage ignored',method,method[:start]+'if (false)\n            '+method[end:])
            for label,old,new in cases:
                assert old in source
                result=run(source.replace(old,new,1),capture=True)
                assert result.returncode!=0,'Undetected mutation: '+label
                print('Detected Alembic sample mutation:',label,flush=True)
            matches=list(re.finditer(r'const MFnNumericData::Type numericType = numericDataType\(iPlug\);',source))
            assert len(matches)==2
            for i,match in enumerate(matches):
                replacement='MStatus status; MFnNumericAttribute wrong(exportValue<MObject>(iPlug), &status); requireAttributeQuery(status); const auto numericType = wrong.unitType();'
                result=run(source[:match.start()]+replacement+source[match.end():],capture=True)
                assert result.returncode!=0,'Undetected numeric data/attribute mismatch'
                print('Detected Alembic numeric-function-set mutation:',i,flush=True)
            method=extract(source,'void MayaAttributeWriter::write(')
            guards=list(re.finditer(r'if \(!filledProp\)',method));assert len(guards)==2
            for i,guard in enumerate(guards):
                altered=method[:guard.start()]+'if (false)'+method[guard.end():]
                result=run(source.replace(method,altered),capture=True)
                assert result.returncode!=0,'Undetected selected-property failure'
                print('Detected Alembic selected-property failure:',i,flush=True)
            for label,old,new in (
                ('schema POD','type.getPod() != pod ||','false ||'),
                ('schema extent','type.getExtent() != extent ||','false ||'),
                ('schema byte width','type.getNumBytes() != bytes','false'),
            ):
                assert old in source
                result=run(source.replace(old,new,1),capture=True)
                assert result.returncode!=0,'Undetected attribute schema mutation: '+label
                print('Detected attribute schema mutation:',label,flush=True)
            guard='if (!requireNumericAttributeType(iType, oProp.getDataType())) return false;'
            matches=list(re.finditer(re.escape(guard),source));assert len(matches)==2
            for i,match in enumerate(matches):
                result=run(source[:match.start()]+source[match.end():],capture=True)
                assert result.returncode!=0,'Undetected sampler schema guard: '+str(i)
                print('Detected sampler schema guard:',i,flush=True)
            for label,old,new in (
                ('native archive reservation','budget->retainNativeProperty(name, bytes);','(void)name; (void)bytes;'),
                ('attribute scan work','requireAttributeWork(work, 1U);','(void)work;'),
                ('shared sampling work','getSampledType(plug, work)','getSampledType(plug)'),
                ('filter byte work','requireAttributeWork(work, f->size());','(void)f;'),
            ):
                assert old in source
                result=run(source.replace(old,new,1),capture=True)
                assert result.returncode!=0,'Undetected native attribute budget mutation: '+label
                print('Detected native attribute budget mutation:',label,flush=True)
