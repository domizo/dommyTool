#!/usr/bin/env python3
"""Exact corner-normal helpers, merge transaction and n-gon face mapping."""
from pathlib import Path
import sys
import os, re, shlex, subprocess, tempfile
sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run
from check_point_utility_safety import declarations

def rest_initializer(root):
    source=(root/'src/modules/mesh/DMesh/Dynamics/Utility/DMRest.cpp').read_text()
    return '\n'.join(declarations('DMRest',source,True)+[extract(source,'MStatus DMRest::initialize(')])

def bodies(root):
    geo = (root / 'src/modules/volume/VDB/Common/DOpenVDBGeoUtil.cpp').read_text()
    normals = (root / 'src/modules/mesh/DMesh/common/DMeshNormals.h').read_text()
    data = (root / 'src/modules/mesh/DMesh/Data/DMeshData.cpp').read_text()
    convert = (root / 'src/modules/mesh/DMesh/Convert/DMConvertFromMeshes.cpp').read_text()
    values=(root/'src/core/common/utility/DMayaValueValidation.h').read_text()
    return '\n'.join([extract((root/'src/core/common/object/DPolygonUtil.h').read_text(),
                             'class meshPrimData')+';',
        'namespace dommy { namespace maya {',
        extract(values,'inline bool isFiniteVector('),
        extract(values,'inline bool isFloatRepresentableVector('), '}}',
        extract((root/'src/core/common/utility/DMayaMeshImport.h').read_text(),
                             'namespace dommy {'),
        extract(normals, 'namespace dommy {'),
        extract((root/'src/core/common/object/DPolygonUtil.cpp').read_text(),
                'MStatus meshPrimData::setNormalPerVtxPerFace('),
        extract(data, 'MStatus DMeshInternalData::createMerged('),
        extract(data, 'MStatus DMeshInternalData::bakeTransform('),
        extract(data, 'MStatus DMeshInternalData::createFromMayaObject('),
        extract(convert, 'struct MeshInput')+';',
        extract(convert, 'MStatus createFullMeshItem('),
        extract(geo, 'bool validVertexIndex('), extract(geo, 'bool appendPrimitive('),
        extract(geo, 'MStatus DOpenVDBMakePrimList(const MObject&')])

def sdk_check(root, sdk):
    fixture=(root/'tests/cpp/mesh_normal_tests.cpp').read_text()
    code=['#include <maya/'+n+'.h>' for n in ('MFnMesh','MItMeshPolygon','MIntArray',
          'MPointArray','MFloatArray','MFloatVectorArray','MVectorArray','MMatrix','MGlobal',
          'MPxNode','MFnTypedAttribute','MFnEnumAttribute','MTransformationMatrix')]
    code+=['#include <memory>','#include <cstdint>','#include <map>',
        '#include "common/utility/DPointTransform.h"','#include "common/utility/DMeshMergePlan.h"',
        '#include "common/utility/DStatusOperation.h"',
        '#include "common/utility/DMayaEntryPointGuard.h"',
        'MStatus operation(); using DPoint=MVector; using DPointList=std::vector<DPoint>;',
        extract((root/'src/modules/mesh/DMesh/Data/DMeshData.h').read_text(),
                'enum class DMeshNormalImportPolicy')+';',
        extract(fixture,'struct DPAttributeData')+';',extract(fixture,'struct DPVec3fAttr')+';',
        'const int D_COMP_PER_VTX=4;',extract(fixture,'struct DMeshInternalData')+';',
        extract(fixture,'namespace openvdb {'),'using DMItem=DMeshInternalData::Ptr;',bodies(root)]
    rest=rest_initializer(root).replace(':AttributeNode',':MPxNode')
    rest=re.sub(r'\{"(?:inDM|aType|outDM)"\}', '', rest)
    code+=['struct DMeshData {static const MTypeId id;};',rest]
    cmd=shlex.split(os.environ.get('CXX','clang++'))+['-std=c++14','-Wall','-Wextra','-Werror',
        '-Wno-deprecated-declarations','-isystem',str(Path(sdk)/'include'),'-I'+str(root/'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-normal-sdk-') as directory:
        path=Path(directory)/'normal.cpp';path.write_text('\n'.join(code))
        subprocess.run(cmd+['-fsyntax-only',str(path)],check=True)
        subprocess.run(cmd+['--analyze','-Xanalyzer','-analyzer-output=text','-Xanalyzer',
            '-analyzer-werror','-o',os.devnull,str(path)],check=True)
    print('Corner normals, bake transaction, merge, primitive mapping and DMRest initializer: Maya SDK syntax/analysis passed (custom storage/OpenVDB boundaries declared)')

def merge_storage(root, source=None, capture=False):
    header = (root/'src/core/common/utility/DMeshMergePlan.h').read_text()
    reduced = header.replace('256U * 1024U * 1024U', '1500U')
    assert reduced != header
    return run(root, source or bodies(root), capture=capture, suite='mesh_normal',
               arguments=['--merge-storage'],
               header_overrides={'common/utility/DMeshMergePlan.h': reduced})

def mutations(root):
    original=bodies(root)
    for label,old,new in [
        ('normal transform omitted','transformed ? mesh.getTransformMatrix() : MMatrix::identity','transformed ? MMatrix::identity : MMatrix::identity'),
        ('merged normals dropped','preserveNormals = preserveNormals || dommy::mesh::hasCornerNormals(*source);','preserveNormals = false;'),
        ('primitive face mapping lost','candidate.size() - primitiveStart, faceIndex','candidate.size() - primitiveStart, 0'),
        ('mesh binding failure ignored','DOMMY_RETURN_IF_FAILURE(status);',''),
        ('normal marker omitted','if (status) hasExplicitNormals = true;',''),
        ('baked normals not committed','if (hasNormals) normalEntry->second.swap(normalAttribute);',''),
        ('baked matrix not reset','m_mat = MMatrix::identity;',''),
        ('import normals dropped','DOMMY_RETURN_IF_FAILURE(dommy::mesh::captureCornerNormals(object, *candidate, space));',
         '(void)space;'),
        ('import normal space lost','mesh.getNormals(normals, space)',
         'mesh.getNormals(normals, (static_cast<void>(space), MSpace::kObject))'),
        ('import transform bake omitted','DOMMY_RETURN_IF_FAILURE(candidate->bakeTransform(input.worldMatrix));',''),
        ('normal import workspace unbounded','const std::size_t limit = 128U * 1024U * 1024U;',
         'const std::size_t limit = std::numeric_limits<std::size_t>::max();'),
        ('mesh import preflight omitted',
         'DOMMY_RETURN_IF_FAILURE(dommy::maya::preflightMeshImport(meshFunction, plan));',
         '(void)plan;'),
        ('mesh point count mismatch ignored',
         'if (points.length() != plan.points) return MS::kFailure;', ''),
        ('mesh topology count mismatch ignored',
         'if (faceCounts.length() != plan.faces || connections.length() != plan.connections)\n        return MS::kFailure;', ''),
        ('mesh UV count mismatch ignored',
         'if (uCoordinates.length() != plan.uvs || vCoordinates.length() != plan.uvs)\n            return MS::kFailure;', ''),
        ('mesh UV assignment count mismatch ignored',
         'if (uvCounts.length() != plan.faces || uvConnections.length() > plan.connections)\n            return MS::kFailure;', ''),
    ]:
        assert old in original,label
        try:run(root,original.replace(old,new),capture=True,suite='mesh_normal',arguments=[])
        except subprocess.CalledProcessError as error:
            assert error.returncode!=1 or 'runtime error:' in (error.stderr or ''), 'Compile failure is not detection: '+label
            print('Detected mutation: '+label)
        else:raise AssertionError('Undetected mutation: '+label)
    for label, new in (
        ('ordinary merge byte admission omitted',
         '(!preserveNormals || dommy::mesh::validMeshOperationStorage(plan, normalBytes))'),
        ('normal staging not combined with geometry',
         'dommy::mesh::validMeshOperationStorage(plan)'),
    ):
        old = 'dommy::mesh::validMeshOperationStorage(plan, normalBytes)'
        assert old in original
        try:
            merge_storage(root, original.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_normal_tests', 'Compile failure: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    original=rest_initializer(root)
    for label,old,new in [
        ('DMRest initializer failure ignored','DOMMY_RETURN_IF_FAILURE(stat);','CHECK_MSTATUS(stat);'),
        ('DMRest initializer exception escapes','return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
    ]:
        assert old in original
        try:run(root,original.replace(old,new,1),capture=True,suite='mesh_rest_init')
        except subprocess.CalledProcessError as error:
            assert error.returncode!=1, 'Compile failure is not detection: '+label
            print('Detected mutation: '+label)
        else:raise AssertionError('Undetected mutation: '+label)

if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root,sys.argv[sys.argv.index('--sdk')+1]);sys.exit(0)
    if '--mutations' in sys.argv:mutations(root);sys.exit(0)
    if '--emit-rest' in sys.argv:
        run(root,rest_initializer(root),suite='mesh_rest_init',arguments=['--emit']);sys.exit(0)
    run(root, bodies(root), suite='mesh_normal', arguments=[])
    merge_storage(root)
    run(root,rest_initializer(root),suite='mesh_rest_init')
