#!/usr/bin/env python3
"""Exact motion duplicator initialization and input/geometry admission.

Native copying, historical DG evaluation and rendering remain host tests.
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_point_utility_safety import declarations
from check_curve_path_safety import run

SOURCE = 'src/modules/generators/Generate/motionDuplicator.cpp'


def timing(root):
    return extract((root / SOURCE.replace('.cpp', '.h')).read_text(), 'enum {') + ';'


def initializer(root):
    source = (root / SOURCE).read_text()
    return '\n'.join([timing(root)] + declarations('DTMotionDuplicator', source, True) +
                     [extract(source, 'MStatus DTMotionDuplicator::initialize(')])


def admission(root):
    source = (root / SOURCE).read_text()
    values = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    budget = (root / 'src/core/common/utility/DMotionGenerationPlan.h').read_text()
    result = [extract(budget, 'namespace dommy {'), timing(root), 'namespace dommy {namespace maya {',
              extract(values, 'inline bool isFiniteMatrix('),
              extract(values, 'inline bool isFinitePoint('), '}}']
    result += [extract(source, 'enum MotionGeometryKind') + ';']
    result += [extract(source, signature) for signature in (
        'bool validTimingType(', 'MStatus queryMotionMeshSize(',
        'MStatus queryMotionCurveSize(', 'MStatus queryMotionSurfaceSize(',
        'MStatus readFloat3(', 'MStatus readDouble3(')]
    result += declarations('DTMotionDuplicator', source, False)
    start = source.index('MStatus DTMotionDuplicator::compute(')
    end = source.index('    const std::int32_t signedSeed =', start)
    prefix = source[start:end]
    assert prefix.index('buildMotionDuplicationBudget(') < prefix.index('inputMesh.getPoints(')
    assert prefix.index('buildMotionDuplicationBudget(') < prefix.index('inputCurve.getCVs(')
    assert prefix.index('buildMotionDuplicationBudget(') < prefix.index('inputSurface.getCVs(')
    result += [prefix + '++admissions; admittedBytes=budget.estimatedBytes; return MS::kSuccess; }\n'
               'DOMMY_COMPUTE_CATCH("DTMotionDuplicator")']
    return '\n'.join(result)


def sdk_check(root, sdk, source_path=SOURCE, label='DTMotionDuplicator', system_includes=()):
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')]
    for include in system_includes:
        command += ['-isystem', str(include)]
    command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
    command += ['-isystem', str(root / 'third_party/libnoise/source'),
                '-idirafter', str(root / 'tests/stubs')]
    source = str(root / source_path)
    subprocess.run(command + ['-fsyntax-only', source], check=True)
    subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
        '-Xanalyzer', '-analyzer-werror', '-o', os.devnull, source], check=True)
    print(label + ': complete Maya SDK translation unit syntax/analysis passed')


def mutations(root):
    initial = initializer(root)
    body = admission(root)
    cases = [
        ('initializer setter failure ignored', 'motion_duplicator_init', initial,
         'DOMMY_RETURN_IF_FAILURE(nAttr.setMin( 0 ));', '(void)nAttr.setMin( 0 );'),
        ('initializer create status overwritten', 'motion_duplicator_init', initial,
         'DOMMY_RETURN_IF_FAILURE(stat);', ''),
        ('initializer exception escapes', 'motion_duplicator_init', initial,
         'return dommy::maya::reportEntryPointException(',
         'throw; return dommy::maya::reportEntryPointException('),
        ('failed time handle read', 'motion_duplicator_admission', body,
         'CHECK_MSTATUS_AND_RETURN_IT(status);\n    const MTime evaluationTime',
         'const MTime evaluationTime'),
        ('failed matrix handle read', 'motion_duplicator_admission', body,
         'CHECK_MSTATUS_AND_RETURN_IT(status);\n    const MMatrix baseMatrix',
         'const MMatrix baseMatrix'),
        ('null triple dereferenced', 'motion_duplicator_admission', body,
         'if (!raw) return MS::kFailure;', ''),
        ('mesh count status ignored', 'motion_duplicator_admission', body,
         'mesh.numVertices(&status);\n    DOMMY_RETURN_IF_FAILURE(status);',
         'mesh.numVertices(&status);'),
        ('set name cardinality ignored', 'motion_duplicator_admission', body,
         'if (names.length() != expected) return MS::kFailure;', ''),
        ('non-current UV sets ignored', 'motion_duplicator_admission', body,
         'i < names.length()', 'i < 1U'),
        ('UV payload omitted from storage', 'motion_duplicator_admission', body,
         'source.uvs, source.normals', '0U, source.normals'),
        ('normal payload omitted from storage', 'motion_duplicator_admission', body,
         'source.normals, source.colors', '0U, source.colors'),
        ('color payload omitted from storage', 'motion_duplicator_admission', body,
         'source.colors, source.knots', '0U, source.knots'),
        ('histories omitted from storage', 'motion_duplicator_admission', body,
         'candidate.objectCount, 1024U,', 'candidate.objectCount, 0U,'),
        ('storage ceiling raised', 'motion_duplicator_admission', body,
         'kMaxMotionDuplicationBytes = 256U * 1024U * 1024U',
         'kMaxMotionDuplicationBytes = 512U * 1024U * 1024U'),
        ('surface CV dimensions added', 'motion_duplicator_admission', body,
         'checkedProduct(u, v, kMaxGeneratedPointCount, candidate.points)',
         'checkedSum(u, v, kMaxGeneratedPointCount, candidate.points)'),
        ('returned point count ignored', 'motion_duplicator_admission', body,
         'if (sourcePoints.length() != sourceSize.points) return MS::kFailure;', ''),
    ]
    for label, suite, original, old, new in cases:
        assert old in original, label
        try:
            run(root, original.replace(old, new, 1), capture=True, suite=suite,
                arguments=None if suite.endswith('_init') else [])
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == suite + '_tests', \
                'Compile failure is not detection: ' + label
            print('Detected mutation: ' + label)
        else:
            raise AssertionError('Undetected mutation: ' + label)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if '--emit' in sys.argv:
        result = run(root, initializer(root), capture=True, suite='motion_duplicator_init', arguments=['--emit'])
        print(result.stdout, end='')
    elif '--sdk' in sys.argv:
        sdk_check(root, sys.argv[sys.argv.index('--sdk') + 1])
    elif '--mutations' in sys.argv:
        mutations(root)
    else:
        run(root, initializer(root), suite='motion_duplicator_init')
        run(root, admission(root), suite='motion_duplicator_admission', arguments=[])
