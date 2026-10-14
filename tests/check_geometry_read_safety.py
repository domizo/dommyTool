#!/usr/bin/env python3
"""Execute production geometry acquisition/traversal boundaries with host doubles.

This covers allocation ordering and status/cardinality checks, not complete
DPScatter VDB evaluation or Maya's internal geometry/cache allocation.
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run


def bodies(root, overrides=None):
    overrides = overrides or {}

    def read(path):
        return overrides.get(path, (root / path).read_text())

    animation = read('src/core/DDataUtility/Generate/DPAnimationEmit.cpp')
    scatter = read('src/core/DDataUtility/Generate/DPScatter.cpp')
    result = [extract(read('src/core/common/utility/DMayaMeshTraversal.h'), 'namespace dommy {'),
              extract(read('src/core/common/object/DNodeOperation.cpp'), 'MStatus getMeshInformation(')]
    geometry = animation[animation.index('    MPointArray sourcePoints;'):
                         animation.index('    dommy::math::RandomGenerator randomGenerator')]
    result.append('MStatus readAnimationGeometry(MDataBlock& block, int emitTypeValue, MPointArray& result) {'
                  'MStatus status; MObject inputGeometry{"geometry"}, outputPointArray{"output"};'
                  'MPlug plug({},outputPointArray); MPointArray outputPoints;' + geometry +
                  'result = sourcePoints; return MS::kSuccess;}')
    vertices = scatter[scatter.index('        const int rawVertexCount ='):
                       scatter.index('        MPoint minPoint, maxPoint;')]
    triangles = scatter[scatter.index('        MIntArray triCountArray, triVtxArray;'):
                        scatter.index('        m_triIds.clear();')]
    result.append('MStatus readScatterMesh(const MObject& meshObj) {MStatus stat;'
                  'MFnMesh m_meshFn(meshObj,&stat); if(!stat)return stat; MPointArray m_pts;' +
                  vertices + triangles + 'return MS::kSuccess;}')
    area = scatter[scatter.index('                MDataHandle meshHandle ='):
                   scatter.index('                // 一つあたりの半径の計算')]
    result.append('MStatus readScatterArea(MDataBlock& data, std::size_t faceCount, double& result) {'
                  'MStatus stat; MObject inObject{"geometry"};' + area +
                  'result = totalArea; return MS::kSuccess;}')
    bounds = animation[animation.index('    double minimumTrailLength ='):
                       animation.index('    std::vector<MMatrix> matrices;')]
    result.append('MStatus readAnimationBounds(double timeLengthValue, double timeLengthRandomValue,'
                  'bool randomizeTrailLength, double& lower, double& upper) try {' + bounds +
                  'lower=minimumTrailLength; upper=maximumTrailLength; return MS::kSuccess;}'
                  'DOMMY_COMPUTE_CATCH("animation bounds")')
    start = animation.index('    const MPlug matrixPlug(thisMObject(), transformMatrix);')
    end = animation.index('    const unsigned int matrixCount =', start)
    result.append('MStatus readAnimationConnection(MDataBlock& block, bool& admitted) try {'
                  'MStatus status; const MObject transformMatrix{"matrix"}, outputPointArray{"output"};'
                  'MPlug plug({},outputPointArray); MPointArray outputPoints;' + animation[start:end] +
                  'admitted=true; return MS::kSuccess;} DOMMY_COMPUTE_CATCH("animation connection")')
    return '\n\n'.join(result)


def check_sdk(root, sdk):
    headers = ('MDataBlock', 'MDataHandle', 'MFnNurbsCurve', 'MFnNurbsSurface', 'MFnMesh',
               'MItMeshPolygon', 'MPointArray', 'MFloatPointArray', 'MIntArray', 'MBoundingBox',
               'MGlobal', 'MPlug')
    source = '\n'.join('#include <maya/' + h + '.h>' for h in headers)
    source += '''
#include "common/utility/DGenerationLimits.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DRandomValueGeneration.h"
#include "math/DBounds.h"
#include <limits>
using DReal = float;
enum {DP_ANIM_EMIT_MATRIX, DP_ANIM_EMIT_POINT, DP_ANIM_EMIT_RANDOM};
MStatus finishOutput(MDataBlock&, const MPlug&, const MObject&, const MPointArray&);
MObject thisMObject();
namespace dommy { namespace packing {
bool isFinite(const MPoint&);
} }
'''
    source += bodies(root).replace('{"geometry"}', '{}').replace('{"output"}', '{}').replace('{"matrix"}', '{}')
    common = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(common + flags, input=source, text=True, check=True)
    print('Geometry read/traversal boundaries: real SDK syntax/analyzer passed')


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--sdk' in sys.argv:
        check_sdk(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        generated = bodies(root)
        old = 'const bool matrixConnected = matrixPlug.isConnected(&status);\n    DOMMY_RETURN_IF_FAILURE(status);'
        assert old in generated
        try:
            run(root, generated.replace(old, 'const bool matrixConnected = matrixPlug.isConnected(&status);', 1),
                suite='geometry_read', arguments=[], capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'geometry_read_tests', 'Mutation compile failure'
            print('Detected mutation: animation connection error becomes empty output')
        else:
            raise AssertionError('Undetected animation connection mutation')
    else:
        run(root, bodies(root), suite='geometry_read', arguments=[])
