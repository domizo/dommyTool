#!/usr/bin/env python3
"""Execute all five procedural attribute serializers with real typed helpers."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
ROOT=Path(__file__).resolve().parent.parent
HEADER='src/modules/curve/DCurve/Cmd/DDataExportUtil.h'
STATE='src/core/common/utility/DExportAttributeState.h'
BASE='src/modules/curve/DCurve/Cmd/Abc/'

def bodies(overrides=None):
    changes=overrides or {};source=changes.get(HEADER,(ROOT/HEADER).read_text())
    result=['namespace ddata_export_util {']
    for sig in ('inline bool isShapeExportType(', 'void addPrimitiveExportFields(', 'void addPointExportFields(',
                'inline dommy::attribute::ExportAttributeFrame planPointExportAttributes(',
                'inline dommy::attribute::ExportAttributeFrame planCurveExportAttributes(',
                'inline dommy::attribute::ExportAttributeFrame planMeshExportAttributes('):
        result.append(('template<typename Owner>\n' if sig.startswith('void add') else '')+extract(source,sig))
    result.append('}')
    for sig in ('inline void DGetAbcDPScalarAttribute(', 'inline void DGetAbcDCPScalarAttribute(',
                'inline void DGetAbcDMPScalarAttribute(', 'inline void DGetAbcDMPFaceAttribute('):
        result.append('template<typename T,typename U>\n'+extract(source,sig))
    result += [extract(source,'inline void DSetAbcAttributeValues('),
               'template<typename GeomParam>\n'+extract(source,'inline void publishShapeExportAttribute(')]
    for cls,names in (('DPointWriter',['getDPointPointAttribute']),
                      ('DCurveWriter',['getDCurvePrimAttribute','getDCurvePointAttribute']),
                      ('DMeshWriter',['getDMeshPrimAttribute','getDMeshPointAttribute'])):
        path=BASE+cls+'.cpp';writer=changes.get(path,(ROOT/path).read_text())
        result.extend(extract(writer,'void '+cls+'::'+name+'(') for name in names)
    return '\n'.join(result)

def run(overrides=None,capture=False,sdk=None):
    changes=overrides or {}
    with tempfile.TemporaryDirectory(prefix='dommy-attribute-state-') as directory:
        temp=Path(directory);(temp/'attribute_state_methods.inc').write_text(bodies(changes))
        if STATE in changes:
            target=temp/'common/utility/DExportAttributeState.h';target.parent.mkdir(parents=True)
            target.write_text(changes[STATE])
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unknown-pragmas',
               '-Wno-unused-parameter','-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),
               '-I'+str(ROOT/'src/core'),'-I'+str(ROOT/'src/core/common/utility')]
        compiler=shlex.split(os.environ.get('CXX','clang++'));binary=temp/'test'
        if sdk:
            supplemental=temp/'supplemental';supplemental.mkdir()
            for name in ('DType.h','ImathVec.h'):
                (supplemental/name).write_text((ROOT/'tests/stubs'/name).read_text())
            flags+=['-I'+str(supplemental),'-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "alembic_attribute_transport.h"\n#include "attribute_state_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-werror',
                '-Xanalyzer','-analyzer-output=text','-o',os.devnull,str(unit)],check=True)
            print('Five typed serializers/state planning: SDK syntax/analysis passed; Imath, procedural owners and Alembic are explicit transport doubles')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_attribute_state_tests.cpp'),'-o',str(binary)],
                       capture_output=capture,text=True,check=True)
        return subprocess.run([str(binary),str(temp/'existing.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label,path,old,new in (
            ('domain collision',STATE,'if (existing != fields_.end())','if (false)'),
            ('static element layout',STATE,'if (layouts_ != frame.layouts_)','if (false)'),
            ('attribute set identity',STATE,'if (fields_.size() != frame.fields_.size())','if (false)'),
            ('attribute schema identity',STATE,'current->second.type != field.second.type','false'),
            ('registry completion',STATE,'if (!field.second.written)','if (false)'),
            ('archive metadata storage',STATE,'!generation::checkedProduct(total.storageBytes, 4U,','!generation::checkedProduct(total.storageBytes, 1U,'),
            ('property sample ordinal',HEADER,'existing->second.getNumSamples() != (sampled ? state.samples() : 1U)','false'),
            ('initial property collision',HEADER,'existing != properties.end() || parent.getPropertyHeader(name)','false'),
            ('sample Scope agreement',HEADER,'sample.getScope() != scope','false'),
            ('missing property failure',HEADER,'throw std::logic_error("Missing selected Alembic attribute property: " + attrName);','return;'),
        ):
            source=(ROOT/path).read_text();assert old in source,label
            result=run({path:source.replace(old,new,1)},capture=True)
            assert result.returncode!=0,'Undetected mutation: '+label
            print('Detected attribute state mutation: '+label,flush=True)
    else:
        assert run().returncode==0
        print('All 60 typed serializer branches: schema domains, static layouts, frame identity/counts, archive budget and faults: OK')
