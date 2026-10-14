#!/usr/bin/env python3
"""Execute complete DAG setup/bounds bodies; writer/archive transport is doubled."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
ROOT=Path(__file__).resolve().parent.parent
JOB='src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.cpp'

def bodies(overrides=None):
    changes=overrides or {}
    source=changes.get(JOB,(ROOT/JOB).read_text())
    header=(ROOT/'src/modules/curve/DCurve/Cmd/Abc/DCAbcWriteJob.h').read_text()
    utility=(ROOT/'src/modules/curve/DCurve/Cmd/DDataExportUtil.cpp').read_text()
    shared=(ROOT/'src/modules/curve/DCurve/Cmd/DDataExportUtil.h').read_text()
    validation=(ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
    command=(ROOT/'src/modules/curve/DCurve/Cmd/DExportAbcCmd.cpp').read_text()
    assert 'createTimeSampling(samplingPlan.boundsTime, secondsPerFrame, boundsTime)' in command
    assert 'geoSamples, geoTime, boundsTime, jobArgs' in command
    begin=source.index('namespace {\nconst std::size_t kNoExportParent')
    budget=(ROOT/'src/core/common/utility/DAttributeArrayConversion.h').read_text()
    code=['namespace dommy {namespace attribute {'+extract(budget,'inline bool validArrayConversionBudget(')+'}}',
          extract(header,'struct AbcWriteJobStatistics')+';',
          extract(header,'class DCAbcWriteJob').replace('private:','public:')+';',
          extract(source[begin:],'namespace {'),
          'namespace ddata_export_util {',extract(utility,'void requireExportQuery('),
          extract(shared,'inline MObject exportDagNode('),extract(shared,'inline MString exportNodeName('),
          extract(utility,'std::string exportDagPathName('),
          extract(utility,'MString exportObjectName('),
          extract(utility,'void requireUniqueExportRootNames('),'}',
          extract(utility,'MString ddata_export_util::stripNamespaces('),
          extract(utility,'bool ddata_export_util::isIntermediate('),
          'namespace dommy {namespace maya {',extract(validation,'inline bool isFinitePoint('),
          extract(validation,'inline bool isFiniteBoundingBox('),'}}']
    code += [extract(command,sig) for sig in ('bool convertFrameToSeconds(',
              'bool convertFramesToSeconds(', 'bool createTimeSampling(')]
    code += [extract(source,sig) for sig in ('void addToString(',
             'void DCAbcWriteJob::trackShapeBounds(',
             'MBoundingBox DCAbcWriteJob::getBoundingBox(', 'void DCAbcWriteJob::setup(',
             'bool DCAbcWriteJob::eval(', 'void DCAbcWriteJob::perFrameCallback(',
             'void DCAbcWriteJob::postCallback(')]
    # Confirm both active archive/callback callsites use the same sample aggregator.
    assert 'getBoundingBox()' in extract(source,'void DCAbcWriteJob::perFrameCallback(')
    assert 'getBoundingBox()' in extract(source,'void DCAbcWriteJob::postCallback(')
    return '\n'.join(code)

def run(overrides=None,capture=False,sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-job-bounds-') as directory:
        temp=Path(directory);(temp/'job_bounds_methods.inc').write_text(bodies(overrides))
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unknown-pragmas',
               '-Wno-unused-parameter','-Wno-unused-variable','-I'+str(temp),
               '-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "alembic_job_bounds_host.h"\n#include "job_bounds_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-werror',
                '-Xanalyzer','-analyzer-output=text','-o',os.devnull,str(unit)],check=True)
            print('Complete DAG setup/sample bounds: SDK syntax/analysis passed; writer/archive/math transport seams declared')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow',
                '-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_job_bounds_tests.cpp'),'-o',str(binary)],
                       check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),str(temp/'existing.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label,old,new in (
            ('root-name preflight caller','ddata_export_util::requireUniqueExportRootNames(\n            mArgs.dagPaths, mArgs.stripNamespace, *mArgs.exportRootAdmission);',''),
            ('DAG depth','depth > kMaxExportDagDepth','false'),
            ('DAG work','depth > kMaxExportDagDepth || !work.consume(1U)','depth > kMaxExportDagDepth'),
            ('DAG child status','requireExportQuery(status, "Unable to read an Alembic DAG child")','requireExportQuery(MS::kSuccess, "Unable to read an Alembic DAG child")'),
            ('DAG node binding','requireExportQuery(status, "Unable to bind an Alembic DAG node")','requireExportQuery(MS::kSuccess, "Unable to bind an Alembic DAG node")'),
            ('shape tracking','trackShapeBounds([dpwrite] { return dpwrite->getSampleBounds(); }, parentIndex);',''),
            ('inherits samples','if (entry.writer->inheritsSampleTransform())','if (true)'),
            ('parent ordering','entry.parent >= world.size()','false'),
            ('all bounds corners','corner < 8U','corner < 2U'),
            ('bounds numeric admission','!dommy::numeric::isFiniteAffineTransform(matrix)','false'),
            ('bounds time index','CreateOArchiveBounds(mRoot, mBoundsTimeIndex)','CreateOArchiveBounds(mRoot, mTransTimeIndex)'),
            ('bounds sample counter','++mBoundsSamples;',''),
            ('bounds metadata count','samp.set(mBoundsSamples);','samp.set(mTransSamples);'),
            ('statistics formatter width','const std::string & name, std::uint64_t value','const std::string & name, unsigned int value'),
            ('point statistics addition','mStats.add(mStats.mDPAnimPts, pointItm->getNumPoints());','mStats.mDPAnimPts += pointItm->getNumPoints();'),
            ('curve statistics addition','mStats.add(mStats.mDCAnimCVs, curveIt->getNumCVs());','mStats.mDCAnimCVs += curveIt->getNumCVs();'),
            ('mesh statistics addition','mStats.add(mStats.mDMAnimVtxs, mIt->getNumPoints());','mStats.mDMAnimVtxs += mIt->getNumPoints();'),
        ):
            source=(ROOT/JOB).read_text();assert old in source,label
            result=run({JOB:source.replace(old,new,1)},capture=True)
            assert result.returncode!=0,'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
    else:
        assert run().returncode==0
        print('Complete setup/eval/callbacks: sample bounds/times/counts, 64-bit statistics, failures, budgets, file preservation: OK')
