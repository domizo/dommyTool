#!/usr/bin/env python3
"""Execute production curvature methods and two initializers with host doubles.

These tests cover control flow, array boundaries and legacy arithmetic, not
Maya ramp interpolation, mesh ownership or DG evaluation.
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
from check_mesh_color_safety import color_output_body

SOURCES = {
    'DCurvatureData': 'src/core/UtilityNode/DCurvatureData/DCurvatureData.cpp',
    'DPolyFaceInfo': 'src/core/UtilityNode/DPolyFaceInfo/DPolyFaceInfo.cpp',
}


def generated_methods(root, sources=None):
    if sources is None:
        sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    expected = {
        'DCurvatureData': ['inMesh', 'aCalcType', 'aCurveMap', 'smIterate',
                           'aIsAdjustRange', 'outMesh'],
        'DPolyFaceInfo': ['inMesh', 'faceIndex', 'outTranslate', 'outRotate'],
    }
    validation = (root / 'src/core/common/utility/DMayaValueValidation.h').read_text()
    bodies = ['namespace dommy { namespace maya {\n' + '\n'.join(
        extract(validation, 'inline bool ' + method + '(') for method in (
            'isFinitePoint', 'isFiniteVector', 'isFloatRepresentableVector', 'normalizedVector'))
        + '\n} }']
    bodies += [color_output_body(root), 'namespace dommy { namespace maya {',
               extract((root / 'src/core/common/utility/DMayaMeshTraversal.h').read_text(),
                       'template <typename Iterator, typename Visitor>'), '}}']
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected[name]:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attribute + ';' for attribute in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
    curvature = sources['DCurvatureData']
    bodies.append(re.search(r'const double kRadiansToDegrees\s*=\s*[^;]+;', curvature).group(0))
    for signature in ('MStatus setOutputMesh(',):
        bodies.append(extract(curvature, signature))
    if 'MStatus initializeDefaultColorRamp(' in curvature:
        bodies.append(extract(curvature, 'MStatus initializeDefaultColorRamp('))
    bodies.append(extract(curvature, 'void DCurvatureData::postConstructor('))
    bodies.append(extract(curvature, 'MStatus DCurvatureData::compute(') +
                  '\nDOMMY_COMPUTE_CATCH("DCurvatureData")')
    return '\n\n'.join(bodies)


def main():
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-curvature-face-') as directory:
        temporary = Path(directory)
        (temporary / 'curvature_face_methods.inc').write_text(generated_methods(root))
        binary = temporary / 'curvature_face_safety_tests'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/curvature_face_safety_tests.cpp'),
            '-o', str(binary)]
        subprocess.run(command, check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        subprocess.run([str(binary), str(root / 'tests/fixtures/curvature_face_initialization_contract.txt')],
                       check=True, env=environment)


if __name__ == '__main__':
    main()
