#!/usr/bin/env python3
"""Real shared batch policy and typed mesh/curve cost inspection."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
from check_mesh_merge_safety import bodies
from check_curve_path_safety import run

root=Path(__file__).resolve().parent.parent
if '--sdk' in sys.argv:
    sdk=Path(sys.argv[sys.argv.index('--sdk')+1])
    with tempfile.TemporaryDirectory(prefix='dommy-bullet-budget-sdk-') as directory:
        unit=Path(directory)/'sdk.cpp'
        unit.write_text('#include "Data/DCurveData.h"\n#include "DMesh/Data/DMeshData.h"\n'
            '#include "modules/dynamics/DBullet/DM/DBulletInputBudget.h"\n'
            'bool inspect(const DMeshData& m,const DCurveData& c){'
            'dommy::geometry::GeometryBatchBudget b;return dommy::bullet::admitMeshInput(m,b)&&'
            'dommy::bullet::admitCurveInput(c,b,false);}\n')
        flags=shlex.split(os.environ.get('CXX','clang++'))+['-std=c++14','-Wall','-Wextra','-Wpedantic','-Werror',
            '-Wno-deprecated-declarations','-Wno-unused-parameter','-Wno-unknown-pragmas',
            '-isystem',str(sdk/'include')]
        flags+=['-I'+str(Path(path)) for path,_,_ in os.walk(root/'src')]
        flags+=['-isystem',str(root/'third_party/libnoise/source'),'-idirafter',str(root/'tests/stubs')]
        subprocess.run(flags+['-fsyntax-only',str(unit)],check=True)
        subprocess.run(flags+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer','-analyzer-werror',
            '-o',os.devnull,str(unit)],check=True)
    print('Real mesh/curve headers and input-budget template instantiations: Maya SDK syntax/analysis passed (limited Imath double)')
else:
    limits=(root/'src/core/common/utility/DGenerationLimits.h').read_text()
    limits=limits.replace('kMaxGeneratedStringByteCount = 100000000U','kMaxGeneratedStringByteCount = 128U')
    run(root,bodies(root),suite='bullet_input_budget',arguments=[],
        header_overrides={'common/utility/DGenerationLimits.h':limits},
        extra_flags=[str(root/'src/core/DDataUtility/Data/DPGroupData.cpp')])
    print('Shared geometry/work/string budget, real metadata costs, mixed inputs and rejected-append preservation: OK')
    if '--mutations' in sys.argv:
        for path,old,new,label in (
            ('src/core/common/utility/DGeometryBatchBudget.h','if (retain) {','if (retain && false) {','retained storage omitted'),
            ('src/core/common/utility/DGeometryBatchBudget.h',
             '!checkedSum(next.work_, work, kMaxWorkItemCount, next.work_)',
             '(static_cast<void>(checkedSum(next.work_, work, kMaxWorkItemCount, next.work_)), false)','aggregate work omitted'),
            ('src/core/common/utility/DGeometryBatchBudget.h',
             '!mesh::meshOperationStorageBytes(next.plan_, bytes, extra)','false','aggregate geometry bytes omitted'),
            ('src/modules/dynamics/DBullet/DM/DBulletInputBudget.h',
             'mesh::addMeshMetadataCost(source.beginMeshes(), source.endMeshes(), metadata)','true','mesh component cost omitted'),
            ('src/modules/dynamics/DBullet/DM/DBulletInputBudget.h',
             'geometry::addCopyMetadataCost(source, source.numberOfCurves(), metadata)','true','curve collection cost omitted')):
            text=(root/path).read_text();assert old in text
            overrides={'common/utility/DGenerationLimits.h':limits,path.removeprefix('src/'):text.replace(old,new,1)}
            if path.startswith('src/core/'):
                del overrides[path.removeprefix('src/')]
                overrides[path.removeprefix('src/core/')]=text.replace(old,new,1)
            try:
                run(root,bodies(root),suite='bullet_input_budget',arguments=[],capture=True,
                    header_overrides=overrides,extra_flags=[str(root/'src/core/DDataUtility/Data/DPGroupData.cpp')])
            except subprocess.CalledProcessError as error:
                assert error.returncode!=1,'Compile failure is not mutation detection: '+label
                print('Detected input-budget mutation:',label,flush=True)
            else:raise AssertionError('Undetected input-budget mutation: '+label)
