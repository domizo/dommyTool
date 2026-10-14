#!/usr/bin/env python3
"""Execute curve ownership, input/preflight and initialization boundaries.

Maya APIs are status-injectable doubles, not an in-process geometry emulation.
Complete initializers and focused production compute sections are extracted.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

SOURCES = {name: 'src/modules/curve/DCurve/Generate/' + name + '.cpp'
           for name in ('DCurveFromCurve', 'DCurveFromSurface', 'DCurveClothFromSurface')}
UTILITY = 'src/modules/curve/DCurve/DCurveUtil.cpp'
INPUT = 'src/core/common/object/DNodeOperation.cpp'


def controls(root, overrides=None):
    overrides = overrides or {}
    bodies = []
    markers = ('if (!std::isfinite(parameterOffset)', 'dommy::curve::SurfaceSamplingPlan plan;',
               'if (!std::isfinite(waveHeightValue)')
    for (name, path), marker in zip(SOURCES.items(), markers):
        source = overrides.get(path, (root / path).read_text())
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        bodies.append('struct ' + name + ': AnimationNode { static MStatus initialize(); '
                      'MStatus readControls(const MPlug&, MDataBlock&); ' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        bodies.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        prefix = extract(source, 'MStatus ' + name + '::compute(').split(marker, 1)[0]
        values = re.findall(r'const (short|int|double|bool|MString) (\w+)\s*=', prefix)
        trace = 'std::ostringstream trace; trace << std::setprecision(17);\n'
        for kind, value in values:
            trace += 'trace << "' + value + ' " << ' + value + (
                '.asChar()' if kind == 'MString' else '') + ' << "\\n";\n'
        bodies.append(prefix.replace('::compute(', '::readControls(') + trace +
                      'controlTrace = trace.str(); return MS::kSuccess; }\n'
                      'DOMMY_COMPUTE_CATCH("curve controls")')
    source = overrides.get(SOURCES['DCurveFromCurve'], (root / SOURCES['DCurveFromCurve']).read_text())
    block = source.split('MTransformationMatrix transform;', 1)[1].split(
        'if (!isFiniteMatrix(matrices[sampleIndex]))', 1)[0]
    bodies.append('MStatus transformSample(MMatrix& result) { '
                  'MQuaternion xRotation, yRotation; xRotation.x=0.5; yRotation.y=-0.75; '
                  'MPoint center(3,4,5); std::vector<MMatrix> matrices(1); '
                  'const std::size_t sampleIndex=0; MTransformationMatrix transform;\n' + block +
                  'result=matrices[0]; return MS::kSuccess; }')
    return '\n\n'.join(bodies)


def geometry(root, overrides=None):
    overrides = overrides or {}
    utility = overrides.get(UTILITY, (root / UTILITY).read_text())
    input_source = overrides.get(INPUT, (root / INPUT).read_text())
    bodies = [extract(utility, 'MStatus generateMCurveFromDcurve(ConstDCItem &crv, MObject &newCurve)'),
              extract(utility, 'MStatus generateMCurveFromDcurve(const DCItem &crv, MObject &newCurve)'),
              extract(input_source, 'MStatus DGetMayaCurveFromPlug(')]
    source = overrides.get(SOURCES['DCurveFromCurve'], (root / SOURCES['DCurveFromCurve']).read_text())
    attrs = re.findall(r'^MObject\s+DCurveFromCurve::(\w+);', source, re.M)
    bodies.append('struct DCurveFromCurve: AttributeNode { MObject thisMObject() const {return MObject{"node"};} '
                  'MStatus prepare(const MPlug&, MDataBlock&); ' +
                  ''.join('static MObject ' + a + ';' for a in attrs) + '};')
    for attr in attrs:
        # Host handles use actual attribute identifiers; scene names are frozen separately.
        bodies.append('MObject DCurveFromCurve::' + attr + '={"' + attr + '"};')
    prefix = extract(source, 'MStatus DCurveFromCurve::compute(').split('MRampAttribute twistRamp', 1)[0]
    bodies.append(prefix.replace('::compute(', '::prepare(') +
                  'preparedPlan=plan; preparedObjects=sourceObjects; preparedTangent=useTangentCurve; '
                  'return MS::kSuccess; }\nDOMMY_COMPUTE_CATCH("curve preparation")')
    fit = (root / 'src/modules/curve/DCurve/Utility/DCFitCopy.cpp').read_text()
    query = re.search(r'DOMMY_RETURN_IF_FAILURE\(\s*copyCrvFn.getCVs\([^;]+;', fit).group(0)
    bodies.append('MStatus copyCvs(const MObject& object, MPointArray& copyCvs) { '
                  'MStatus status; MFnNurbsCurve copyCrvFn(object,&status); '
                  'DOMMY_RETURN_IF_FAILURE(status); ' + query + ' return MS::kSuccess; }')
    # Geometry data has no DAG path. Ensure every downstream shape-space query
    # in this node uses the same contract as the executable input/copy tests.
    for match in re.finditer(r'(?:curveFn|tangentFn)\.(?:getCVs|closestPoint|getPointAtParam|tangent|normal)\([^;]+;', source):
        if 'MSpace::kObject' not in match.group(0):
            raise AssertionError('Geometry-data query must use object space: ' + match.group(0))
    bodies.append(connection_stages(root))
    bodies.append(extract(input_source, 'MStatus getTransformNodeFromPlug('))
    return '\n\n'.join(bodies)


def connection_stages(root):
    # Exact connection/input stages from related registered nodes. Geometry
    # generation and DG evaluation are deliberately outside these probes.
    bodies = []
    spiral = (root / 'src/modules/curve/legacy/DSpiralCurve.cpp').read_text()
    start = spiral.index('MDataHandle dataHnd =', spiral.index('MStatus DSpiralCurve::compute('))
    end = spiral.index('//Get curve info', start)
    bodies.append('MStatus spiralConnections() try { MStatus stat; MDataBlock data; '
                  'const MObject inputCurve{"inputCurve"}, inputTangentCurve{"inputTangentCurve"};' +
                  spiral[start:end] + 'preparedTangent=useTangent; ++connectionAdmissions; '
                  'return MS::kSuccess;} DOMMY_COMPUTE_CATCH("spiral connections")')
    for name, path, marker, end_marker, attribute in (
        ('timeShapeConnections', 'src/modules/mesh/DMesh/Generate/DMTimeShapeGen.cpp',
         'MPlug meshPlug(thisMObject(), inMesh);', 'MDataHandle meshHnd =', 'inMesh'),
        ('edgeDirectionConnections', 'src/modules/mesh/DMesh/Utility/DMEdgeDisplaceDir.cpp',
         'MPlug meshPlug(thisMObject(), aInObject);', 'MDataHandle objectHandle =', 'aInObject'),
    ):
        mesh = (root / path).read_text()
        start = mesh.index(marker)
        end = mesh.index(end_marker, start)
        bodies.append('MStatus ' + name + '() try { MStatus stat; const MObject ' +
                      attribute + '{"' + attribute + '"};' + mesh[start:end] +
                      '++connectionAdmissions; return MS::kSuccess;} '
                      'DOMMY_COMPUTE_CATCH("mesh connections")')
    return '\n\n'.join(bodies)


def connection_sdk(root, sdk):
    code = '''
#include <maya/MDataBlock.h>
#include <maya/MDataHandle.h>
#include <maya/MFnNurbsCurve.h>
#include <maya/MPlug.h>
#include <maya/MObject.h>
#include <maya/MString.h>
#include <maya/MGlobal.h>
#include "common/utility/DStatusOperation.h"
#include "common/utility/DComputeGuard.h"
MObject thisMObject();
MString name();
void EchoWarning(const char*, const char*);
extern int connectionAdmissions;
extern bool preparedTangent;
'''
    stages = connection_stages(root).replace('MStatus spiralConnections()',
                                              'MStatus spiralConnections(MDataBlock& data)')
    stages = stages.replace('MStatus stat; MDataBlock data;', 'MStatus stat;')
    code += re.sub(r'\{"[^\"]+"\}', '{}', stages)
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Spiral/time-shape/edge-direction connection stages: native SDK syntax/analyzer passed')


def run(root, bodies, arguments, capture=False, name='curve_boundary_controls'):
    with tempfile.TemporaryDirectory(prefix='dommy-curve-boundaries-') as directory:
        temp = Path(directory)
        (temp / (name + '.inc')).write_text(bodies)
        binary = temp / name
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-Wno-unused-function', '-Wno-unused-variable',
            '-Wno-unused-parameter', '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temp), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src'), '-I' + str(root / 'src/core'),
            str(root / ('tests/cpp/' + name + '_tests.cpp')), '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' +
                           ('0' if sys.platform == 'darwin' else '1'))
        return subprocess.run([str(binary)] + arguments, check=True, env=environment,
                              capture_output=capture, text=True)


def main():
    root = Path(__file__).resolve().parent.parent
    if '--sdk-connections' in sys.argv:
        connection_sdk(root, sys.argv[sys.argv.index('--sdk-connections') + 1])
        return
    if '--mutations' in sys.argv:
        generated = geometry(root)
        for label, variable, receiver, status in (
            ('native source connection', 'mayaCurveConnected', 'mayaCurvePlug', 'status'),
            ('custom source connection', 'dCurveConnected', 'dCurvePlug', 'status'),
            ('optional tangent connection', 'tangentConnected',
             'MPlug(thisMObject(), aInTangentCurve)', 'status'),
            ('spiral source connection', 'inputConnected', 'inCrvPlug', 'stat'),
            ('spiral tangent connection', 'tangentConnected', 'tangentPlug', 'stat'),
            ('mesh connection', 'meshConnected', 'meshPlug', 'stat'),
            ('edge direction connection', 'meshConnected', 'meshPlug', 'stat'),
        ):
            expression = ('const bool ' + variable + ' = ' + receiver + '.isConnected(&' + status + ');')
            pattern = re.escape(expression) + r'\s*DOMMY_RETURN_IF_FAILURE\(' + status + r'\);'
            offset = generated.index('MStatus edgeDirectionConnections()') if label == 'edge direction connection' else 0
            replacement, count = re.subn(pattern, expression, generated[offset:], count=1)
            mutated = generated[:offset] + replacement
            assert count == 1, label
            try:
                run(root, mutated, [], True, name='curve_boundary_geometry')
            except subprocess.CalledProcessError as error:
                assert Path(error.cmd[0]).name == 'curve_boundary_geometry', 'Compile failure: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)
        return
    run(root, controls(root), [str(root / 'tests/fixtures/curve_boundary_initialization_contract.txt'),
                              str(root / 'tests/fixtures/curve_boundary_controls_contract.txt')])
    run(root, geometry(root), [], name='curve_boundary_geometry')


if __name__ == '__main__':
    main()
