#!/usr/bin/env python3
"""Exercise output admission, geometry generation, batch and attribute transfer.

Float storage is real; Maya matrix/array/custom-data APIs are explicit seams.
This does not run the complete node, DG publication or native Maya ownership.
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

ROOT = Path(__file__).resolve().parent.parent
NODE = 'src/modules/point/DPoint/mesh/DPMeshDuplicator.cpp'
DATA = 'src/core/common/data/DDataOp.h'


def body(source=None, sdk=False):
    source = source if source is not None else (ROOT / NODE).read_text()
    compute = extract(source, 'MStatus DPMeshDuplicator::compute(')
    start = compute.index('std::vector<dommy::mesh::DuplicationSourceSize> sourceSizes;')
    end = compute.index('if (transAttrNameArray.length())', start)
    result = []
    result.append(extract(source, 'namespace {'))
    if not sdk:
        conversions = (ROOT / DATA).read_text()
        result.append(extract(conversions, 'inline size_t DGetGlobalProcessNum('))
        signature = 'void DMayaVecArrayToVecVectorList('
        template = conversions.rfind('template <', 0, conversions.index(signature))
        result.append(conversions[template:conversions.index(signature)] +
                      extract(conversions, signature))
        values = (ROOT / 'src/core/common/utility/DMayaValueValidation.h').read_text()
        result.append('namespace dommy {namespace maya {')
        result.extend(extract(values, signature) for signature in (
            'inline bool isFiniteMatrix(', 'inline bool isFinitePoint(',
            'inline bool isFloatRepresentablePoint('))
        result.append('}}')
    result.append('''
MStatus generateDuplicateGeometry(
    const std::vector<dupMesh>& dupShapeArray,
    const std::vector<int>& objTypeIds,
    const std::vector<MMatrix>& matrices,
    const std::vector<MVector>& scaleExprArray,
    bool aIsOutUVVal, int aUVTypeVal, int matrixAssignType,
    const DPData& pointData, const MStringArray& transAttrNameArray,
    DMeshData& destination) try {
    const size_t totalCount = objTypeIds.size();
    const auto* matrixArray = &matrices;
    auto* outDMeshes = &destination;
    const auto* inputDPData = &pointData;
    // Raw name-control admission belongs to the separate compute-prefix suite.
    // This boundary starts with already split name arrays.
    dommy::point::PointPayloadBudget transferNameStorage;
    const double alongUVRatio = totalCount ? 1.0 / double(totalCount) : 0.0;
    const size_t numTileU = size_t(std::ceil(std::sqrt(double(totalCount))));
    const size_t numTileV = numTileU;
''')
    result.extend((compute[start:end], extract(compute, 'if (transAttrNameArray.length())'),
                   'return MS::kSuccess;}',
                   'DOMMY_COMPUTE_CATCH("DPMeshDuplicator")'))
    return '\n\n'.join(result)


def check(source=None, capture=False, budget=False, overrides=None):
    headers = dict(overrides or {})
    if budget:
        for name, old, new in (
                ('DAttributeArrayConversion.h', 'maximum = 256U * 1024U * 1024U', 'maximum = 8192U'),
                ('DMeshMergePlan.h', 'kMaxMeshOperationBytes = 256U * 1024U * 1024U',
                 'kMaxMeshOperationBytes = 8192U')):
            content = headers.get('common/utility/' + name,
                                  (ROOT / 'src/core/common/utility' / name).read_text())
            assert old in content
            content = content.replace(old, new, 1)
            headers['common/utility/' + name] = content
            headers[name] = content
    return run(ROOT, body(source), suite='mesh_duplication_vertices',
               arguments=['budget'] if budget else [], capture=capture,
               header_overrides=headers,
               extra_flags=['-I' + str(ROOT / 'src/core/common/utility')])


def sdk_check(sdk):
    includes = ['-I' + str(folder) for folder, _, _ in os.walk(ROOT / 'src')]
    code = '''
#include "common/utility/DComputeGuard.h"
#include "common/utility/DStatusOperation.h"
#include "common/utility/DMayaValueValidation.h"
#include "common/utility/DUvParameter.h"
#include "DDataOp.h"
#include "dupMesh.h"
#include "DMeshDuplicationPlan.h"
#include "DPData.h"
#include "common/utility/DPointPayloadBudget.h"
#include "DMayaDebug.h"
#include <map>
#include <cstring>
#include <maya/MTransformationMatrix.h>
inline MString name() { return "DPMeshDuplicator SDK boundary"; }
''' + body(sdk=True)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(ROOT / 'third_party/libnoise/source'),
        '-idirafter', str(ROOT / 'tests/stubs'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Production duplication admission/geometry/transfer and real array/data headers: SDK syntax/analysis passed')


def mutations():
    check()
    source = (ROOT / NODE).read_text()
    for label, old, new in (
        ('finite-only admission restored', 'isFloatRepresentablePoint(pt)', 'isFinitePoint(pt)'),
        ('final vertex skipped', 'p < baseVtxArray.length()', 'p + 1U < baseVtxArray.length()'),
        ('axis source corrupted', 'static_cast<float>(pt[j])', 'static_cast<float>(pt[0])'),
        ('freeze mode reversed', 'if (matrixAssignType == 1)', 'if (matrixAssignType == 0)'),
        ('expression scale status ignored',
         'DOMMY_RETURN_IF_FAILURE(expressionScale.setScale(', '(void)(expressionScale.setScale('),
        ('matrix composition order reversed', 'mat = expressionScale.asMatrix() * mat;',
         'mat = mat * expressionScale.asMatrix();'),
    ):
        assert source.count(old) == 1, label
        try:
            check(source.replace(old, new, 1), capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_duplication_vertices_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def admission_mutations():
    source = (ROOT / NODE).read_text()
    payload = 'dommy::point::preflightAttributePayload(selected.begin(), selected.end(),'
    for label, old, new in (
            ('transferred payload admission omitted', payload, 'if (false) ' + payload),
            ('retained workspace omitted', 'retained.estimatedBytes()', '0U'),
            ('discarded source default charged', 'input.numberOfPoints(), workspace, false)',
             'input.numberOfPoints(), workspace, true)')):
        assert source.count(old) == 1, label
        try:
            check(source.replace(old, new, 1), capture=True, budget=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'mesh_duplication_vertices_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected duplication admission mutation:', label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)
    path = 'modules/point/DPoint/mesh/DMeshDuplicationPlan.h'
    header = (ROOT / 'src' / path).read_text()
    old = 'if (!validMeshOperationStorage(geometry, extraBytes)) return false;'
    assert old in header
    try:
        check(capture=True, budget=True, overrides={path: header.replace(old,
            '(void)extraBytes;', 1)})
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'mesh_duplication_vertices_tests', 'Compile failure: geometry admission omitted'
        assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr
        print('Detected duplication admission mutation: geometry admission omitted', flush=True)
    else:
        raise AssertionError('Undetected mutation: geometry admission omitted')
    from check_procedural_api_safety import check as prefix_check
    old = 'transferNameStorage.add(std::strlen(transAttrStr.asChar()), sizeof(char));'
    assert source.count(old) == 1
    try:
        prefix_check({NODE: source.replace(old, ';', 1)},
                     arguments=('duplicator-budget',), capture=True, budget=True)
    except subprocess.CalledProcessError as error:
        assert Path(error.cmd[0]).name == 'procedural_api_tests', 'Compile failure: raw transfer names unbounded'
        assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr
        print('Detected duplication admission mutation: raw transfer names unbounded', flush=True)
    else:
        raise AssertionError('Undetected mutation: raw transfer names unbounded')


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()
        check(budget=True)
        admission_mutations()
