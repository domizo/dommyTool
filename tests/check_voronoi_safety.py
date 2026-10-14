#!/usr/bin/env python3
"""Run production Voronoi crop/publication bodies with injectable Maya boundaries."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

POLYGON = 'src/core/common/object/DPolygonUtil.cpp'
VORONOI = 'src/modules/point/DPoint/mesh/DPVoronoi.cpp'


def bodies(root, sdk=False):
    polygon = (root / POLYGON).read_text()
    voronoi = (root / VORONOI).read_text()
    result = []
    if not sdk:
        result += ['namespace dommy { namespace maya {']
        for path, signatures in (
            ('DMayaMeshTraversal.h', ('MStatus visitMeshElements(',)),
            ('DMayaMeshImport.h', ('inline MStatus preflightMeshImport(',)),
            ('DMayaValueValidation.h', ('inline bool isFinitePoint(',
                                      'inline bool isFloatRepresentablePoint('))):
            source = (root / 'src/core/common/utility' / path).read_text()
            for signature in signatures:
                if 'visitMeshElements' in signature:
                    result.append('template<class Iterator, class Visitor>')
                result.append(extract(source, signature))
        result.append('}}')
    for signature in ('MStatus findCollapsibleEdge(', 'MStatus DPolyIsIntersectMeshCheck('):
        result.append(extract(polygon, signature))
    for signature in ('bool isFinitePoint(', 'bool pointBounds(', 'MStatus buildCropMesh(',
                      'MStatus prepareBoundsAndCrop(', 'MStatus validateLineTopology(',
                      'MStatus publishCurveOutput(', 'MStatus clipCellsToMesh(',
                      'MStatus publishMeshOutput('):
        result.append(extract(voronoi, signature))
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-unused-parameter', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
        '-I' + str(root / 'tests/cpp'), '-I' + str(root)]
    with tempfile.TemporaryDirectory(prefix='dommy-voronoi-sdk-') as directory:
        path = Path(directory) / 'voronoi.cpp'
        path.write_text('#include "voronoi_sdk_boundaries.h"\n' + bodies(root, True))
        subprocess.run(command + ['-fsyntax-only', str(path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                       '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, str(path)], check=True)
    print('Voronoi crop and publication bodies: real Maya SDK syntax/analysis passed')
    print('Custom data and Voro++ boundaries are declarations only; no linking or Maya execution')


def mutations(root):
    source = bodies(root)
    run(root, source, suite='voronoi_safety', arguments=[])
    variants = [
        ('individual mesh insertions', 'if (!output->insert(stagedMeshes)) return MS::kFailure;',
         'for (const auto& item : stagedMeshes) if (!output->insert(DMeshData::DMeshList{item})) return MS::kFailure;'),
        ('crop failure ignored', 'DOMMY_RETURN_IF_FAILURE(DPolyIsIntersectMeshCheck(\n'
         '                cropMesh, cells, keep, true, tolerance));',
         '(void)DPolyIsIntersectMeshCheck(cropMesh, cells, keep, true, tolerance);'),
        ('Boolean failure ignored', 'DOMMY_RETURN_IF_FAILURE(target.booleanOps(MFnMesh::kIntersection, operands, true));',
         '(void)target.booleanOps(MFnMesh::kIntersection, operands, true);'),
        ('cleanup failure ignored', 'DOMMY_RETURN_IF_FAILURE(cache.release());', '(void)cache.release();'),
        ('selection published early', 'std::vector<bool> candidate(targetObjects.size(), false);',
         'std::vector<bool> candidate(targetObjects.size(), false); isCheckedArray = candidate;'),
        ('crop world space', 'inputFunction.getPoints(points, MSpace::kObject)', 'inputFunction.getPoints(points, MSpace::kWorld)'),
        ('vertex world space', 'vertex.position(MSpace::kObject, &status)', 'vertex.position(MSpace::kWorld, &status)'),
        ('edge world space', 'edge.point(0, MSpace::kObject, &status)', 'edge.point(0, MSpace::kWorld, &status)'),
        ('import budget omitted', '!mesh::validMeshOperationStorage(candidate)', 'false'),
        ('point cardinality unchecked', 'points.length() != plan.points ||', ''),
        ('topology sum unchecked', 'if (totalConnections != plan.connections) return MS::kInvalidParameter;', ''),
        ('curve allocation status ignored', 'DOMMY_RETURN_IF_FAILURE(dommy::maya::buildCheckedArray(',
         '(void)(dommy::maya::buildCheckedArray('),
        ('zero-hit ray not outside', 'else outer = true;', 'else (void)outer;'),
    ]
    for label, old, new in variants:
        assert old in source, label
        try:
            run(root, source.replace(old, new, 1), capture=True,
                suite='voronoi_safety', arguments=[])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name.endswith('_tests'), 'Compile failure: ' + label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, bodies(root), suite='voronoi_safety', arguments=[])
