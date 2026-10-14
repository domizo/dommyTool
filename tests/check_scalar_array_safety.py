#!/usr/bin/env python3
"""Execute scalar-array node methods and the real timing helper with host doubles.

This checks control flow, numeric results and failure handling, not Maya's DG,
array ownership or plug registration implementation.
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

SOURCES = {
    'DPIntArrayGen': 'src/core/DDataUtility/Data/DPIntArrayGen.cpp',
    'DPDoubleArrayGen': 'src/modules/point/DPoint/generate/data/DPDoubleArrayGen.cpp',
}


def generated_methods(root, sources=None, timing=None):
    if sources is None:
        sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    if timing is None:
        timing = (root / 'src/core/common/utility/DTimeSampling.h').read_text()
    bodies = ['namespace dommy { namespace maya { namespace detail {\n' +
              extract(timing, 'inline MStatus copyScalarInput(') + '\n}\n' +
              extract(timing, 'inline MStatus readTimeOffsetAndStretch(') + '\n' +
              extract(timing, 'inline bool computeSampleTime(') + '\n} }']
    expected = ['generateType', 'value', 'valueSize', 'aSeed', 'minRange', 'maxRange',
                'aShiftRange', 'aValueRamp', 'aRampRange', 'aGlobalOffset',
                'aInOffsetArray', 'aInStretchArray', 'outDataArray']
    for name, source in sources.items():
        attributes = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        if attributes != expected:
            raise ValueError(name + ' attributes changed')
        bodies.extend('MObject ' + name + '::' + attribute + ';' for attribute in attributes)
        bodies.append(extract(source, 'MStatus ' + name + '::initialize('))
        bodies.append(extract(source, 'MStatus ' + name + '::compute(') +
                      '\nDOMMY_COMPUTE_CATCH("' + name + '")')
    return '\n\n'.join(bodies)


def check(sources=None, timing=None, capture=False):
    root = Path(__file__).resolve().parent.parent
    with tempfile.TemporaryDirectory(prefix='dommy-scalar-arrays-') as directory:
        temporary = Path(directory)
        (temporary / 'scalar_array_methods.inc').write_text(generated_methods(root, sources, timing))
        binary = temporary / 'scalar_array_safety_tests'
        subprocess.run(shlex.split(os.environ.get('CXX', 'clang++')) + [
            '-std=c++11', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
            '-Wno-unknown-pragmas', '-fsanitize=address,undefined,float-cast-overflow',
            '-fno-sanitize-recover=all', '-fno-omit-frame-pointer',
            '-I' + str(temporary), '-I' + str(root / 'tests/stubs'),
            '-I' + str(root / 'src/core'), str(root / 'tests/cpp/scalar_array_safety_tests.cpp'),
            '-o', str(binary)], check=True)
        environment = os.environ.copy()
        environment['ASAN_OPTIONS'] = 'detect_leaks=' + ('0' if sys.platform == 'darwin' else '1')
        return subprocess.run([str(binary), str(root / 'tests/fixtures/scalar_array_initialization_contract.txt')],
                              check=True, env=environment, capture_output=capture, text=True)


def mutations():
    root = Path(__file__).resolve().parent.parent
    timing = (root / 'src/core/common/utility/DTimeSampling.h').read_text()
    sources = {name: (root / path).read_text() for name, path in SOURCES.items()}
    cases = [('offset/stretch connection failure ignored', None, timing.replace(
        'if (!status) return status;\n    if (!connected)', 'if (!connected)'))]
    for name, source in sources.items():
        cases.append((name + ' value connection failure ignored', name, source.replace(
            'if (!status) return status;\n        if (!connected', 'if (!connected')))
        for label, old, new in (
            ('ramp maximum bypass removed', 'rampValue == 1.0f ? maximum : ', ''),
            ('ramp minimum endpoint corrupted', 'rampValue == 0.0f ? minimum :',
             'rampValue == 0.0f ? maximum :'),
            ('ramp extrapolation clamped', 'rangeSize * rampValue',
             'rangeSize * std::max(0.0f, std::min(1.0f, rampValue))'),
        ):
            assert source.count(old) == 1, name + ' ' + label
            cases.append((name + ' ' + label, name, source.replace(old, new, 1)))
        cases.append((name + ' ramp pinned to array position', name, source.replace(
            'rampValue == 0.0f ? minimum :', 'position == 0.0f ? minimum :').replace(
            'rampValue == 1.0f ? maximum :', 'position == 1.0f ? maximum :')))
    name = 'DPIntArrayGen'
    source = sources[name]
    truncation = 'const double generated = std::trunc(\n'
    assert source.count(truncation) == 1
    cases.append(('integer ramp truncation changed to floor', name, source.replace(
        truncation, 'const double generated = std::floor(\n', 1)))
    name = 'DPDoubleArrayGen'
    source = sources[name]
    start = source.index('        } else if (valueCount > 1U)')
    end = source.index('        break;', start)
    old_loop = '''        } else if (valueCount > 1U) {
            const double step =
                (static_cast<double>(maximum) - minimum) / static_cast<double>(valueCount - 1U);
            for (std::size_t index = 0U; index < valueCount; ++index) {
                const double generated = minimum + step * index;
                if (!std::isfinite(generated)) return MS::kFailure;
                values[static_cast<unsigned int>(index)] = static_cast<float>(generated);
            }
        }
'''
    cases.append(('historical unpinned/unchecked float shift restored', name,
                  source[:start] + old_loop + source[end:]))
    for label, old, new in (
        ('last float endpoint corrupted', 'output = maximum;', 'output = minimum;'),
        ('ordinary interior rounding formula changed', 'minimum + step * index;',
         'minimum + step * (index + 1U);'),
        ('float shift minimum ordering reversed',
         'const float minimum = std::min(range[0], range[1]);',
         'const float minimum = std::max(range[0], range[1]);'),
    ):
        assert source.count(old) == 1, label
        cases.append((label, name, source.replace(old, new, 1)))

    # Deliberately perturb one intermediate in the extracted node, not the
    # numeric helper. The real fallback must recover the original bounded value.
    generated = 'const double generated = minimum + step * index;'
    perturbed = source.replace(generated, '''const double generated =
                    minimum == -std::numeric_limits<float>::max() &&
                    maximum == std::numeric_limits<float>::max() &&
                    valueCount == 28U && index == 1U
                    ? std::nextafter(double(maximum), INFINITY)
                    : minimum + step * index;''', 1)
    assert perturbed != source
    check(dict(sources, **{name: perturbed}))
    print('Injected out-of-range float intermediate: real interpolation recovery passed', flush=True)
    arguments = 'minimum, maximum, position, output))'
    assert perturbed.count(arguments) == 1
    cases.append(('float interpolation recovery disabled', name, perturbed.replace(
        arguments, 'minimum, maximum, position, output) || position >= 0.0)')))
    cases.append(('float interpolation recovery weight corrupted', name, perturbed.replace(
        arguments, 'minimum, maximum, position * 0.0, output))')))
    for label, name, altered in cases:
        assert altered != (timing if name is None else sources[name]), label
        try:
            check(sources if name is None else dict(sources, **{name: altered}),
                  altered if name is None else timing, capture=True)
        except subprocess.CalledProcessError as error:
            assert Path(error.cmd[0]).name == 'scalar_array_safety_tests', 'Compile failure: ' + label
            assert 'Assertion' in error.stderr or 'runtime error:' in error.stderr, label
            print('Detected mutation: ' + label, flush=True)
        else:
            raise AssertionError('Undetected mutation: ' + label)


def sdk_boundary(sdk):
    root = Path(__file__).resolve().parent.parent
    code = '#include "common/utility/DTimeSampling.h"\n'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-isystem', str(Path(sdk) / 'include'),
        '-I' + str(root / 'src/core'), '-x', 'c++', '-']
    for flags in (['-fsyntax-only'], ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                    '-Xanalyzer', '-analyzer-werror', '-o', os.devnull]):
        subprocess.run(command + flags, input=code, text=True, check=True)
    print('Production time sampling header: native Maya API syntax/analysis passed; no host runtime')


def sdk_nodes(sdk):
    sdk_boundary(sdk)
    root = Path(__file__).resolve().parent.parent
    includes = ['-I' + str(directory) for directory, _, _ in os.walk(root / 'src')]
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unknown-pragmas',
        '-isystem', str(Path(sdk) / 'include')] + includes + [
        '-isystem', str(root / 'third_party/libnoise/source'),
        '-idirafter', str(root / 'tests/stubs')]
    for path in SOURCES.values():
        subprocess.run(command + ['-fsyntax-only', str(root / path)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                 '-Xanalyzer', '-analyzer-werror', '-o', os.devnull,
                                 str(root / path)], check=True)
    print('DPIntArrayGen and DPDoubleArrayGen: complete SDK syntax/analysis passed; no host runtime')


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_nodes(sys.argv[2])
    elif len(sys.argv) == 3 and sys.argv[1] == '--sdk-boundary':
        sdk_boundary(sys.argv[2])
    elif sys.argv[1:] == ['--mutations']:
        mutations()
    else:
        assert not sys.argv[1:]
        check()
