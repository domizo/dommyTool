#!/usr/bin/env python3
"""Exercise all production marker conversion adapters without a Maya runtime."""
import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract

ROOT = Path(__file__).resolve().parent.parent
UTILITY = ROOT / 'src/core/common/utility/DViewportAttributeConversion.h'
MODES = ('ScalarColor', 'ScalarVector', 'Vector3', 'Vector2', 'Vector2XZ',
         'Vector2Color', 'Vector3Color', 'Scalar')
RAW = ('SingleToColorFloat', 'SingleToVectorFloat', 'VectorToVectorFloat',
       'TwoVectorToValueFloat', 'TwoVectorTo3DVectorFloat', 'TwoVectorToColorFloat',
       'VectorToColorFloat', 'ValueToValueFloat')
MARKERS = (
    ('src/modules/curve/DCurve/Display/DCMarker.cpp',
     ('setPrimSingleValueColor', 'setPrim1DVector', 'setPrimSimpleVectorValue',
      'setPrimTwoVectorValue', 'setCurve2To3DVectorValue', 'setPrimTwoVecColor',
      'setPrimThreeVecColor', 'setPrimSimpleSingleValue'),
     ('setPointSingleValueColor', 'setPoint1DVector', 'setPointSimpleVectorValue',
      'setPointTwoVectorValue', 'setCurvePoint2To3DVectorValue', 'setPointTwoVecColor',
      'setPointThreeVecColor', 'setPointSimpleSingleValue')),
    ('src/modules/mesh/DMesh/Display/DMMarker.cpp',
     ('setMeshSingleValueColor', 'setMesh1DVector', 'setMeshSimpleVectorValue',
      'setMeshTwoVectorValue', 'setMesh2To3DVectorValue', 'setMeshTwoVecColor',
      'setMeshThreeVecColor', 'setMeshSimpleSingleValue'),
     ('setMeshPointSingleValueColor', 'setMeshPoint1DVector', 'setMeshPointSimpleVectorValue',
      'setMeshPointTwoVectorValue', 'setMeshPoint2To3DVectorValue', 'setMeshPointTwoVecColor',
      'setMeshPointThreeVecColor', 'setMeshPointSimpleSingleValue')))


def bodies():
    source = (ROOT / 'src/core/common/data/DDataOp.h').read_text()
    result = ['template <typename T>\n' + extract(source, 'void ' + name + '(') for name in RAW]
    for path, primitive, point in MARKERS:
        source = (ROOT / path).read_text()
        result += ['template <typename T>\n' + extract(source, 'inline void ' + name + '(')
                   for name in primitive + point]
    return '\n\n'.join(result)


def cases():
    result = []
    for dimension in (1, 2, 3):
        result.append('template <typename T> void cases' + str(dimension) + '() {')
        for name, mode in zip(RAW, MODES):
            required = 2 if mode.startswith('Vector2') else (3 if mode.startswith('Vector3') else 1)
            if required != dimension:
                continue
            for black in ((False, True) if 'Color' in mode else (False,)):
                arg = ', ' + str(black).lower() if 'Color' in mode else ''
                result.append('testRaw<T>(Mode::' + mode + ', ' + str(black).lower() + ', ' +
                              str(name == 'SingleToColorFloat').lower() +
                              ', [](const std::vector<T>& input, std::vector<float>& output) {' +
                              name + '<T>(input' + arg + ', output);});')
        for _, primitive, point in MARKERS:
            for is_point, names in ((False, primitive), (True, point)):
                for name, mode in zip(names, MODES):
                    required = 2 if mode.startswith('Vector2') else (3 if mode.startswith('Vector3') else 1)
                    if required != dimension:
                        continue
                    if is_point and mode == 'ScalarColor':
                        mode = 'ScalarLuminanceColor'
                    for black in ((False, True) if 'Color' in mode else (False,)):
                        arg = ', ' + str(black).lower() if 'Color' in mode else ''
                        result.append('testMarker<T>(Mode::' + mode + ', ' + str(black).lower() +
                                      ', ' + str(is_point).lower() +
                                      ', [](const FakeOwner& input, std::vector<float>& output) {' +
                                      name + '<T>(input, "attribute"' + arg + ', output);});')
        result.append('}')
    return '\n'.join(result)


def run(utility=None, capture=False):
    with tempfile.TemporaryDirectory(prefix='dommy-viewport-attributes-') as directory:
        temp = Path(directory)
        (temp / 'viewport_attribute_methods.inc').write_text(bodies())
        (temp / 'viewport_attribute_cases.inc').write_text(cases())
        if utility is not None:
            (temp / 'DViewportAttributeConversion.h').write_text(utility)
        binary = temp / 'test'
        command = shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-fsanitize=address,undefined,float-cast-overflow', '-fno-sanitize-recover=all',
            '-fno-omit-frame-pointer', '-I' + str(temp), '-I' + str(ROOT / 'tests/stubs'),
            '-I' + str(ROOT / 'src/core'),
            '-I' + str(ROOT / 'src/core/common/utility'),
            str(ROOT / 'tests/cpp/viewport_attribute_conversion_tests.cpp'), '-o', str(binary)]
        subprocess.run(command, check=True, capture_output=capture, text=True)
        environment = dict(os.environ, ASAN_OPTIONS='detect_leaks=' + ('0' if sys.platform == 'darwin' else '1'))
        subprocess.run([str(binary)], check=True, env=environment, capture_output=capture, text=True)


if __name__ == '__main__':
    if '--mutations' in sys.argv:
        original = UTILITY.read_text()
        compiler = shlex.split(os.environ.get('CXX', 'clang++'))
        macros = subprocess.run(compiler + ['-dM', '-E', '-x', 'c++', '-'], input='',
                                check=True, capture_output=True, text=True).stdout
        double_width_long_double = '#define __LDBL_MANT_DIG__ 53' in macros
        for label, old, new in (
            ('unchecked float conversion',
             '!numeric::checkedScalarCast(Components::get(value, axis), converted[axis])',
             '(converted[axis] = static_cast<float>(Components::get(value, axis)), false)'),
            ('2D display axis changed', 'tuple[2] = converted[1]', 'tuple[1] = converted[1]'),
            ('missing color alpha changed', 'blackTransparent ? 0.0f : 1.0f', '0.0f'),
            ('point field cardinality ignored', 'source.size() != points', 'source.size() < points'),
            ('read-only point field copied',
             'const auto& source = (*it)->template attrListValue<T>(name);',
             'const auto source = (*it)->template attrListValue<T>(name);'),
            ('primitive color repetition removed', 'const bool repeatColor = dimension == 4U;',
             'const bool repeatColor = false;'),
            ('viewport allocation admission removed', 'count > kMaxViewportVertexCount ||', 'false ||'),
            ('native int64 rounding removed',
             'converted[axis] = static_cast<float>(Components::get(value, axis));', ''),
        ):
            if label == 'native int64 rounding removed' and not double_width_long_double:
                print('Skipped native rounding mutation: this host represents int64 exactly in long double.', flush=True)
                continue
            assert old in original, label
            try:
                run(original.replace(old, new, 1), capture=True)
            except subprocess.CalledProcessError as error:
                assert error.returncode != 1 or any(marker in (error.stderr or '') for marker in
                    ('runtime error:', 'ERROR: AddressSanitizer')), 'Compile failure is not detection: ' + label
                print('Detected mutation: ' + label, flush=True)
            else:
                raise AssertionError('Undetected mutation: ' + label)
    else:
        run()
