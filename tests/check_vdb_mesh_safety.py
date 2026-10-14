#!/usr/bin/env python3
"""Mesh conversion, volume emitter and FromMesh production-boundary regression tests."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run
from check_vdb_distribution_safety import compute_bodies as distribution_bodies

NODE = 'src/modules/volume/VDB/Generate/DOpenVDBFromMesh.cpp'
POLICY = 'src/modules/volume/VDB/Edit/DOpenVDBMeshConversionPolicy.h'
EMITTER = 'src/core/DDataUtility/Generate/DPointVolumeEmit.cpp'

def initializers(root):
    source=(root/NODE).read_text()
    return '\n'.join(declarations('DOpenVDBFromMesh',source,True)+[
        extract(source,'MStatus DOpenVDBFromMesh::initialize(')])

def compute_bodies(root):
    source=(root/NODE).read_text()
    adapter=(root/'src/core/common/vdb/DVdbOp.cpp').read_text()
    emitter=(root/EMITTER).read_text()
    code=[distribution_bodies(root).split('struct DOpenVDBScatter {')[0],
          extract((root/POLICY).read_text(),'namespace dommy {'),
          extract((root/'src/core/common/utility/DMayaMeshTraversal.h').read_text(),'namespace dommy {'),
          extract(adapter,'MStatus preflightVdbMeshInput('),
          extract((root/'src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp').read_text(),
                  'MStatus DOpenVDBMakePointList(const MObject&'),
          extract(adapter,'bool GenerateVdbMeshOp(')]
    for name,body in [('DOpenVDBFromMesh',source),('DPointVolumeEmit',emitter)]:
        declared=declarations(name,body,False)
        declared[0]=declared[0].replace('MStatus compute(', 'MObject thisMObject() const {return {"node"};} MStatus compute(')
        code+=declared
    code += [extract(source,'namespace {'),extract(source,'MStatus DOpenVDBFromMesh::compute('),
             'DOMMY_VDB_COMPUTE_CATCH("DOpenVDBFromMesh")',
             extract(emitter,'MStatus readVolumeMeshScatter(')]
    header=(root/EMITTER).with_suffix('.h').read_text()
    code += [re.search(r'enum\s*\{[^}]+\};typedef int D_VOLUME_EMIT_TYPE;',header).group(0),
             'const float kMaxSafeCoordinate=std::sqrt(std::numeric_limits<float>::max());']
    # Use unchanged common input/output paths and the complete mesh case. Other
    # emitter modes retain their separate packing/sampling test coverage.
    body=extract(emitter,'MStatus DPointVolumeEmit::compute(')
    code += [body.split('switch (emitTypeVal)')[0]+'switch (emitTypeVal) {\n'+
             extract(body,'case D_VOLUME_EMIT_MESH:')+' break; default: return MS::kInvalidParameter;}\n'+
             body[body.rindex('        MObject outputObject;'):],
             'DOMMY_VDB_COMPUTE_CATCH("DPointVolumeEmit")']
    return '\n'.join(code)

def sdk_check(root,sdk):
    code=['#include <maya/'+name+'.h>' for name in ('MPxNode','MFnNumericAttribute',
          'MFnTypedAttribute','MFnMeshData','MFnMesh','MFnStringData','MDataHandle',
          'MDataBlock','MPlug','MFloatPointArray')]
    code += ['#include <cmath>','#include <sstream>','#include <vector>','#include <memory>',
             '#include "common/string/DStreamFormat.h"',
             '#include "math/DBounds.h"','#include "common/utility/DNumericValidation.h"',
             '#include "common/utility/DStatusOperation.h"','#include "common/utility/DComputeGuard.h"',
             '#include "common/utility/DMayaMeshTraversal.h"',r'''
// OpenVDB/custom geometry are declarations only in this Maya SDK check.
namespace openvdb {using Index64=std::uint64_t;}
namespace dommy {namespace vdb {
struct TreeEditBudget {bool addBytes(std::size_t,std::size_t);bool addWork(std::size_t,std::size_t);};
''',extract((root/POLICY).read_text(),'struct MeshConversionSettings')+';',
             extract((root/POLICY).read_text(),'inline bool validMeshConversionSettings('),
             extract((root/'src/modules/volume/VDB/Edit/DOpenVDBScatterPolicy.h').read_text(),'struct ScatterSettings')+';','}}',r'''
struct DOpenVDBData {static const MTypeId id;};
struct CustomMesh {const std::vector<MPoint>* getPositionPtrConst()const;};
struct DMeshData {
static const MTypeId id;
using Iterator=std::vector<std::shared_ptr<CustomMesh>>::const_iterator;
Iterator beginMeshes()const;Iterator endMeshes()const;
};
MStatus getValidInputDM(const MObject&,MDataBlock&,const DMeshData*&);
''']
    for name,path in [('DOpenVDBFromMesh',NODE),('DPointVolumeEmit',EMITTER)]:
        source=(root/path).read_text()
        declared=declarations(name,source,True)
        declared[0]=declared[0].replace(':AttributeNode',':MPxNode')
        declared[1:]=[re.sub(r'\{"[^"]*"\}','',item) for item in declared[1:]]
        code+=declared
        if name=='DOpenVDBFromMesh':
            code += [extract(source,'MStatus DOpenVDBFromMesh::initialize('),
                     extract(source,'struct FromMeshSettings')+';']
            code += [extract(source,prefix) for prefix in ('MStatus readFromMeshSettings(',
                     'MStatus readFromMeshObject(','MStatus estimateFromMeshResolution(')]
        else:code += [extract(source,'MStatus readVolumeMeshScatter(')]
    code += [extract((root/'src/core/common/vdb/DVdbOp.cpp').read_text(),'MStatus preflightVdbMeshInput(')]
    command=shlex.split(os.environ.get('CXX','clang++'))+[
        '-std=c++14','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-deprecated-declarations',
        '-isystem',str(Path(sdk)/'include'),'-I'+str(root/'src'),'-I'+str(root/'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-vdb-mesh-sdk-') as directory:
        path=Path(directory)/'mesh.cpp';path.write_text('\n'.join(code))
        subprocess.run(command+['-fsyntax-only',str(path)],check=True)
        subprocess.run(command+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
                               '-analyzer-werror','-o',os.devnull,str(path)],check=True)
    print('FromMesh initializer, readers, estimated resolution, mesh preflight and emitter scatter reader: Maya SDK syntax/analysis passed')
    print('Native OpenVDB and complete compute translation units are NOT covered')

def mutations(root):
    init,body=initializers(root),compute_bodies(root)
    cases=[
        ('initializer failure ignored',True,init.replace('DOMMY_RETURN_IF_FAILURE(stat);','',1)),
        ('initializer exception escapes',True,init.replace('return dommy::maya::reportEntryPointException(',
             'throw; return dommy::maya::reportEntryPointException(',1)),
        ('voxel scale unchecked',False,body.replace('settings.voxelSize >= 1e-6f','true')),
        ('coordinate limit unchecked',False,body.replace('std::fabs(box.min()[axis]) > 1e6 || std::fabs(box.max()[axis]) > 1e6','false')),
        ('mesh workspace uncharged',False,body.replace('chargeDenseTreeRegion<GridType>(checked, budget, outputCopies + 2, 64, 128)','true')),
        ('primitive index unchecked',False,body.replace('if (primitive[axis] >= points.size()) return false;','')),
        ('native map unchecked',False,body.replace('if (grid.transform() != transform) return false;',
             'if (grid.transform() != transform && false) return false;')),
        ('empty fog passed to native',False,body.replace('if (grid->empty()) {','if (false) {')),
        ('estimated resolution narrows unchecked',False,body.replace('!dommy::numeric::checkedNumericCast(cells, resolution[axis])',
             '(resolution[axis]=static_cast<int>(cells), false)')),
        ('failed settings handle consumed',False,body.replace('const auto distance = data.inputValue(DOpenVDBFromMesh::aExportDistanceGrid, &status);\n    DOMMY_RETURN_IF_FAILURE(status);',
             'const auto distance = data.inputValue(DOpenVDBFromMesh::aExportDistanceGrid, &status);')),
        ('Maya mesh read status ignored',False,body.replace('!mesh.getPoints(vertices, MSpace::kWorld)',
             '(mesh.getPoints(vertices, MSpace::kWorld), false)')),
        ('info publication omitted',False,body.replace('infoHandle.set(infoText);','')),
        ('emitter mode unchecked',False,body.replace('if (pmode < 0 || pmode > 2) return MS::kInvalidParameter;','')),
        ('emitter spread unchecked',False,body.replace('spread < 0 || spread > 1','false')),
    ]
    for label,initialization,changed in cases:
        assert changed!=(init if initialization else body),label
        try:run(root,changed,capture=True,suite='vdb_mesh_init' if initialization else 'vdb_mesh_compute',arguments=None if initialization else [])
        except subprocess.CalledProcessError as error:
            assert error.returncode!=1 or any(marker in (error.stderr or '') for marker in
                ('ERROR: AddressSanitizer','runtime error:')),'Compilation failure is not detection: '+label
            print('Detected mutation: '+label,flush=True)
        else:raise AssertionError('Undetected mutation: '+label)

if __name__ == '__main__':
    root=Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root,sys.argv[sys.argv.index('--sdk')+1]);sys.exit(0)
    if '--mutations' in sys.argv:
        mutations(root);sys.exit(0)
    run(root,initializers(root),suite='vdb_mesh_init',arguments=['--emit'] if '--emit' in sys.argv else None)
    if '--emit' not in sys.argv:
        run(root,compute_bodies(root),suite='vdb_mesh_compute',arguments=[])
