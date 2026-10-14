#!/usr/bin/env python3
"""Frozen mesh-noise initialization schemas and injected API failures."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from check_geometry_state_transactions import extract
from check_curve_path_safety import run

NAMES = ('DMNoise', 'DMFastNoise')
FOLDER = 'src/modules/mesh/DMesh/Utility/'


def bodies(root, base='AttributeNode'):
    header = (root / 'src/core/common/object/DNoiseOp.h').read_text()
    result = []
    for alias in ('NoiseBaseType', 'NoiseDeformType', 'FastNoiseType', 'FNInterpType',
                  'FNFractalType', 'FNCellularDistanceType', 'FNCellularReturnType'):
        match = re.search(r'enum\s*\{[^}]*\};typedef short ' + alias + ';', header)
        assert match, alias
        result.append(match.group())
    for name in NAMES:
        source = (root / (FOLDER + name + '.cpp')).read_text()
        attrs = re.findall(r'^MObject\s+' + name + r'::(\w+);', source, re.M)
        result.append('struct ' + name + ':' + base + '{static MStatus initialize();' +
                      ''.join('static MObject ' + a + ';' for a in attrs) + '};')
        result.extend('MObject ' + name + '::' + a + ';' for a in attrs)
        result.append(extract(source, 'MStatus ' + name + '::initialize('))
    return '\n\n'.join(result)


def sdk_check(root, sdk):
    # Real Maya signatures for both initializers, without requiring FastNoise
    # runtime code. This deliberately does not claim a complete optional unit.
    includes = ('MPxNode', 'MFnNumericAttribute', 'MFnEnumAttribute', 'MFnTypedAttribute',
                'MFnStringData', 'MTypeId', 'MObject')
    preamble = '\n'.join('#include <maya/' + name + '.h>' for name in includes)
    preamble += '\n#include "common/utility/DComputeGuard.h"\n#include "common/utility/DStatusOperation.h"\n'
    preamble += 'struct DMeshData {static const MTypeId id;};const MTypeId DMeshData::id(0x0012);\n'
    command = shlex.split(os.environ.get('CXX', 'clang++')) + [
        '-std=c++14', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
        '-Wno-deprecated-declarations', '-Wno-unused-parameter',
        '-isystem', str(Path(sdk) / 'include'), '-I' + str(root / 'src/core')]
    with tempfile.TemporaryDirectory(prefix='dommy-noise-initialization-') as directory:
        source = Path(directory) / 'initialization.cpp'
        source.write_text(preamble + bodies(root, 'MPxNode'))
        subprocess.run(command + ['-fsyntax-only', str(source)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)], check=True)
    print('Mesh noise: both complete initializer bodies passed real Maya SDK syntax/analysis')
    dependency = Path(os.environ.get('DOMMY_LEGACY_DEPENDENCY_ROOT',
                      str(Path(os.environ.get('TMPDIR', '/tmp')) / 'dommytool-legacy-dependencies')))
    seexpr = dependency / 'SeExpr-a5f02bb03199630759b0b94a64f37ce56c08675a/src/SeExpr'
    if (seexpr / 'SeExpression.h').is_file():
        command += ['-Wno-unknown-pragmas', '-isystem', str(seexpr)]
        command += ['-I' + str(path) for path, _, _ in os.walk(root / 'src')]
        command += ['-isystem', str(root / 'third_party/libnoise/source'),
                    '-idirafter', str(root / 'tests/stubs')]
        source = root / (FOLDER + 'DMNoise.cpp')
        subprocess.run(command + ['-fsyntax-only', str(source)], check=True)
        subprocess.run(command + ['--analyze', '-Xanalyzer', '-analyzer-output=text',
                                  '-Xanalyzer', '-analyzer-werror', '-o', '/dev/null', str(source)], check=True)
        print('DMNoise: complete SDK unit passed syntax/analysis (limited Imath test double)')
    else:
        print('SKIP complete DMNoise SDK unit: pinned SeExpr headers unavailable')
    print('Optional FastNoise algorithm/full unit is not covered by this check')


def mutations(root):
    source = bodies(root)
    cases = [
        ('ignored create status', source.replace('DOMMY_RETURN_IF_FAILURE(stat);', 'CHECK_MSTATUS(stat);', 1)),
        ('ignored setter status', source.replace('DOMMY_RETURN_IF_FAILURE(tAttr.setReadable(false));',
                                                'tAttr.setReadable(false);', 1)),
        ('uncaught initializer exception', source.replace('return dommy::maya::reportEntryPointException(',
                                                         'throw; return dommy::maya::reportEntryPointException(', 1)),
    ]
    for name, modified in cases:
        assert modified != source, name
        try:
            run(root, modified, capture=True, suite='mesh_noise_init')
        except subprocess.CalledProcessError as error:
            assert error.returncode != 1 or 'ERROR: AddressSanitizer' in (error.stderr or ''), name
            print('Detected mutation: ' + name)
        else:
            raise AssertionError('Undetected mutation: ' + name)


if __name__ == '__main__':
    root = Path(__file__).resolve().parent.parent
    if len(sys.argv) == 3 and sys.argv[1] == '--sdk':
        sdk_check(root, sys.argv[2])
    elif len(sys.argv) == 2 and sys.argv[1] == '--mutations':
        mutations(root)
    else:
        run(root, bodies(root), suite='mesh_noise_init')
