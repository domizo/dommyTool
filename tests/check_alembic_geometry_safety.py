#!/usr/bin/env python3
"""Execute shape/locator construction and frame bodies with explicit seams."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
ROOT=Path(__file__).resolve().parent.parent
BASE='src/modules/curve/DCurve/Cmd/Abc/'
HEADER='src/modules/curve/DCurve/Cmd/DDataExportUtil.h'

def bodies(overrides=None):
    overrides=overrides or {}
    header=overrides.get(HEADER,(ROOT/HEADER).read_text())
    utility=(ROOT/'src/modules/curve/DCurve/Cmd/DDataExportUtil.cpp').read_text()
    budget=(ROOT/'src/core/common/utility/DAttributeArrayConversion.h').read_text()
    names=(ROOT/'src/core/common/utility/DMayaNameList.h').read_text()
    code=['namespace dommy {namespace attribute {',extract(budget,'inline bool validArrayConversionBudget('),
          '}namespace maya {',extract(names,'struct NameListBudget')+';','}}',
          'namespace ddata_export_util {',extract(utility,'void requireExportQuery('),
          'template<typename Value>\n'+extract(header,'inline Value readExportValue(')]
    validation=(ROOT/'src/core/common/utility/DMayaValueValidation.h').read_text()
    code.insert(0,'namespace dommy {namespace maya {'+extract(validation,'inline bool isFinitePoint(')+
                extract(validation,'inline bool isFiniteBoundingBox(')+'}}')
    for signature in ('inline MPlug requiredExportPlug(', 'inline MString exportNodeName(',
                      'inline MObject exportDagNode(', 'inline float checkedExportFloat(',
                      'inline std::list<std::string> readExportNameList('):
        code.append(extract(header,signature))
    code.append('template<typename Points>\n'+extract(header,'MBoundingBox exportSampleBounds('))
    for signature in ('inline bool isShapeExportType(', 'void addPrimitiveExportFields(',
                      'void addPointExportFields(', 'inline dommy::attribute::ExportAttributeFrame planPointExportAttributes(',
                      'inline dommy::attribute::ExportAttributeFrame planCurveExportAttributes(',
                      'inline dommy::attribute::ExportAttributeFrame planMeshExportAttributes('):
        code.append(('template<typename Owner>\n' if signature.startswith('void add') else '')+extract(header,signature))
    code.append('}')
    code.append(extract(utility, 'MString ddata_export_util::stripNamespaces('))
    code.append('namespace ddata_export_util {' + extract(utility, 'MString exportObjectName(') + '}')
    for cls in ('DPointWriter','DCurveWriter','DMeshWriter','MayaLocatorWriter'):
        path=BASE+cls+'.cpp';source=overrides.get(path,(ROOT/path).read_text())
        helper={'DPointWriter':'bool hasExportablePointTopology(', 'DCurveWriter':'bool hasExportableCurveTopology(',
                'DMeshWriter':'bool hasExportableMeshTopology(', 'MayaLocatorWriter':'std::array<double, 6> readLocatorSample('}[cls]
        code+=['namespace {',extract(source,helper),'}',extract(source,cls+'::'+cls+'('),
               extract(source,'void '+cls+'::write('),extract(source,'bool '+cls+'::isAnimated(')]
        if cls=='MayaLocatorWriter':code.append(extract(source,cls+'::~'+cls+'('))
        else:
            for getter in {'DPointWriter':['getNumPoints'],'DCurveWriter':['getNumCVs','getNumCurves'],
                           'DMeshWriter':['getNumPoints','getNumMeshs']}[cls]:
                code.append(extract(source,'size_t '+cls+'::'+getter+'('))
        if cls in ('DCurveWriter','DMeshWriter'):
            method='getDCurvePrimAttribute' if cls=='DCurveWriter' else 'getDMeshPrimAttribute'
            prefix=extract(source,'void '+cls+'::'+method+'(').split('DP_ATTR_DATA_TYPE attrType',1)[0]
            owner='dCurves' if cls=='DCurveWriter' else 'dmesh'
            code.append(prefix+'}}payloadAttributeStage();fakeCollectionAttributeStage('+owner+
                        ',attributeList,mAttributeState,pointAttributeList,false,isAnim);}')
    return '\n'.join(code)

def run(overrides=None,capture=False,sdk=None):
    with tempfile.TemporaryDirectory(prefix='dommy-alembic-geometry-') as directory:
        temp=Path(directory);(temp/'geometry_methods.inc').write_text(bodies(overrides))
        source=(overrides or {}).get(HEADER,(ROOT/HEADER).read_text())
        (temp/'builtin_sample_state.inc').write_text('namespace ddata_export_util {\n'+
            extract(source,'class BuiltinSampleState')+';\n}')
        flags=['-std=c++11','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-parameter',
               '-Wno-unused-variable','-Wno-unknown-pragmas','-Wno-sign-compare',
               '-I'+str(temp),'-I'+str(ROOT/'tests/cpp'),'-I'+str(ROOT/'src/core')]
        compiler=shlex.split(os.environ.get('CXX','clang++'))
        if sdk:
            flags+=['-DDOMMY_SDK_CHECK=1','-isystem',str(Path(sdk)/'include')]
            unit=temp/'sdk.cpp';unit.write_text('#include "alembic_geometry_host.h"\n#include "geometry_methods.inc"\n')
            subprocess.run(compiler+flags+['-fsyntax-only',str(unit)],check=True)
            subprocess.run(compiler+flags+['--analyze','-Xanalyzer','-analyzer-werror','-Xanalyzer','-analyzer-output=text','-o',os.devnull,str(unit)],check=True)
            print('All shape/locator constructor/frame bodies: Maya SDK syntax/analysis passed; data/attribute/archive seams declared')
            return
        flags+=['-I'+str(ROOT/'tests/stubs'),'-fsanitize=address,undefined,float-cast-overflow','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
        binary=temp/'test'
        subprocess.run(compiler+flags+[str(ROOT/'tests/cpp/alembic_geometry_tests.cpp'),'-o',str(binary)],check=True,capture_output=capture,text=True)
        return subprocess.run([str(binary),str(temp/'fixture.abc')],capture_output=capture,text=True,timeout=30,
            env=dict(os.environ,ASAN_OPTIONS='detect_leaks='+('0' if sys.platform=='darwin' else '1')))

if __name__=='__main__':
    if '--sdk' in sys.argv:run(sdk=sys.argv[sys.argv.index('--sdk')+1])
    elif '--mutations' in sys.argv:
        for label,path,old,new in (
            ('point output naming',BASE+'DPointWriter.cpp','ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace)','ddata_export_util::exportNodeName(fnDepNode)'),
            ('curve output naming',BASE+'DCurveWriter.cpp','ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace)','ddata_export_util::exportNodeName(fnDepNode)'),
            ('mesh output naming',BASE+'DMeshWriter.cpp','ddata_export_util::exportObjectName(fnDepNode, iArgs.stripNamespace)','ddata_export_util::exportNodeName(fnDepNode)'),
            ('locator output naming',BASE+'MayaLocatorWriter.cpp','ddata_export_util::exportObjectName(fnLocator, iArgs.stripNamespace)','ddata_export_util::exportNodeName(fnLocator)'),
            ('unique mesh names',BASE+'DMeshWriter.cpp','std::to_string(i)','std::to_string(0U)'),
            ('mesh partition count',BASE+'DMeshWriter.cpp','dmesh->numberOfMesh() != mMeshCount','false'),
            ('mesh schema cardinality',BASE+'DMeshWriter.cpp','mTransSchemaList.size() != mMeshCount','false'),
            ('point constructor admission',BASE+'DPointWriter.cpp','if (!hasExportablePointTopology(*dpoint))','if (false)'),
            ('curve constructor admission',BASE+'DCurveWriter.cpp','if (!hasExportableCurveTopology(*dCurves))','if (false)'),
            ('mesh constructor admission',BASE+'DMeshWriter.cpp','if (!hasExportableMeshTopology(*dmesh))','if (false)'),
            ('point empty frame',BASE+'DPointWriter.cpp','if (dpoint->numberOfPoints() == 0U)','if (false)'),
            ('curve empty frame',BASE+'DCurveWriter.cpp','if (dCurves->numberOfCurves() == 0U)','if (false)'),
            ('point width mode',BASE+'DPointWriter.cpp','widthType < 0 || widthType > 1','false'),
            ('borrowed velocity lifetime',BASE+'DPointWriter.cpp','if (velocitiesPresent) {',
             'if (velocitiesPresent) { std::vector<Alembic::AbcGeom::V3f> velocity;'),
            ('point built-in identity',BASE+'DPointWriter.cpp','mBuiltinState.require(widthsPresent, widthScope, velocitiesPresent);',''),
            ('curve built-in identity',BASE+'DCurveWriter.cpp','mBuiltinState.require(widthsPresent, widthScope, false);',''),
            ('width Scope identity',HEADER,'(widths && scope != widthScope_)','false'),
            ('width presence identity',HEADER,'widths != widths_','false'),
            ('velocity presence identity',HEADER,'velocities != velocities_','false'),
            ('point built-in publication',BASE+'DPointWriter.cpp','mBuiltinState.commit(widthsPresent, widthScope, velocitiesPresent);',''),
            ('curve built-in publication',BASE+'DCurveWriter.cpp','mBuiltinState.commit(widthsPresent, widthScope, false);',''),
            ('point absent width omission',BASE+'DPointWriter.cpp','if (widthsPresent)','if (true)'),
            ('curve absent width omission',BASE+'DCurveWriter.cpp','if (widthsPresent)','if (true)'),
            ('mixed periodicity admission',BASE+'DCurveWriter.cpp','else if (curve->isCurveOpen() != isOpen) return false;','else if (curve->isCurveOpen() != isOpen) (void)isOpen;'),
            ('point negative width',BASE+'DPointWriter.cpp','!std::isfinite(value) || value < 0.0f','!std::isfinite(value)'),
            ('curve negative width',BASE+'DCurveWriter.cpp','!std::isfinite(value) || value < 0.0f','!std::isfinite(value)'),
            ('mesh face copy',BASE+'DMeshWriter.cpp','faceArray.length() != static_cast<std::size_t>(mIt->numFace())','false'),
            ('curve knot copy',BASE+'DCurveWriter.cpp','knotsArray.length() != cIt->numberOfKnots()','false'),
            ('locator bind status',BASE+'MayaLocatorWriter.cpp','ddata_export_util::requireExportQuery(status, "Unable to bind an Alembic locator");','(void)status;'),
            ('locator finite admission',BASE+'MayaLocatorWriter.cpp','if (!std::isfinite(values[index]))','if (false)'),
            ('shared primitive admission',HEADER,'if (!attribute || attribute->attrSize() != count)','if (false)'),
            ('point attribute frame planning',BASE+'DPointWriter.cpp','mAttributeState.prepare(ddata_export_util::planPointExportAttributes(*dpoint, ptattrListArray, mAttributeState));',''),
            ('curve attribute frame planning',BASE+'DCurveWriter.cpp','mAttributeState.prepare(ddata_export_util::planCurveExportAttributes(\n            *dCurves, attrListArray, ptattrListArray, mAttributeState));',''),
            ('mesh attribute frame planning',BASE+'DMeshWriter.cpp','mAttributeState.prepare(ddata_export_util::planMeshExportAttributes(\n            *dmesh, attrListArray, ptattrListArray, mAttributeState));',''),
            ('attribute frame commit',BASE+'DPointWriter.cpp','mAttributeState.commit();',''),
            ('DAG query status',HEADER,'requireExportQuery(status, "Unable to read an Alembic DAG node");','(void)status;'),
            ('shared scalar status',HEADER,'requireExportQuery(readPlugValue(plug, value),','(void)readPlugValue(plug, value);requireExportQuery(MS::kSuccess,'),
            ('float conversion admission',HEADER,'if (!dommy::numeric::checkedFloatCast(value, result))','(void)dommy::numeric::checkedFloatCast(value, result);if (false)'),
            ('name-list length',HEADER,'if (value.length() > dommy::generation::kMaxGeneratedStringByteCount)','if (false)'),
            ('name-list count',HEADER,'!dommy::generation::checkedSum(candidate.names, count,\n                dommy::generation::kMaxGeneratedStringCount, candidate.names)','false'),
        ):
            source=(ROOT/path).read_text();assert old in source,label
            # Empty-frame guards occur in both constructors and writes.
            modified=source.replace(old,new) if 'empty frame' in label else source.replace(old,new,1)
            result=run({path:modified},capture=True)
            assert result.returncode!=0,'Undetected mutation: '+label
            print('Detected mutation: '+label,flush=True)
    else:
        assert run().returncode==0
        print('Alembic shape/locator bodies: borrowed arrays, fixed built-in schemas, curve wrap, mesh partitions, faults and pending destinations: OK')
