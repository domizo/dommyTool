#!/usr/bin/env python3
"""Complete transform writer and IK classifiers, with explicit archive seams."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract

ROOT=Path(__file__).resolve().parent.parent
WRITER='src/modules/curve/DCurve/Cmd/Abc/MayaTransformWriter.cpp'
UTILITY='src/modules/curve/DCurve/Cmd/DDataExportUtil.cpp'
HEADER='src/modules/curve/DCurve/Cmd/DDataExportUtil.h'

def bodies(overrides=None):
    overrides=overrides or {}
    header=overrides.get(HEADER,(ROOT/HEADER).read_text())
    utility=overrides.get(UTILITY,(ROOT/UTILITY).read_text())
    writer=overrides.get(WRITER,(ROOT/WRITER).read_text())
    shared='namespace ddata_export_util {\n'+extract(utility,'void requireExportQuery(')+'\n'
    for signature in ('inline Value readExportValue(', 'inline MPlug requiredExportPlug(',
                      'inline MString exportNodeName(', 'inline double inverseScale('):
        shared+=('template<typename Value>\n' if 'Value' in signature else '')+extract(header,signature)+'\n'
    shared+='}\n'
    shared += extract(utility, 'MString ddata_export_util::stripNamespaces(') + '\n'
    shared += 'namespace ddata_export_util {\n' + extract(utility, 'MString exportObjectName(') + '}\n'
    shared+='\n'.join(extract(utility,signature) for signature in (
        'bool ddata_export_util::getRotOrder(', 'bool ddata_export_util::isDrivenByFBIK(',
        'bool ddata_export_util::isDrivenBySplineIK('))
    return shared+'\n'+writer[writer.index('namespace {'):]

def run(overrides=None,capture=False,sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-transform-') as directory:
        temp=Path(directory);(temp/'transform_methods.inc').write_text(bodies(overrides))
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror',
               '-Wno-unused-parameter','-Wno-unused-variable','-Wno-deprecated-declarations',
               '-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "alembic_transform_host.h"\n#include "transform_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-output=text',
                '-Xanalyzer','-analyzer-werror','-o',os.devnull,str(unit)],check=True)
            print('Complete transform writer/shared reads/IK classifiers: Maya SDK syntax/analysis passed; archive/attributes/sampling seams declared')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_transform_tests.cpp'),'-o',str(binary)],
            check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),str(temp/'fixture.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label,path,old,new in (
            ('joint output naming',WRITER,'ddata_export_util::exportObjectName(joint, iArgs.stripNamespace)','ddata_export_util::exportNodeName(joint)'),
            ('transform output naming',WRITER,'ddata_export_util::exportObjectName(trans, iArgs.stripNamespace)','ddata_export_util::exportNodeName(trans)'),
            ('shared scalar read status', HEADER, 'requireExportQuery(readPlugValue(plug, value),', '(void)readPlugValue(plug, value);requireExportQuery(MS::kSuccess,'),
            ('required plug lookup status', HEADER, 'requireExportQuery(status, "Unable to find a required Alembic transform input");', '(void)status;'),
            ('transform bind status', WRITER, 'ddata_export_util::requireExportQuery(bindStatus, "Unable to bind an Alembic transform");', '(void)bindStatus;'),
            ('FBIK status', UTILITY, 'requireExportQuery(status, "Unable to inspect Alembic FBIK animation");', '(void)status;'),
            ('Spline IK connection status', UTILITY, 'requireExportQuery(status, "Unable to inspect Alembic joint message connections");', '(void)status;'),
            ('joint orientation status', WRITER, 'ddata_export_util::requireExportQuery(iJoint.getOrientation(vals, eJointOrientOrder), "Unable to read Alembic joint orientation");', 'iJoint.getOrientation(vals, eJointOrientOrder);'),
            ('joint axis status', WRITER, 'ddata_export_util::requireExportQuery(iJoint.getScaleOrientation(vals, eRotateAxisOrder), "Unable to read Alembic joint orientation");', 'iJoint.getScaleOrientation(vals, eRotateAxisOrder);'),
            ('frame sample staging', WRITER, 'auto candidateSample = mSample;', 'auto& candidateSample = mSample;'),
            ('Euler history staging', WRITER, 'auto candidateRotate = mPrevRotateSolution;', 'auto& candidateRotate = mPrevRotateSolution;'),
            ('inherits-only animation', WRITER, 'if (sampleCount > 0 || !mInheritsPlug.isNull() || mStructuralControlsAnimated)', 'if (sampleCount > 0)'),
            ('transform numeric admission', WRITER, 'if (!std::isfinite(value))', 'if (false)'),
            ('filtered rotation numeric admission', WRITER, 'finiteTransformValue(Alembic::AbcGeom::RadiansToDegrees(xx))', 'Alembic::AbcGeom::RadiansToDegrees(xx)'),
            ('rotation-order tracking', WRITER, 'trackStructuralControl(iTrans, "rotateOrder", false, iForceStatic);', '(void)iForceStatic;'),
            ('scale-compensation tracking', WRITER, 'trackStructuralControl(iJoint, "segmentScaleCompensate", true, iForceStatic);', '(void)iForceStatic;'),
            ('world ancestor tracking', WRITER, 'trackStructuralControl(dagNode, "inheritsTransform", true, iTimeIndex == 0);', '(void)iTimeIndex;'),
            ('structural frame validation', WRITER, 'requireStructuralControls();', '(void)0;'),
            ('structural comparison', WRITER, 'if (value != control.value)', 'if (false)'),
            ('control animation', WRITER, 'mStructuralControlsAnimated = true;', 'mStructuralControlsAnimated = false;'),
            ('control work budget', WRITER, 'if (!work.consume(1U))', 'if (false)'),
            ('ancestor cardinality', WRITER, 'numPaths > dommy::exporting::kMaxAbcDagDepth', 'false'),
            ('control enum range', WRITER, 'if (!boolean && (value < 0 || value > 5))', 'if (false)'),
            ('retained control storage', WRITER, 'mInspectionBudget->retainInspectionStorage(sizeof(StructuralControl));', '(void)0;'),
        ):
            source=(ROOT/path).read_text()
            assert old in source,'Mutation target missing: '+label
            result=run({path:source.replace(old,new,1)},capture=True)
            assert result.returncode!=0,'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
    else:
        result=run();assert result.returncode==0
        print('Complete transform constructors/frame writes: status/exception faults, static/world/joint paths, retained state, file preservation and retry: OK')
