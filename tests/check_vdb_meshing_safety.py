#!/usr/bin/env python3
"""VDB-to-mesh production control flow with explicit Maya/native boundaries."""
from pathlib import Path
import os,re,shlex,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_vdb_distribution_safety import compute_bodies as distribution_bodies
NODES=('DOpenVDBToMesh','DOpenVDBConvert')
FOLDER='src/modules/volume/VDB/Convert/'

def initializers(root):
    result=[]
    for name in NODES:
        source=(root/(FOLDER+name+'.cpp')).read_text()
        result+=declarations(name,source,True)+[extract(source,'MStatus '+name+'::initialize(')]
    return '\n'.join(result)

def bodies(root):
    folder='src/modules/volume/VDB/'
    result=[distribution_bodies(root).split('struct DOpenVDBScatter {')[0],
            extract((root/(folder+'Edit/DOpenVDBMeshConversionPolicy.h')).read_text(),'namespace dommy {'),
            extract((root/(folder+'Edit/DOpenVDBMeshingPolicy.h')).read_text(),'namespace dommy {'),
            extract((root/(folder+'Common/DOpenVDBGeoUtil.h')).read_text(),'namespace {'),
            extract((root/'src/core/common/utility/DMayaMeshTraversal.h').read_text(),'namespace dommy {'),
            extract((root/'src/core/common/vdb/DVdbOp.cpp').read_text(),'MStatus preflightVdbMeshInput('),
            extract((root/(folder+'Common/DOpenVDBGeoUtil.cpp')).read_text(),'MStatus DOpenVDBMakePointList(const MObject&'),
            extract((root/(folder+'Common/DOpenVDBUtil.cpp')).read_text(),'void getGrids(')]
    for name in NODES:
        source=(root/(FOLDER+name+'.cpp')).read_text()
        declared=declarations(name,source,False)
        declared[0]=declared[0].replace('MStatus compute(', 'MObject thisMObject()const{return {"node"};} MString name()const{return "node";} MStatus compute(')
        if name=='DOpenVDBConvert':
            declared[0]=declared[0][:-2]+'''template<typename G>MStatus referenceIntake(
                const std::list<openvdb::GridBase::ConstPtr>&,openvdb::tools::VolumeToMesh&,
                bool,MDataBlock&,std::vector<meshPrimData>&,dommy::vdb::MeshingBudget&);
                template<typename G>MStatus referenceMeshing(
                const std::list<openvdb::GridBase::ConstPtr>& grids,openvdb::tools::VolumeToMesh& mesher,
                bool normals,MDataBlock& data,std::vector<meshPrimData>& output,dommy::vdb::MeshingBudget& budget) {
                DOMMY_RETURN_IF_FAILURE(referenceIntake<G>(grids,mesher,normals,data,output,budget));
                nativeCall("reference.dispatch"); referenceNames.clear();
                for(const auto& g:grids) referenceNames.push_back(g->getName());
                output.push_back(meshPrimData());output.back().hasExplicitNormals=normals;return MS::kSuccess;}};'''
        result+=declared
        if name=='DOpenVDBToMesh':result+=[extract(source,'namespace {')]
        result += [extract(source,'MStatus '+name+'::compute('),'DOMMY_VDB_COMPUTE_CATCH("'+name+'")']
        if name=='DOpenVDBConvert':
            # Execute the exact reference intake through the native volume boundary.
            # The remaining native/host sharpening and normal path is SDK-checked,
            # not simulated as geometry by this portable test.
            intake=extract(source,'template <class GridType>\nMStatus DOpenVDBConvert::referenceMeshing(')
            intake=intake.split('    openvdb::tools::MeshToVoxelEdgeData edgeData;')[0]
            result += [intake.replace('::referenceMeshing(', '::referenceIntake(')+
                '\n    (void)sizeof(TreeType); (void)backgroundValue; (void)transferAttributes; (void)concernAllPartsNormal;\n'
                '    (void)mesher; (void)computeNormals; (void)primDataList; return MS::kSuccess; }']
    return '\n'.join(result)

def sdk_check(root,sdk):
    code=['#include <maya/'+name+'.h>' for name in ('MPxNode','MFnNumericAttribute',
          'MFnTypedAttribute','MFnGenericAttribute','MFnMeshData','MFnMesh','MFnStringData',
          'MDataHandle','MDataBlock','MPlug','MFloatPointArray','MPointArray','MIntArray',
          'MArrayDataBuilder','MArrayDataHandle','MVectorArray','MFloatVectorArray',
          'MItMeshPolygon','MItMeshVertex','MItMeshEdge','MBoundingBox','MTransformationMatrix','MFnPluginData')]
    code+=['#include <cmath>','#include "common/utility/DComputeGuard.h"',
           '#include "common/utility/DStatusOperation.h"','#include "common/utility/DGenerationLimits.h"',
           '#include "common/utility/DMeshMergePlan.h"','#include "common/utility/DMayaPluginDataOutput.h"',
           '#include "common/utility/DNumericValidation.h"','#include "common/utility/DMayaValueValidation.h"',
           '#include "common/utility/DMayaMeshTraversal.h"','#include "math/dommyMath.h"',
           extract((root/'src/core/common/object/DPolygonUtil.h').read_text(),'class meshPrimData')+';',
           '#include "vdb_meshing_sdk_boundary.h"',
           extract((root/'src/modules/volume/VDB/Common/DOpenVDBGeoUtil.h').read_text(),'namespace {')]
    for name in NODES:
        source=(root/(FOLDER+name+'.cpp')).read_text()
        declared=declarations(name,source,True)
        declared[0]=declared[0].replace(':AttributeNode',':MPxNode')
        declared[1:]=[re.sub(r'\{"[^"]*"\}','',item) for item in declared[1:]]
        if name=='DOpenVDBConvert':
            declared[0]=declared[0][:-2]+'''template<typename G>MStatus referenceMeshing(
                const std::list<openvdb::GridBase::ConstPtr>&,openvdb::tools::VolumeToMesh&,
                bool,MDataBlock&,std::vector<meshPrimData>&,dommy::vdb::MeshingBudget&);};'''
        code+=declared+[extract(source,'MStatus '+name+'::initialize(')]
        if name=='DOpenVDBToMesh':
            code+=['using GridList=GridCPtrVec;',extract(source,'struct ConversionInputs')+';']
            code+=[extract(source,prefix) for prefix in ('MStatus readInputVdb(',
                'MStatus readConversionInputs(','MStatus writeMeshArray(', 'MStatus writeDMeshData(')]
        else:
            code+=[extract(source,'template <class GridType>\nMStatus DOpenVDBConvert::referenceMeshing(')]
    for grid in ('FloatGrid','DoubleGrid'):
        code += ['template MStatus DOpenVDBConvert::referenceMeshing<openvdb::'+grid+'>(const std::list<openvdb::GridBase::ConstPtr>&,openvdb::tools::VolumeToMesh&,bool,MDataBlock&,std::vector<meshPrimData>&,dommy::vdb::MeshingBudget&);']
    command=shlex.split(os.environ.get('CXX','clang++'))+['-std=c++14','-Wall','-Wextra',
        '-Wpedantic','-Werror','-Wno-unknown-pragmas','-Wno-deprecated-declarations',
        '-isystem',str(Path(sdk)/'include'),'-I'+str(root/'src'),'-I'+str(root/'src/core'),
        '-I'+str(root/'tests/cpp')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-meshing-sdk-') as directory:
        path=Path(directory)/'meshing.cpp';path.write_text('\n'.join(code))
        subprocess.run(command+['-fsyntax-only',str(path)],check=True)
        subprocess.run(command+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
                               '-analyzer-werror','-o',os.devnull,str(path)],check=True)
    print('ToMesh/Convert initializers, readers, array/batch DMesh writers, shared converter and referenceMeshing: Maya SDK syntax/analysis passed')
    print('OpenVDB/custom geometry boundaries are declarations only; native kernels and full translation units are NOT covered')

def mutations(root):
    init,body=initializers(root),bodies(root)
    run(root,init,suite='vdb_meshing_init')
    run(root,body,suite='vdb_meshing_compute',arguments=[])
    cases=[
        ('individual Convert insertions',False,body.replace('if (!outDMeshes->insert(stagedMeshes)) return MS::kFailure;',
            'for (const auto& item : stagedMeshes) if (!outDMeshes->insert(DMeshData::DMeshList{item})) return MS::kFailure;')),
        ('individual ToMesh insertions',False,body.replace('if (!outputData->insert(stagedMeshes)) return MS::kFailure;',
            'for (const auto& item : stagedMeshes) if (!outputData->insert(DMeshData::DMeshList{item})) return MS::kFailure;')),
        ('initializer failure ignored',True,init.replace('DOMMY_RETURN_IF_FAILURE(stat);','',1)),
        ('initializer exception escapes',True,init.replace('return dommy::maya::reportEntryPointException(',
            'throw; return dommy::maya::reportEntryPointException(',1)),
        ('failed point allocation consumed',False,body.replace(
            'DOMMY_RETURN_IF_FAILURE(points.setLength(static_cast<unsigned int>(pointCount)));',
            'points.setLength(static_cast<unsigned int>(pointCount));')),
        ('failed index allocation consumed',False,body.replace(
            'DOMMY_RETURN_IF_FAILURE(indices.setLength(static_cast<unsigned int>(connects)));',
            'indices.setLength(static_cast<unsigned int>(connects));')),
        ('failed mesh creation accepted',False,body.replace('DOMMY_RETURN_IF_FAILURE(result);','')),
        ('null mesh accepted',False,body.replace('if (created.isNull()) return MS::kFailure;','')),
        ('mesh array element failure ignored',False,body.replace('DOMMY_RETURN_IF_FAILURE(element.set(meshes[index]));',
            'element.set(meshes[index]);')),
        ('grid selection ignored',False,body.replace('getGrids(selectedGrids, *inputVdb, names);',
            'getGrids(selectedGrids, *inputVdb, "*");')),
        ('insertion failure ignored',False,body.replace('if (!outDMeshes->insert(stagedMeshes)) return MS::kFailure;',
            'outDMeshes->insert(stagedMeshes);')),
        ('primitive index unchecked',False,body.replace('if (!dommy::memory::indexInRange(index, pointCount)) return MS::kFailure;','')),
        ('native point flags unchecked',False,body.replace('if (!flags.empty() && flags.size() != pointCount) return MS::kFailure;','')),
        ('reference primitive map unchecked',False,body.replace(
            'static_cast<std::size_t>(*it) >= primitives.size()', 'false')),
        ('reference transform unchecked',False,body.replace('if (grid.transform() != transform) return false;',
            'if (grid.transform() != transform && false) return false;')),
        ('late invalid grid reaches kernel',False,body.replace(
            'if (!dommy::vdb::planMeshingGrids(inputs.grids, budget)) return MS::kInvalidParameter;', '')),
    ]
    for label,initialization,changed in cases:
        assert changed!=(init if initialization else body),label
        try:run(root,changed,capture=True,suite='vdb_meshing_init' if initialization else 'vdb_meshing_compute',arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode!=1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer','runtime error:')),'Compilation failure is not detection: '+label
            print('Detected mutation: '+label,flush=True)
        else:raise AssertionError('Undetected mutation: '+label)

if __name__=='__main__':
    root=Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root,sys.argv[sys.argv.index('--sdk')+1]);sys.exit(0)
    if '--mutations' in sys.argv:
        mutations(root);sys.exit(0)
    run(root,initializers(root),suite='vdb_meshing_init',arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv:
        run(root,bodies(root),suite='vdb_meshing_compute',arguments=[])
